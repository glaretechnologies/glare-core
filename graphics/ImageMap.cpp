/*=====================================================================
ImageMap.cpp
-------------------
Copyright Glare Technologies Limited 2017 -
Generated at Fri Mar 11 13:14:38 +0000 2011
=====================================================================*/
#include "ImageMap.h"


#include "../utils/OutStream.h"
#include "../utils/InStream.h"
#include "../utils/Exception.h"
#include "../utils/StringUtils.h"
#include "../utils/TaskManager.h"
#include "../utils/Task.h"
#ifdef _MSC_VER
#pragma warning(push, 0) // Disable warnings
#endif
#if OPENEXR_SUPPORT
#include <half.h>
#endif
#ifdef _MSC_VER
#pragma warning(pop) // Re-enable warnings
#endif


// Explicit template instantiation
template void writeToStream(const ImageMap<float, FloatComponentValueTraits>& im, OutStream& stream);
template void readFromStream(InStream& stream, ImageMap<float, FloatComponentValueTraits>& image);

#if OPENEXR_SUPPORT
template Reference<Map2D> ImageMap<half,   HalfComponentValueTraits>  ::resizeMidQuality(const int new_width, const int new_height, Indigo::TaskManager& task_manager) const;
#endif
template Reference<Map2D> ImageMap<float,  FloatComponentValueTraits> ::resizeMidQuality(const int new_width, const int new_height, Indigo::TaskManager& task_manager) const;
template Reference<Map2D> ImageMap<uint8,  UInt8ComponentValueTraits> ::resizeMidQuality(const int new_width, const int new_height, Indigo::TaskManager& task_manager) const;
template Reference<Map2D> ImageMap<uint16, UInt16ComponentValueTraits>::resizeMidQuality(const int new_width, const int new_height, Indigo::TaskManager& task_manager) const;


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
		throw Indigo::Exception("Unknown version " + toString(v) + ", expected " + toString(IMAGEMAP_SERIALISATION_VERSION) + ".");

	const uint32 w = stream.readUInt32();
	const uint32 h = stream.readUInt32();
	const uint32 N = stream.readUInt32();

	// TODO: handle max image size

	image.resize(w, h, N);
	stream.readData((void*)image.getData(), (size_t)w * (size_t)h * (size_t)N * sizeof(V));
}


template <class V, class VTraits>
class ResizeMidQualityTask : public Indigo::Task
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

		const int new_width  = image->getWidth();
		const int new_height = image->getHeight();
		const int src_w = src_image->getMapWidth();
		const int src_h = src_image->getMapHeight();
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

	int begin_y, end_y;
	const ImageMap<V, VTraits>* image_in;
	ImageMap<V, VTraits>* image_out;
};


template <class V, class VTraits>
Reference<Map2D> ImageMap<V, VTraits>::resizeMidQuality(const int new_width, const int new_height, Indigo::TaskManager& task_manager) const
{
	assert(this->getN() == 3 || this->getN() == 4);

	ImageMap<V, VTraits>* const new_image = new ImageMap<V, VTraits>(new_width, new_height, 3);

	const int num_tasks = myClamp<int>((int)task_manager.getNumThreads(), 1, new_height); // We want at least one task, but no more than the number of rows in the new image.
	const int y_step = Maths::roundedUpDivide(new_height, num_tasks);
	for(int z=0, begin_y=0; z<task_manager.getNumThreads(); ++z, begin_y += y_step)
	{
		Reference<ResizeMidQualityTask<V, VTraits> > task = new ResizeMidQualityTask<V, VTraits>();
		task->begin_y = myMin(begin_y,          new_height);
		task->end_y   = myMin(begin_y + y_step, new_height);
		task->image_in = this;
		task->image_out = new_image;
		task_manager.addTask(task);
	}

	task_manager.waitForTasksToComplete();

	return Reference<ImageMap<V, VTraits> >(new_image);
}
