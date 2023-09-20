/*=====================================================================
ImageMap.cpp
------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "ImageMap.h"


#include "../utils/OutStream.h"
#include "../utils/InStream.h"
#include "../utils/Exception.h"
#include "../utils/StringUtils.h"
#include "../utils/TaskManager.h"
#include "../utils/Task.h"
#if OPENEXR_SUPPORT
#include "../utils/IncludeHalf.h"
#endif


// Explicit template instantiation
template void writeToStream(const ImageMap<float, FloatComponentValueTraits>& im, OutStream& stream);
template void readFromStream(InStream& stream, ImageMap<float, FloatComponentValueTraits>& image);

#if MAP2D_FILTERING_SUPPORT
#if OPENEXR_SUPPORT
template Reference<Map2D> ImageMap<half,   HalfComponentValueTraits>  ::resizeMidQuality(const int new_width, const int new_height, glare::TaskManager* task_manager) const;
#endif
template Reference<Map2D> ImageMap<float,  FloatComponentValueTraits> ::resizeMidQuality(const int new_width, const int new_height, glare::TaskManager* task_manager) const;
template Reference<Map2D> ImageMap<uint8,  UInt8ComponentValueTraits> ::resizeMidQuality(const int new_width, const int new_height, glare::TaskManager* task_manager) const;
template Reference<Map2D> ImageMap<uint16, UInt16ComponentValueTraits>::resizeMidQuality(const int new_width, const int new_height, glare::TaskManager* task_manager) const;

template void ImageMap<float, FloatComponentValueTraits>::downsampleImage(const size_t factor, const size_t border_width, const size_t filter_span,
	const float * const resize_filter, const float pre_clamp, const ImageMap<float, FloatComponentValueTraits>& img_in, 
	ImageMap<float, FloatComponentValueTraits>& img_out, glare::TaskManager& task_manager);

#endif // MAP2D_FILTERING_SUPPORT

template double ImageMap<float, FloatComponentValueTraits>::averageLuminance() const;

template void ImageMap<float, FloatComponentValueTraits>::blendImage(const ImageMap<float, FloatComponentValueTraits>& img, const int destx, const int desty, const Colour4f& colour);



static const uint32 IMAGEMAP_SERIALISATION_VERSION = 1;


template <class V, class VTraits>
void writeToStream(const ImageMap<V, VTraits>& im, OutStream& stream)
{
	stream.writeUInt32(IMAGEMAP_SERIALISATION_VERSION);
	stream.writeUInt32((uint32)im.getWidth());
	stream.writeUInt32((uint32)im.getHeight());
	stream.writeUInt32((uint32)im.getN());
	stream.writeData(im.getData(), (size_t)im.getWidth() * (size_t)im.getHeight() * (size_t)im.getN() * sizeof(V));
}


template <class V, class VTraits>
void readFromStream(InStream& stream, ImageMap<V, VTraits>& image)
{
	const uint32 v = stream.readUInt32();
	if(v != IMAGEMAP_SERIALISATION_VERSION)
		throw glare::Exception("Unknown version " + toString(v) + ", expected " + toString(IMAGEMAP_SERIALISATION_VERSION) + ".");

	const uint32 w = stream.readUInt32();
	const uint32 h = stream.readUInt32();
	const uint32 N = stream.readUInt32();

	// TODO: handle max image size

	image.resize(w, h, N);
	stream.readData((void*)image.getData(), (size_t)w * (size_t)h * (size_t)N * sizeof(V));
}


#if MAP2D_FILTERING_SUPPORT

template <class V, class VTraits>
class ResizeMidQualityTask : public glare::Task
{
public:
	virtual void run(size_t thread_index)
	{
		ImageMap<V, VTraits>* const image = this->image_out;
		const ImageMap<V, VTraits>* const src_image = this->image_in;

		// For this implementation we will use a tent (bilinear) filter, with normalisation.
		// Tent filter gives reasonable resulting image quality, is separable (and therefore fast), and has a small support.
		// We need to do normalisation however, to avoid banding/spotting artifacts when resizing uniform images with the tent filter.
		// Note that if we use a higher quality filter like Mitchell Netravali, we can avoid the renormalisation.
		// But M.N. has a support radius twice as large (2 px), so we'll stick with the fast tent filter.

		const int new_width  = (int)image->getWidth();
		const int new_height = (int)image->getHeight();
		const int src_w = (int)src_image->getMapWidth();
		const int src_h = (int)src_image->getMapHeight();
		const int src_N = (int)src_image->getN();
		const float scale_factor_x = (float)src_w / new_width;
		const float scale_factor_y = (float)src_h / new_height;
		const float filter_r_x = myMax(1.f, scale_factor_x);
		const float filter_r_y = myMax(1.f, scale_factor_y);
		const float recip_filter_r_x = 1 / filter_r_x;
		const float recip_filter_r_y = 1 / filter_r_y;
		const float filter_r_plus_1_x  = filter_r_x + 1.f;
		const float filter_r_plus_1_y  = filter_r_y + 1.f;
		const float filter_r_minus_1_x = filter_r_x - 1.f;
		const float filter_r_minus_1_y = filter_r_y - 1.f;

		if(!src_image->channel_names.empty() && ::hasPrefix(src_image->channel_names[0], "wavelength")) // If this is a spectral image map, we want to resample all channels, not just the first 3.
		{
			std::vector<float> sum(src_N);

			for(int y = begin_y; y < end_y; ++y)
				for(int x = 0; x < new_width; ++x)
				{
					const float src_x = x * scale_factor_x;
					const float src_y = y * scale_factor_y;

					const int src_begin_x = myMax(0, (int)(src_x - filter_r_minus_1_x));
					const int src_end_x   = myMin(src_w, (int)(src_x + filter_r_plus_1_x));
					const int src_begin_y = myMax(0, (int)(src_y - filter_r_minus_1_y));
					const int src_end_y   = myMin(src_h, (int)(src_y + filter_r_plus_1_y));

					for(int c=0; c<src_N; ++c)
						sum[c] = 0.f;
					float filter_sum = 0.f;

					for(int sy = src_begin_y; sy < src_end_y; ++sy)
						for(int sx = src_begin_x; sx < src_end_x; ++sx)
						{
							const float dx = (float)sx - src_x;
							const float dy = (float)sy - src_y;
							const float fabs_dx = std::fabs(dx);
							const float fabs_dy = std::fabs(dy);
							const float filter_val = myMax(1 - fabs_dx * recip_filter_r_x, 0.f) * myMax(1 - fabs_dy * recip_filter_r_y, 0.f);
							
							for(int c=0; c<src_N; ++c)
								sum[c] += (float)src_image->getPixel(sx, sy)[c] * filter_val;

							filter_sum += filter_val;
						}

					const float scale = 1.f / filter_sum;

					for(int c=0; c<src_N; ++c)
						image->getPixel(x, y)[c] = (V)(sum[c] * scale); // Normalise
				}
		}
		else if(src_N == 4)
		{
			for(int y = begin_y; y < end_y; ++y)
				for(int x = 0; x < new_width; ++x)
				{
					const float src_x = x * scale_factor_x;
					const float src_y = y * scale_factor_y;

					const int src_begin_x = myMax(0, (int)(src_x - filter_r_minus_1_x));
					const int src_end_x   = myMin(src_w, (int)(src_x + filter_r_plus_1_x));
					const int src_begin_y = myMax(0, (int)(src_y - filter_r_minus_1_y));
					const int src_end_y   = myMin(src_h, (int)(src_y + filter_r_plus_1_y));

					Colour4f sum(0.f);
					float filter_sum = 0.f;
					for(int sy = src_begin_y; sy < src_end_y; ++sy)
						for(int sx = src_begin_x; sx < src_end_x; ++sx)
						{
							const float dx = (float)sx - src_x;
							const float dy = (float)sy - src_y;
							const float fabs_dx = std::fabs(dx);
							const float fabs_dy = std::fabs(dy);
							const float filter_val = myMax(1 - fabs_dx * recip_filter_r_x, 0.f) * myMax(1 - fabs_dy * recip_filter_r_y, 0.f);
							Colour4f px_col(
								(float)src_image->getPixel(sx, sy)[0],
								(float)src_image->getPixel(sx, sy)[1],
								(float)src_image->getPixel(sx, sy)[2],
								(float)src_image->getPixel(sx, sy)[3]
							);

							sum += px_col * filter_val;
							filter_sum += filter_val;
						}

					const Colour4f col = sum * (1.f / filter_sum); // Normalise
					image->getPixel(x, y)[0] = (V)col[0];
					image->getPixel(x, y)[1] = (V)col[1];
					image->getPixel(x, y)[2] = (V)col[2];
					image->getPixel(x, y)[3] = (V)col[3];
				}
		}
		else if(src_N == 3)
		{
			for(int y = begin_y; y < end_y; ++y)
				for(int x = 0; x < new_width; ++x)
				{
					const float src_x = x * scale_factor_x;
					const float src_y = y * scale_factor_y;

					const int src_begin_x = myMax(0, (int)(src_x - filter_r_minus_1_x));
					const int src_end_x   = myMin(src_w, (int)(src_x + filter_r_plus_1_x));
					const int src_begin_y = myMax(0, (int)(src_y - filter_r_minus_1_y));
					const int src_end_y   = myMin(src_h, (int)(src_y + filter_r_plus_1_y));

					Colour4f sum(0.f);
					for(int sy = src_begin_y; sy < src_end_y; ++sy)
						for(int sx = src_begin_x; sx < src_end_x; ++sx)
						{
							const float dx = (float)sx - src_x;
							const float dy = (float)sy - src_y;
							const float fabs_dx = std::fabs(dx);
							const float fabs_dy = std::fabs(dy);
							const float filter_val = myMax(1 - fabs_dx * recip_filter_r_x, 0.f) * myMax(1 - fabs_dy * recip_filter_r_y, 0.f);
							Colour4f px_col(
								(float)src_image->getPixel(sx, sy)[0],
								(float)src_image->getPixel(sx, sy)[1],
								(float)src_image->getPixel(sx, sy)[2],
								1.f
							);

							sum += px_col * filter_val;
						}

					const Colour4f col = sum * (1.f / sum[3]); // Normalise
					image->getPixel(x, y)[0] = (V)col[0];
					image->getPixel(x, y)[1] = (V)col[1];
					image->getPixel(x, y)[2] = (V)col[2];
				}
		}
		else if(src_N == 2)
		{
			for(int y = begin_y; y < end_y; ++y)
				for(int x = 0; x < new_width; ++x)
				{
					const float src_x = x * scale_factor_x;
					const float src_y = y * scale_factor_y;

					const int src_begin_x = myMax(0, (int)(src_x - filter_r_minus_1_x));
					const int src_end_x   = myMin(src_w, (int)(src_x + filter_r_plus_1_x));
					const int src_begin_y = myMax(0, (int)(src_y - filter_r_minus_1_y));
					const int src_end_y   = myMin(src_h, (int)(src_y + filter_r_plus_1_y));

					Colour4f sum(0.f);
					for(int sy = src_begin_y; sy < src_end_y; ++sy)
						for(int sx = src_begin_x; sx < src_end_x; ++sx)
						{
							const float dx = (float)sx - src_x;
							const float dy = (float)sy - src_y;
							const float fabs_dx = std::fabs(dx);
							const float fabs_dy = std::fabs(dy);
							const float filter_val = myMax(1 - fabs_dx * recip_filter_r_x, 0.f) * myMax(1 - fabs_dy * recip_filter_r_y, 0.f);
							Colour4f px_col(
								(float)src_image->getPixel(sx, sy)[0],
								(float)src_image->getPixel(sx, sy)[1],
								1.f,
								1.f
							);

							sum += px_col * filter_val;
						}

					const Colour4f col = sum * (1.f / sum[3]); // Normalise
					image->getPixel(x, y)[0] = (V)col[0];
					image->getPixel(x, y)[1] = (V)col[1];
				}
		}
		else
		{
			assert(src_N == 1);

			for(int y = begin_y; y < end_y; ++y)
				for(int x = 0; x < new_width; ++x)
				{
					const float src_x = x * scale_factor_x;
					const float src_y = y * scale_factor_y;

					const int src_begin_x = myMax(0, (int)(src_x - filter_r_minus_1_x));
					const int src_end_x   = myMin(src_w, (int)(src_x + filter_r_plus_1_x));
					const int src_begin_y = myMax(0, (int)(src_y - filter_r_minus_1_y));
					const int src_end_y   = myMin(src_h, (int)(src_y + filter_r_plus_1_y));

					Colour4f sum(0.f);
					for(int sy = src_begin_y; sy < src_end_y; ++sy)
						for(int sx = src_begin_x; sx < src_end_x; ++sx)
						{
							const float dx = (float)sx - src_x;
							const float dy = (float)sy - src_y;
							const float fabs_dx = std::fabs(dx);
							const float fabs_dy = std::fabs(dy);
							const float filter_val = myMax(1 - fabs_dx * recip_filter_r_x, 0.f) * myMax(1 - fabs_dy * recip_filter_r_y, 0.f);
							Colour4f px_col(
								(float)src_image->getPixel(sx, sy)[0],
								1.f,
								1.f,
								1.f
							);

							sum += px_col * filter_val;
						}

					const Colour4f col = sum * (1.f / sum[3]); // Normalise
					image->getPixel(x, y)[0] = (V)col[0];
				}
		}
	}

	int begin_y, end_y;
	const ImageMap<V, VTraits>* image_in;
	ImageMap<V, VTraits>* image_out;
};


template <class V, class VTraits>
Reference<Map2D> ImageMap<V, VTraits>::resizeMidQuality(const int new_width, const int new_height, glare::TaskManager* task_manager) const
{
	ImageMap<V, VTraits>* new_image = new ImageMap<V, VTraits>(new_width, new_height, this->getN());

	const bool spectral = !this->channel_names.empty() && ::hasPrefix(this->channel_names[0], "wavelength"); // If this is a spectral image:
	if(this->getN() > 4 && !spectral)
		throw glare::Exception("Invalid num channels for resizing: " + toString(this->getN()));

	new_image->channel_names = this->channel_names;

	if(task_manager)
	{
		const int num_tasks = myClamp<int>((int)task_manager->getNumThreads(), 1, new_height); // We want at least one task, but no more than the number of rows in the new image.
		const int y_step = Maths::roundedUpDivide(new_height, num_tasks);
		for(int z=0, begin_y=0; z<(int)task_manager->getNumThreads(); ++z, begin_y += y_step)
		{
			Reference<ResizeMidQualityTask<V, VTraits> > task = new ResizeMidQualityTask<V, VTraits>();
			task->begin_y = myMin(begin_y,          new_height);
			task->end_y   = myMin(begin_y + y_step, new_height);
			task->image_in = this;
			task->image_out = new_image;
			task_manager->addTask(task);
		}

		task_manager->waitForTasksToComplete();
	}
	else
	{
		ResizeMidQualityTask<V, VTraits> task;
		task.begin_y = 0;
		task.end_y   = new_height;
		task.image_in = this;
		task.image_out = new_image;
		task.run(0);
	}

	return Reference<ImageMap<V, VTraits> >(new_image);
}


//--------------------------------------------------


template <class V>
struct DownsampleImageMapTaskClosure
{
	V const * in_buffer;
	V		* out_buffer;
	const float* resize_filter;
	ptrdiff_t factor, border_width, in_xres, in_yres, filter_bound, out_xres, out_yres, N;
	float lower_clamping_bound;
};


template <class V>
class DownsampleImageMapTask : public glare::Task
{
public:
	DownsampleImageMapTask(const DownsampleImageMapTaskClosure<V>& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		// Copy to local variables for performance reasons.
		V const* const in_buffer  = closure.in_buffer;
		V      * const out_buffer = closure.out_buffer;
		const float* const resize_filter = closure.resize_filter;
		const ptrdiff_t factor = closure.factor;
		const ptrdiff_t border_width = closure.border_width;
		const ptrdiff_t in_xres = closure.in_xres;
		const ptrdiff_t in_yres = closure.in_yres;
		const ptrdiff_t N = closure.N;
		const ptrdiff_t filter_bound = closure.filter_bound;
		const ptrdiff_t out_xres = closure.out_xres;
		const float lower_clamping_bound = closure.lower_clamping_bound;
		assert(N <= 4);

		for(int y = begin; y < end; ++y)
			for(int x = 0; x < out_xres; ++x)
			{
				const ptrdiff_t u_min = (x + border_width) * factor + factor / 2 - filter_bound;
				const ptrdiff_t v_min = (y + border_width) * factor + factor / 2 - filter_bound;
				const ptrdiff_t u_max = (x + border_width) * factor + factor / 2 + filter_bound;
				const ptrdiff_t v_max = (y + border_width) * factor + factor / 2 + filter_bound;

				float weighted_sum[4] = { 0.f, 0.f, 0.f, 0.f };
				if(u_min >= 0 && v_min >= 0 && u_max < in_xres && v_max < in_yres) // If filter support is completely in bounds:
				{
					uint32 filter_addr = 0;
					for(ptrdiff_t v = v_min; v <= v_max; ++v)
						for(ptrdiff_t u = u_min; u <= u_max; ++u)
						{
							const ptrdiff_t addr = (v * in_xres + u) * N;
							assert(addr >= 0 && addr < (ptrdiff_t)(in_xres * in_yres * N));

							for(ptrdiff_t c = 0; c<N; ++c)
								weighted_sum[c] += in_buffer[addr + c] * resize_filter[filter_addr];
						
							filter_addr++;
						}
				}
				else
				{
					uint32 filter_addr = 0;
					for(ptrdiff_t v = v_min; v <= v_max; ++v)
						for(ptrdiff_t u = u_min; u <= u_max; ++u)
						{
							const ptrdiff_t addr = (v * in_xres + u) * N;
							if(u >= 0 && v >= 0 && u < in_xres && v < in_yres) // Check position we are reading from is in bounds
							{
								for(ptrdiff_t c = 0; c<N; ++c)
									weighted_sum[c] += in_buffer[addr + c] * resize_filter[filter_addr];
							}
							filter_addr++;
						}
				}

				for(ptrdiff_t c = 0; c<N; ++c)
					out_buffer[(y * out_xres + x)*N + c] = myMax(lower_clamping_bound, weighted_sum[c]);
			}
	}

	const DownsampleImageMapTaskClosure<V>& closure;
	int begin, end;
};


// NOTE: copied and adapted from Image4f::downsampleImage().
// border width = margin @ ssf1
template <class V, class VTraits>
void ImageMap<V, VTraits>::downsampleImage(const size_t ssf, const size_t margin_ssf1, const size_t filter_span,
	const float * const resize_filter, const float lower_clamping_bound,
	const ImageMap<V, VTraits>& img_in, ImageMap<V, VTraits>& img_out, glare::TaskManager& task_manager)
{
	assert(margin_ssf1 >= 0);							// have padding pixels
	assert(img_in.getWidth()  > margin_ssf1 * 2);	// have at least one interior pixel in x
	assert(img_in.getHeight() > margin_ssf1 * 2);	// have at least one interior pixel in y
	assert(img_in.getWidth()  % ssf == 0);				// padded img_in is multiple of supersampling factor
	assert(img_in.getHeight() % ssf == 0);				// padded img_in is multiple of supersampling factor

	assert(filter_span > 0);
	assert(resize_filter != 0);

	const ptrdiff_t in_xres  = (ptrdiff_t)img_in.getWidth();
	const ptrdiff_t in_yres  = (ptrdiff_t)img_in.getHeight();
	const ptrdiff_t N = img_in.getN();
	const ptrdiff_t filter_bound = filter_span / 2 - 1;

	// See RendererSettings::computeFinalWidth()
	const ptrdiff_t out_xres = img_in.getWidth()  / ssf - margin_ssf1 * 2;
	const ptrdiff_t out_yres = img_in.getHeight() / ssf - margin_ssf1 * 2;
	img_out.resizeNoCopy((unsigned int)out_xres, (unsigned int)out_yres, (unsigned int)N);

	DownsampleImageMapTaskClosure<V> closure;
	closure.in_buffer = img_in.getPixel(0, 0);
	closure.out_buffer = img_out.getPixel(0, 0);
	closure.resize_filter = resize_filter;
	closure.factor = ssf;
	closure.border_width = margin_ssf1;
	closure.in_xres = in_xres;
	closure.in_yres = in_yres;
	closure.N = N;
	closure.filter_bound = filter_bound;
	closure.out_xres = out_xres;
	closure.out_yres = out_yres;
	closure.lower_clamping_bound = lower_clamping_bound;

	task_manager.runParallelForTasks<DownsampleImageMapTask<V>, DownsampleImageMapTaskClosure<V> >(closure, 0, out_yres);
}


#endif // MAP2D_FILTERING_SUPPORT


template <class V, class VTraits>
double ImageMap<V, VTraits>::averageLuminance() const
{
	const int lum_channel = (N >= 3) ? 1 : 0;
	double sum = 0;
	for(size_t i = 0; i < numPixels(); ++i)
		sum += getPixel(i)[lum_channel];
	return sum / numPixels();
}


template <class V, class VTraits>
void ImageMap<V, VTraits>::blendImage(const ImageMap<V, VTraits>& img, const int destx, const int desty, const Colour4f& colour)
{
	assert(N <= 3);
	const size_t use_N = myMin(this->N, img.N);

	const int h = (int)getHeight();
	const int w = (int)getWidth();

	for(int y = 0; y < (int)img.getHeight(); ++y)
		for(int x = 0; x < (int)img.getWidth(); ++x)
		{
			const int dx = x + destx;
			const int dy = y + desty;

			if(dx >= 0 && dx < w && dy >= 0 && dy < h)
			{
				// setPixel(dx, dy, solid_colour * img.getPixel(x, y).r * alpha + getPixel(dx, dy) * (1 - img.getPixel(x, y).r * alpha));

				float use_alpha = img.getPixel(x, y)[0] * colour.x[3];

				float* pixel = getPixel(dx, dy);
				for(size_t c=0; c<use_N; ++c)
					pixel[c] = colour[c] * use_alpha + pixel[c] * (1 - use_alpha);
			}
		}
}

