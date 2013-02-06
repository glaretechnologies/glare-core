/*=====================================================================
ImageFilter.cpp
---------------
File created by ClassTemplate on Sat Aug 05 19:05:41 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "ImageFilter.h"


#include "MitchellNetravali.h"
#include "PNGDecoder.h"
#include "jpegdecoder.h"
#include "EXRDecoder.h"
#include "GaussianImageFilter.h"
#include "image.h"
#include "bitmap.h"
#include "imformatdecoder.h"
#include "../utils/array2d.h"
#include "../utils/stringutils.h"
#include "../indigo/globals.h"
#include "../utils/TaskManager.h"
#include "../maths/vec2.h"
#include "../maths/Matrix2.h"
#include "../maths/mathstypes.h"
#include "../utils/MTwister.h" // just for testing
#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"
#include "../utils/TaskManager.h"
#include "../utils/Task.h"
#include "fftss.h"
#include "../utils/timer.h"
#include "../utils/Plotter.h"
#ifndef INDIGO_NO_OPENMP
#include <omp.h>
#endif
#include "FFTPlan.h"
#include "../maths/GeometrySampling.h"


// Defined in fft4f2d.c
extern "C" void rdft2d(int n1, int n2, int isgn, double **a, int *ip, double *w);


ImageFilter::ImageFilter()
{
}


ImageFilter::~ImageFilter()
{
}



template <class ImageType>
struct ResizeImageTaskClosure
{
	const ImageType* in;
	ImageType* out;
	float pixel_scale;
	float recip_scale;
	int r;
	float mn_b, mn_c;
	float norm_factor;
};


template <class ImageType>
class ResizeImageTask : public Indigo::Task
{
public:
	ResizeImageTask(const ResizeImageTaskClosure<ImageType>& closure_, size_t begin_, size_t end_, size_t stride_) : closure(closure_), begin((int)begin_), end((int)end_), stride((int)stride_) {}

	virtual void run(size_t thread_index)
	{
		// 'Unpack' data from closure ///////
		const ImageType& in = *closure.in;
		ImageType& out = *closure.out;
		const float pixel_scale = closure.pixel_scale;
		const float recip_scale = closure.recip_scale;
		const int r = closure.r;
		const float norm_factor = closure.norm_factor;

		MitchellNetravali<float> mn(closure.mn_b, closure.mn_c);
		//////////////////////////////////////


		int out_w = (int)out.getWidth();
		int out_h = (int)out.getHeight();

		float out_w_2 = out_w * 0.5f;
		float out_h_2 = out_h * 0.5f;

		int in_w = (int)in.getWidth();
		int in_h = (int)in.getHeight();

		float in_w_2 = in_w * 0.5f;
		float in_h_2 = in_h * 0.5f;

		//float recip_out_w = 1.0f / out_w;
		//float recip_out_h = 1.0f / out_h;

		for(int y = begin; y < end; y += stride)
		{
			for(int x=0; x<out_w; ++x)
			{
				float destx = x - out_w_2;
				float desty = y - out_h_2;

				float sx = destx * pixel_scale;
				float sy = desty * pixel_scale;

				float sx_p = sx + in_w_2;
				float sy_p = sy + in_h_2;

				// floor to integer pixel indices
				int sx_pi = (int)std::floor(sx_p);
				int sy_pi = (int)std::floor(sy_p);

				// compute src filter region
				int x_begin = myMax(0, sx_pi - r + 1);
				int y_begin = myMax(0, sy_pi - r + 1);

				int x_end = myMin(in_w, sx_pi + r + 1);
				int y_end = myMin(in_h, sy_pi + r + 1);

				typename ImageType::ColourType c(0.0f);
				float f_sum = 0;
				for(int sy=y_begin; sy<y_end; ++sy)
				for(int sx=x_begin; sx<x_end; ++sx)
				{
					float dx = sx - sx_p; 
					float dy = sy - sy_p;
					float d2 = dx*dx + dy*dy;

					//assert(fabs(dx) <= ceil(2 * scale));
					//assert(fabs(dy) <= ceil(2 * scale));

					//assert((int)(scaled_d2 * mn_table_factor) < 1024);
					//int i = (int)(scaled_d2 * mn_table_factor);
					//float f = i < MN_TABLE_SIZE ? mn_table[i] : 0;
					float f = mn.eval(std::sqrt(d2) * recip_scale); //std::sqrt(scaled_d2));
					f_sum += f;

					//c.r += in.getPixel(sx, sy).r * f;
					//c.g += in.getPixel(sx, sy).g * f;
					//c.b += in.getPixel(sx, sy).b * f;
					c += in.getPixel(sx, sy) * typename ImageType::ColourType(f);
				}

				// Lookup the filter normalisation term
				/*int fnt_x = (int)((sx_p - (float)sx_pi) * (float)FNT_W);
				int fnt_y = (int)((sy_p - (float)sy_pi) * (float)FNT_W);
				assert(fnt_x >= 0 && fnt_x < FNT_W);
				assert(fnt_y >= 0 && fnt_y < FNT_W);

				float f_norm_scale = filter_norm_table[fnt_x + fnt_y * FNT_W];*/
		
				c *= norm_factor;
				out.setPixel(x, y, c);
			}
		}
	}

	const ResizeImageTaskClosure<ImageType>& closure;
	int begin, end, stride;
};


template <class ImageType>
static inline void doResizeImage(const ImageType& in, ImageType& out, float pixel_enlargement_factor, float mn_b, float mn_c, Indigo::TaskManager& task_manager)
{
	assert(mn_b >= 0 && mn_b <= 1);
	assert(mn_c >= 0 && mn_c <= 1);

	MitchellNetravali<float> mn(mn_b, mn_c);

	// Pixel enlargement scale is the 'zoom factor' of out.

	/*
		Let the source image be f_s(u, v).
		Let say we want to compute f_d(s, t) = f_s(s/A, t/A)
		Let B = 1/A
		f_d(s, t)
		= f_s(s/A, t/A)
		= f_s(s*B, t*B)

		We want to reconstruct f_s at the point (s*B, t*B)

		Suppose we are reconstructing at pointx, and x_i = floor(x).
		Assuming a support radius of 2.
		Therefore the point x_1-2 is the largest point before the filter support, and the point x_1+3 is the smallest point above the filter support.
		So we want to loop over the indices like so:
		for(int x=x_i-1; x<x_1+3; ++x)
		In general, with radius r:
		for(int x=x_i - r + 1; x<x_1 + r + 1; ++x)

		                         
x_i-2      x_i-1       x_i       x_i+1      x_i+2       x_1+3
  |----------|----------|----------|----------|----------|
		                  ^
						  x
	*/

	float pixel_scale = 1.0f / pixel_enlargement_factor;

	// Scale is how much we enlarge the reconstruction filter on the source image.  We don't want the reconstruction filter less than the original size.
	// On the other hand, when enlarging the image, we want to widen the filter to filter out high frequency detail.
	float scale = myMax(1.0f, pixel_scale);
	float recip_scale = 1.0f / scale;

	float filter_radius = 2 * scale;
	int r = (int)std::ceil(filter_radius);


	// Table that maps distance squared to mn value for distance
	// Entry at index MN_TABLE_SCALE corresponds to MN filter evaluated at d^2 of 1, entry at index MN_TABLE_SCALE*2 corresponds to MN evaled at d^2 of 2, etc..
	// So last non-zero entry will by at MN_TABLE_SCALE*4, which corresponds to a d^2 of 4, or a distance of 2, which is where the MN function becomes zero.
	/*const int MN_TABLE_SCALE_I = 1024;
	const int MN_TABLE_SIZE = MN_TABLE_SCALE_I * 4;

	float mn_table_factor = MN_TABLE_SCALE_I;

	float mn_table[MN_TABLE_SIZE];
	for(int i=0; i<MN_TABLE_SIZE; ++i)
	{
		float d2 = i * (1 / mn_table_factor);
		float d = std::sqrt(d2);
		mn_table[i] = mn.eval(d) * recip_scale;
	}*/

	// Filter normalisation table.
	// Normalisation factor varies with position
	/*const int FNT_W = 512;
	float recip_FNT_W = 1.f / FNT_W;

	float filter_norm_table[FNT_W*FNT_W];
	for(int y=0; y<FNT_W; ++y)
	{
		//conPrint("");
	for(int x=0; x<FNT_W; ++x)
	{
		// Get (sx_p, sy_p) in [0, 1)^2
		float sx_p = x * recip_FNT_W;
		float sy_p = y * recip_FNT_W;

		// compute src filter region
		int x_begin = 0 - r + 1;
		int y_begin = 0 - r + 1;

		int x_end = 0 + r + 1;
		int y_end = 0 + r + 1;

		float f_sum = 0;
		for(int sy=y_begin; sy<y_end; ++sy)
		for(int sx=x_begin; sx<x_end; ++sx)
		{
			float dx = sx - sx_p; 
			float dy = sy - sy_p;
			float d2 = dx*dx + dy*dy;
			float scaled_d2 = d2 * recip_scale;

			int i = (int)(scaled_d2 * mn_table_factor);
			float f = i < MN_TABLE_SIZE ? mn_table[i] : 0;
			f_sum += f;
		}

		filter_norm_table[x + y*FNT_W] = 1.f / f_sum;

		//conPrintStr(toString(f_sum) + " ");
	}
	}*/


	// Get normalisation factor for filter
	float norm_factor = 1;
	float precomputed_f_sum = 1;
	{
		int x_begin = 0 - r + 1;
		int y_begin = 0 - r + 1;

		int x_end = 0 + r + 1;
		int y_end = 0 + r + 1;

		float f_sum = 0;
		for(int sy=y_begin; sy<y_end; ++sy)
		for(int sx=x_begin; sx<x_end; ++sx)
		{
			float dx = sx - 0.f; 
			float dy = sy - 0.f;
			float d2 = dx*dx + dy*dy;

			assert(fabs(dx) <= ceil(2 * scale));
			assert(fabs(dy) <= ceil(2 * scale));

			float f = mn.eval(std::sqrt(d2) * recip_scale);
			f_sum += f;
		}

		precomputed_f_sum = f_sum;
		norm_factor = 1 / f_sum;
	}


	ResizeImageTaskClosure<ImageType> closure;
	closure.in = &in;
	closure.out = &out;
	closure.pixel_scale = pixel_scale;
	closure.recip_scale = recip_scale;
	closure.r = r;
	closure.mn_b = mn_b;
	closure.mn_c = mn_c;
	closure.norm_factor = norm_factor;

	task_manager.runParallelForTasksInterleaved<ResizeImageTask<ImageType>, ResizeImageTaskClosure<ImageType> >(
		closure, 
		0, // begin
		out.getHeight() // end
	);
}


///==================================================================================================================


void ImageFilter::resizeImage(const Image& in, Image& out, float pixel_enlargement_factor, float mn_b, float mn_c, Indigo::TaskManager& task_manager)
{
	doResizeImage<Image>(in, out, pixel_enlargement_factor, mn_b, mn_c, task_manager);
}


void ImageFilter::resizeImage(const Image4f& in, Image4f& out, float pixel_enlargement_factor, float mn_b, float mn_c, Indigo::TaskManager& task_manager)
{
	doResizeImage<Image4f>(in, out, pixel_enlargement_factor, mn_b, mn_c, task_manager);
}


///==================================================================================================================


struct ResizeImageMapFloatTaskClosure
{
	const ImageMapFloat* in;
	ImageMapFloat* out;
	float pixel_scale;
	float recip_scale;
	int r;
	float mn_b, mn_c;
	float norm_factor;
};


template <int N>
class ResizeImageMapFloatTask : public Indigo::Task
{
public:
	ResizeImageMapFloatTask(const ResizeImageMapFloatTaskClosure& closure_, size_t begin_, size_t end_, size_t stride_) : closure(closure_), begin((int)begin_), end((int)end_), stride((int)stride_) {}

	virtual void run(size_t thread_index)
	{
		// 'Unpack' data from closure ///////
		const ImageMapFloat& in = *closure.in;
		ImageMapFloat& out = *closure.out;
		const float pixel_scale = closure.pixel_scale;
		const float recip_scale = closure.recip_scale;
		const int r = closure.r;
		const float norm_factor = closure.norm_factor;

		MitchellNetravali<float> mn(closure.mn_b, closure.mn_c);
		//////////////////////////////////////


		int out_w = (int)out.getWidth();
		int out_h = (int)out.getHeight();

		float out_w_2 = out_w * 0.5f;
		float out_h_2 = out_h * 0.5f;

		int in_w = (int)in.getWidth();
		int in_h = (int)in.getHeight();

		float in_w_2 = in_w * 0.5f;
		float in_h_2 = in_h * 0.5f;

		float sum[N];

		for(int y = begin; y < end; y += stride)
		{
			for(int x=0; x<out_w; ++x)
			{
				float destx = x - out_w_2;
				float desty = y - out_h_2;

				float sx = destx * pixel_scale;
				float sy = desty * pixel_scale;

				float sx_p = sx + in_w_2;
				float sy_p = sy + in_h_2;

				// floor to integer pixel indices
				int sx_pi = (int)std::floor(sx_p);
				int sy_pi = (int)std::floor(sy_p);

				// compute src filter region
				int x_begin = myMax(0, sx_pi - r + 1);
				int y_begin = myMax(0, sy_pi - r + 1);

				int x_end = myMin(in_w, sx_pi + r + 1);
				int y_end = myMin(in_h, sy_pi + r + 1);

				for(int n=0; n<N; ++n)
					sum[n] = 0;

				float f_sum = 0;
				for(int sy=y_begin; sy<y_end; ++sy)
				for(int sx=x_begin; sx<x_end; ++sx)
				{
					float dx = sx - sx_p; 
					float dy = sy - sy_p;
					float d2 = dx*dx + dy*dy;

					//assert(fabs(dx) <= ceil(2 * scale));
					//assert(fabs(dy) <= ceil(2 * scale));

					//assert((int)(scaled_d2 * mn_table_factor) < 1024);
					//int i = (int)(scaled_d2 * mn_table_factor);
					//float f = i < MN_TABLE_SIZE ? mn_table[i] : 0;
					float f = mn.eval(std::sqrt(d2) * recip_scale); //std::sqrt(scaled_d2));
					f_sum += f;

					for(int n=0; n<N; ++n)
						sum[n] += in.getPixel(sx, sy)[n] * f;
				}

				for(int n=0; n<N; ++n)
					out.getPixel(x, y)[n] = sum[n] * norm_factor;
			}
		}
	}

	const ResizeImageMapFloatTaskClosure& closure;
	int begin, end, stride;
};


void ImageFilter::resizeImage(const ImageMapFloat& in, ImageMapFloat& out, float pixel_enlargement_factor, float mn_b, float mn_c, Indigo::TaskManager& task_manager)
{
	assert(mn_b >= 0 && mn_b <= 1);
	assert(mn_c >= 0 && mn_c <= 1);

	assert(in.getN() == out.getN());

	MitchellNetravali<float> mn(mn_b, mn_c);

	// Pixel enlargement scale is the 'zoom factor' of out.

	/*
		Let the source image be f_s(u, v).
		Let say we want to compute f_d(s, t) = f_s(s/A, t/A)
		Let B = 1/A
		f_d(s, t)
		= f_s(s/A, t/A)
		= f_s(s*B, t*B)

		We want to reconstruct f_s at the point (s*B, t*B)

		Suppose we are reconstructing at pointx, and x_i = floor(x).
		Assuming a support radius of 2.
		Therefore the point x_1-2 is the largest point before the filter support, and the point x_1+3 is the smallest point above the filter support.
		So we want to loop over the indices like so:
		for(int x=x_i-1; x<x_1+3; ++x)
		In general, with radius r:
		for(int x=x_i - r + 1; x<x_1 + r + 1; ++x)

		                         
x_i-2      x_i-1       x_i       x_i+1      x_i+2       x_1+3
  |----------|----------|----------|----------|----------|
		                  ^
						  x
	*/

	float pixel_scale = 1.0f / pixel_enlargement_factor;

	// Scale is how much we enlarge the reconstruction filter on the source image.  We don't want the reconstruction filter less than the original size.
	// On the other hand, when enlarging the image, we want to widen the filter to filter out high frequency detail.
	float scale = myMax(1.0f, pixel_scale);
	float recip_scale = 1.0f / scale;

	float filter_radius = 2 * scale;
	int r = (int)std::ceil(filter_radius);


	
	

	// Table that maps distance squared to mn value for distance
	// Entry at index MN_TABLE_SCALE corresponds to MN filter evaluated at d^2 of 1, entry at index MN_TABLE_SCALE*2 corresponds to MN evaled at d^2 of 2, etc..
	// So last non-zero entry will by at MN_TABLE_SCALE*4, which corresponds to a d^2 of 4, or a distance of 2, which is where the MN function becomes zero.
	/*const int MN_TABLE_SCALE_I = 1024;
	const int MN_TABLE_SIZE = MN_TABLE_SCALE_I * 4;

	float mn_table_factor = MN_TABLE_SCALE_I;

	float mn_table[MN_TABLE_SIZE];
	for(int i=0; i<MN_TABLE_SIZE; ++i)
	{
		float d2 = i * (1 / mn_table_factor);
		float d = std::sqrt(d2);
		mn_table[i] = mn.eval(d) * recip_scale;
	}*/

	// Get normalisation factor for filter
	float norm_factor = 1;
	float precomputed_f_sum = 1;
	{
		int x_begin = 0 - r + 1;
		int y_begin = 0 - r + 1;

		int x_end = 0 + r + 1;
		int y_end = 0 + r + 1;

		float f_sum = 0;
		for(int sy=y_begin; sy<y_end; ++sy)
		for(int sx=x_begin; sx<x_end; ++sx)
		{
			float dx = sx - 0.f; 
			float dy = sy - 0.f;
			float d2 = dx*dx + dy*dy;
			//float scaled_d2 = d2 * recip_scale;

			assert(fabs(dx) <= ceil(2 * scale));
			assert(fabs(dy) <= ceil(2 * scale));

			float f = mn.eval(std::sqrt(d2) * recip_scale); //std::sqrt(scaled_d2));
			f_sum += f;
		}

		precomputed_f_sum = f_sum;
		norm_factor = 1 / f_sum;
	}


	ResizeImageMapFloatTaskClosure closure;
	closure.in = &in;
	closure.out = &out;
	closure.pixel_scale = pixel_scale;
	closure.recip_scale = recip_scale;
	closure.r = r;
	closure.mn_b = mn_b;
	closure.mn_c = mn_c;
	closure.norm_factor = norm_factor;

	if(in.getN() == 1)
	{
		task_manager.runParallelForTasksInterleaved<ResizeImageMapFloatTask<1>, ResizeImageMapFloatTaskClosure>(
			closure, 
			0, // begin
			out.getHeight() // end
		);
	}
	else if(in.getN() == 2)
	{
		task_manager.runParallelForTasksInterleaved<ResizeImageMapFloatTask<2>, ResizeImageMapFloatTaskClosure>(
			closure, 
			0, // begin
			out.getHeight() // end
		);
	}
	else if(in.getN() == 3)
	{
		task_manager.runParallelForTasksInterleaved<ResizeImageMapFloatTask<3>, ResizeImageMapFloatTaskClosure>(
			closure, 
			0, // begin
			out.getHeight() // end
		);
	}
	else if(in.getN() == 4)
	{
		task_manager.runParallelForTasksInterleaved<ResizeImageMapFloatTask<4>, ResizeImageMapFloatTaskClosure>(
			closure, 
			0, // begin
			out.getHeight() // end
		);
	}
	else
	{
		assert(0);
	}

}


///==================================================================================================================


void ImageFilter::chromaticAberration(const Image& in, Image& out, float amount, Indigo::TaskManager& task_manager)
{
	assert(in.getHeight() == out.getHeight() && in.getWidth() == out.getWidth());

	size_t N = in.numPixels();

	out.resize(in.getWidth(), in.getHeight());
	Image temp(in.getWidth(), in.getHeight());
	float mn_b = 0.33f;
	float mn_c = 0.33f;

	resizeImage(in, temp, 1 + amount, mn_b, mn_c, task_manager);
	for(size_t i=0; i<N; ++i)
	{
		out.getPixel(i).r = temp.getPixel(i).r;
		out.getPixel(i).b = temp.getPixel(i).b;
	}

	resizeImage(in, temp, 1 - amount, mn_b, mn_c, task_manager);
	for(size_t i=0; i<N; ++i)
	{
		out.getPixel(i).g = temp.getPixel(i).g;
	}
}


static int smallestPowerOf2GE(int x)
{
	int y = 1;
	while(y < x)
		y *= 2;
	return y;
}


static void saveImageToPng(const Image& im, const std::string& path)
{
	try
	{
		Image save_image = im;
		save_image.scale(1.0e0f);
		save_image.clampInPlace(0, 1);

		Bitmap ldr_image((unsigned int)save_image.getWidth(), (unsigned int)save_image.getHeight(), 3, NULL);

		save_image.copyRegionToBitmap(ldr_image, 0, 0, (unsigned int)save_image.getWidth(), (unsigned int)save_image.getHeight());

		PNGDecoder::write(ldr_image, path);
	}
	catch(ImageExcep& e)
	{
		conPrint("ImageExcep: " + e.what());
	}
}


static void writeImage(const Image& image, const std::string& path)
{
	EXRDecoder::saveImageTo32BitEXR(image, path);

	saveImageToPng(image, path + ".png");
}


static void printImageStats(const Image& image, const std::string& name)
{
	conPrint("Image '" + name + "':");

	double mean_sum = 0;
	double min_val = std::numeric_limits<double>::infinity();
	double max_val = -std::numeric_limits<double>::infinity();
	
	int num_nans = 0;
	int num_infs = 0;
	int num_neg = 0;
	for(int i=0; i<image.numPixels(); ++i)
	{
		const Colour3f& c = image.getPixel(i);

		if(::isNAN(c.r) || ::isNAN(c.g) || ::isNAN(c.b))
			num_nans++;
		else if(::isInf(c.r) || ::isInf(c.g) || ::isInf(c.b))
			num_infs++;
		else if(c.r < 0 || c.g < 0 || c.b < 0)
			num_neg++;

		mean_sum += c.averageVal();
		min_val = myMin<double>(min_val,  c.averageVal());
		max_val = myMax<double>(max_val,  c.averageVal());
	}


	double mean = mean_sum / image.numPixels();

	printVar(mean);
	printVar(min_val);
	printVar(max_val);
	printVar(num_nans);
	printVar(num_infs);
	printVar(num_neg);



	// Plot intensity of horizontal slice through image.
	const int y = (int)image.getHeight() / 2;

	Plotter::DataSet dataset;

	int xr = 50;
	for(int x=myMax(0, (int)image.getWidth()/2 - xr); x<myMin((int)image.getWidth()/2 + xr, (int)image.getWidth()); ++x)
	{
		dataset.points.push_back(Vec2f((float)x, image.getPixel(x, y).r));
	}

	Plotter::PlotOptions options;
	//options.y_axis_log = true;
	Plotter::plot(name + "_plot.png", name, "x", "r", std::vector<Plotter::DataSet>(1, dataset), options);
}


void ImageFilter::lowResConvolve(const Image4f& in, const Image& filter_low, int ssf, Image4f& out, Indigo::TaskManager& task_manager)
{
		const bool debug_output = false;
		const bool verbose = false;

		Timer t;
		if(verbose) conPrint("lowResConvolve()");
	assert(filter_low.getWidth() == 1024);
	assert(filter_low.getHeight() == 1024);

		//writeImage(in, "in.exr");
		//writeImage(filter_low, "filter.exr");

	FFTPlan plan;

		//if(debug_output)
		//	printImageStats(in, "in");

	// Downsample in and filter
	Image4f in_low(in.getWidth() / ssf, in.getHeight() / ssf);
	//Image filter_low(filter.getWidth() / ssf, filter.getHeight() / ssf);

		Timer resize_timer;
	resizeImage(in, in_low, 1.f / ssf, 0.33f, 0.33f, task_manager);
		if(verbose) conPrint("in downsize took " + resize_timer.elapsedStringNPlaces(4));
	//resizeImage(in, in_low, 1.f / ssf, 1.0f, 0.0f);
	//resizeImage(filter, filter_low, 1.f / ssf, 0.33f, 0.33f);


		if(verbose) conPrint("in averageLuminance: " + doubleToStringScientific(in.averageLuminance()));
		if(verbose) conPrint("in_low averageLuminance: " + doubleToStringScientific(in_low.averageLuminance()));

	// Clamp the filter to positive values
	//Image clamped_filter = filter_low;
	//clamped_filter.posClamp();

		if(debug_output)
		{
			//writeImage(in_low, "in_low.exr");
			//writeImage(filter_low, "filter_low.exr");

			//printImageStats(in_low, "in_low");
			//printImageStats(filter_low, "filter_low");
		}

	// convolve
	Image4f convolved_low;
	convolveImage(in_low, filter_low, convolved_low, plan);

		if(debug_output)
		{
			//writeImage(convolved_low, "convolved_low.exr");
			//printImageStats(convolved_low, "convolved_low");
		}

	//convolved_low.posClamp();
	//writeImage(convolved_low, "convolved_low_posclamped.exr");


	// Compute difference image: diff = convolved - in
	Image4f diff_low = convolved_low;
	diff_low.subImage(in_low, 0, 0); // TEMP HACK OFFSET


		if(debug_output)
		{
			//writeImage(diff_low, "diff_low.exr");
			//printImageStats(diff_low, "diff_low");
		}


		//NEW: blur diff_low
		/*Indigo::TaskManager task_manager;
		Image blurred_diff_low(diff_low.getWidth(), diff_low.getHeight());
		GaussianImageFilter::gaussianFilter(diff_low, blurred_diff_low, 
			6.0f, // std dev
			task_manager
		);

		writeImage(blurred_diff_low, "blurred_diff_low.exr");*/
	
	// upsample difference image.
	Image4f diff(in.getWidth(), in.getHeight());

		resize_timer.reset();

	//TEMP resizeImage(diff_low, diff, (float)ssf, 0.6f, 0.2f, task_manager);
	resizeImage(diff_low, diff, (float)ssf, 0.333f, 0.333f, task_manager);

		if(verbose) conPrint("diff upsize took " + resize_timer.elapsedStringNPlaces(4));

		if(debug_output)
		{
			//writeImage(diff, "diff.exr");
			//printImageStats(diff, "diff");
		}

	//resizeImage(diff_low, diff, (float)ssf, 1.0f, 0.0f);

		//conPrint("diff_low averageLuminance: " + doubleToStringScientific(diff_low.averageLuminance()));
		//conPrint("diff averageLuminance: " + doubleToStringScientific(diff.averageLuminance()));
		//writeImage(diff, "diff.exr");

	/*Image pos_diff = diff;
	pos_diff.clampInPlace(0, std::numeric_limits<float>::max());

		writeImage(pos_diff, "pos_diff.exr");*/

	// TEMP: Blur diff
	/*Indigo::TaskManager task_manager;
	Image blurred_diff(diff.getWidth(), diff.getHeight());
	GaussianImageFilter::gaussianFilter(
		pos_diff, 
		blurred_diff, 
		2.0f, // std dev
		task_manager
	);

		writeImage(blurred_diff, "blurred_diff.exr");*/

	// TEMP NEW:
	//diff.clampInPlace(0, std::numeric_limits<float>::max());

	// Add to in: out = in + diff = in + (convolved - in) = convolved

	// NEW: 
	// Do a blend between in and in + diff, with alpha based on how bright the pixel is

	// TEMP HACK: Add a gaussian convolution of the in middle pixel around the middle of diff

		//diff.zero(); // TEMP

	////////////////////////////////////////////////////
	// TEMP HACK: For every really dark pixel in diff, Add in a gaussian convolved with the input at the pixel.
	/*double mean_sum = 0;
	double min_val = std::numeric_limits<double>::infinity();
	double max_val = -std::numeric_limits<double>::infinity();
	
	int num_nans = 0;
	int num_infs = 0;
	int num_neg = 0;
	for(int i=0; i<diff.numPixels(); ++i)
	{
		const Colour3f& c = diff.getPixel(i);

		if(::isNAN(c.r) || ::isNAN(c.g) || ::isNAN(c.b))
			num_nans++;
		else if(::isInf(c.r) || ::isInf(c.g) || ::isInf(c.b))
			num_infs++;
		else if(c.r < 0 || c.g < 0 || c.b < 0)
			num_neg++;

		mean_sum += ::fabs(c.averageVal());
		min_val = myMin<double>(min_val,  c.averageVal());
		max_val = myMax<double>(max_val,  c.averageVal());
	}


	double mean = mean_sum / diff.numPixels();

	/////////////////////////////////////////////

	//double threshold = mean * 10000.0f;
	double threshold = myMax(fabs(min_val), fabs(max_val)) * 0.1f;*/

	//conPrint("--------------- Doing Gaussian splat----------------");
	//printVar(threshold);

	//TEMP:
	// Make an image of just the bright pixels
	//Image threshold_image(diff.getWidth(), diff.getHeight());
	//threshold_image.zero();
	

	/*for(int y=0; y<diff.getHeight(); ++y)
	for(int x=0; x<diff.getWidth(); ++x)
	{
		if(fabs(diff.getPixel(x, y).averageVal()) > threshold)
		{
			//TEMP:
			threshold_image.setPixel(x, y, Colour3f(1));

			//conPrint("Pixel " + toString(x) + ", " + toString(y) + " is above threshold.");

			const Colour3f in_middle = in.getPixel(x, y);

			// Splat in a gaussian 
			int filter_r = 16;
			int dx_begin = myMax(x - filter_r, 0);
			int dx_end   = myMin(x + filter_r, (int)diff.getWidth());
			int dy_begin = myMax(y - filter_r, 0);
			int dy_end   = myMin(y + filter_r, (int)diff.getHeight());

			for(int dy=dy_begin; dy<dy_end; ++dy)
			for(int dx=dx_begin; dx<dx_end;  ++dx)
			{
				const float r2 = Vec2f(dx, dy).getDist2(Vec2f(x, y));

				diff.getPixel(dx, dy) += in_middle * Maths::eval2DGaussian(
					r2,
					3.0f // std deviation
				);
			}
		}
	}*/

	//TEMP:
	//writeImage(threshold_image, "threshold_image.exr");

		if(debug_output)
		{
			
			//printImageStats(diff, "diff_with_gaussians");
		}


	out = in;
	out.addImage(diff, 0, 0);


	// Just copy the alpha over from in for now.
	for(size_t z=0; z<in.numPixels(); ++z)
		out.getPixel(z).x[3] = in.getPixel(z).x[3];


		if(debug_output)
		{
			//writeImage(out, "out.exr");
			//printImageStats(out, "out");
		}

		if(verbose) conPrint("\tlowResConvolve done.  elapsed: " + t.elapsedStringNPlaces(4));

}



void ImageFilter::convolveImage(const Image& in, const Image& filter, Image& out, FFTPlan& plan)
{
	if((filter.getWidth() * filter.getHeight()) > 9)
	{
		//Timer timer;

		// NOTE: Using single-threaded convolveImageFFT() instead of convolveImageFFTSS()
		// This is because
		// a) it uses half the memory
		// b) it seems faster
		// c) it doesn't use OpenMP

		//convolveImageFFTSS(in, filter, out, plan);

		convolveImageFFT(in, filter, out);

		//convolveImageFFTBySections(in, filter, out);

		//conPrint("ImageFilter::convolveImage took " + timer.elapsedStringNPlaces(4));
	}
	else
	{
		convolveImageSpatial(in, filter, out);
	}
}


void ImageFilter::convolveImage(const Image4f& in, const Image& filter, Image4f& out, FFTPlan& plan) // throws Indigo::Exception on out of mem.
{
	convolveImageFFT(in, filter, out);

	assert(out.getWidth() == in.getWidth() && out.getHeight() == in.getHeight());

	// Copy alpha over from in to out
	for(size_t z=0; z<in.numPixels(); ++z)
		out.getPixel(z).x[3] = in.getPixel(z).x[3];
}


void ImageFilter::convolveImageSpatial(const Image& in, const Image& filter, Image& result_out)
{
	result_out.resize(in.getWidth(), in.getHeight());

	//const int rneg_x = (int)((filter.getWidth()  % 2 == 0) ? filter.getWidth()  / 2 - 1 : filter.getWidth()  / 2);
	//const int rneg_y = (int)((filter.getHeight() % 2 == 0) ? filter.getHeight() / 2 - 1 : filter.getHeight() / 2);
	const int rneg_x = (int)(filter.getWidth()  / 2);
	const int rneg_y = (int)(filter.getHeight() / 2);
	const int rpos_x = (int)filter.getWidth()  - rneg_x;
	const int rpos_y = (int)filter.getHeight() - rneg_y;

	assert(rneg_x + rpos_x == filter.getWidth());
	assert(rneg_y + rpos_y == filter.getHeight());

	const int W = (int)result_out.getWidth();
	const int H = (int)result_out.getHeight();

	// For each pixel of result image
	for(int y = 0; y < H; ++y)
	{
		const int unclipped_src_y_min = y - rneg_y;
		const int src_y_min = myMax(0, y - rneg_y);
		const int src_y_max = myMin(H, y + rpos_y);

		//printVar(y);

		for(int x = 0; x < W; ++x)
		{
			const int unclipped_src_x_min = x - rneg_x;
			const int src_x_min = myMax(0, x - rneg_x);
			const int src_x_max = myMin(W, x + rpos_x);

			Colour3f c(0.0f);

			// For each pixel in filter support of source image
			for(int sy = src_y_min; sy < src_y_max; ++sy)
			{
				//const int filter_y = (sy - y) + filter_h_2;
				const int filter_y = sy - unclipped_src_y_min;
				assert(filter_y >= 0 && filter_y < (int)filter.getHeight());		

				for(int sx = src_x_min; sx < src_x_max; ++sx)
				{
					//const int filter_x = (sx - x) + filter_w_2;
					const int filter_x = sx - unclipped_src_x_min;
					assert(filter_x >= 0 && filter_x < (int)filter.getWidth());	

					assert(in.getPixel(sx, sy).r >= 0.0 && in.getPixel(sx, sy).g >= 0.0 && in.getPixel(sx, sy).b >= 0.0);
					assert(isFinite(in.getPixel(sx, sy).r) && isFinite(in.getPixel(sx, sy).g) && isFinite(in.getPixel(sx, sy).b));

					c.addMult(in.getPixel(sx, sy), filter.getPixel(filter_x, filter_y));
				}
			}

			assert(c.r >= 0.0 && c.g >= 0.0 && c.b >= 0.0);
			assert(isFinite(c.r) && isFinite(c.g) && isFinite(c.b));

			result_out.setPixel(x, y, c);
		}
	}
}

void ImageFilter::slowConvolveImageFFT(const Image& in, const Image& filter, Image& out)
{
	const int x_offset = (int)filter.getWidth()  / 2;
	const int y_offset = (int)filter.getHeight() / 2;
	const int W = smallestPowerOf2GE((int)myMax(in.getWidth(),  filter.getWidth())  + x_offset);
	const int H = smallestPowerOf2GE((int)myMax(in.getHeight(), filter.getHeight()) + y_offset);

	Array2d<double> padded_in(W, H);
	Array2d<double> padded_filter(W, H);
	Array2d<double> padded_convolution(W, H);

	out.resize(in.getWidth(), in.getHeight());

	for(int comp = 0; comp < 3; ++comp)
	{
		// Zero pad input
		padded_in.setAllElems(0.0);

		// Blit component of input to padded input
		for(size_t y = 0; y < in.getHeight(); ++y)
		for(size_t x = 0; x < in.getWidth();  ++x)
			padded_in.elem(x, y) = (double)in.getPixel(x, y)[comp];

		// Zero pad filter
		padded_filter.setAllElems(0.0);

		// Blit component of filter to padded filter
		for(size_t y = 0; y < filter.getHeight(); ++y)
		for(size_t x = 0; x < filter.getWidth();  ++x)
			padded_filter.elem(x, y) = (double)filter.getPixel((filter.getWidth() - x) % filter.getWidth(), (filter.getHeight() - y) % filter.getHeight())[comp];
			//padded_filter.elem(x, y) = (double)filter.getPixel(filter.getWidth() - 1 - x, filter.getHeight() - 1 - y)[comp];

		Array2d<Complexd> ft_in;
		realFT(padded_in, ft_in);

		Array2d<Complexd> ft_filter;
		realFT(padded_filter, ft_filter);

		//TEMP:
		/*if(comp == 0)
		{
			conPrint("slowConvolveImageFFT, padded in FT:");
			for(int y=0; y<H; ++y)
			{
				for(int x=0; x<W; ++x)
				{
					conPrintStr(" (" + toString(ft_in.elem(x, y).re()) + ", " + toString(ft_in.elem(x, y).im()) + ")");
				}
				conPrintStr("\n");
			}
			conPrint("slowConvolveImageFFT, padded filter FT:");
			for(int y=0; y<H; ++y)
			{
				for(int x=0; x<W; ++x)
				{
					conPrintStr(" (" + toString(ft_filter.elem(x, y).re()) + ", " + toString(ft_filter.elem(x, y).im()) + ")");
				}
				conPrintStr("\n");
			}
		}*/

		Array2d<Complexd> product(W, H);
		for(int y = 0; y < H; ++y)
		for(int x = 0; x < W; ++x)
			product.elem(x, y) = ft_in.elem(x, y) * ft_filter.elem(x, y);

		//TEMP:
		/*if(comp == 0)
		{
			conPrint("slowConvolveImageFFT, product:");
			for(int y=0; y<H; ++y)
			{
				for(int x=0; x<W; ++x)
				{
					conPrintStr(" (" + toString(product.elem(x, y).re()) + ", " + toString(product.elem(x, y).im()) + ")");
				}
				conPrintStr("\n");
			}
		}*/

		realIFT(product, padded_convolution);

		/*if(comp == 0)
		{
			conPrint("slowConvolveImageFFT, IFT'd product:");
			for(int y=0; y<H; ++y)
			{
				for(int x=0; x<W; ++x)
				{
					conPrintStr(" (" + toString(padded_convolution.elem(x, y))+ ")");
				}
				conPrintStr("\n");
			}
		}*/

		const double scale = 2.0 / (double)(W * H);

		for(size_t y = 0; y < out.getHeight(); ++y)
		for(size_t x = 0; x < out.getWidth();  ++x)
			out.getPixel(x, y)[comp] = 
				(float)(padded_convolution.elem(
					x + x_offset, //(x - 1 + filter.getWidth()/2) % W,
					y + y_offset//(y - 1 + filter.getHeight()/2) % H
					) * scale);
	}
}


void ImageFilter::realFT(const Array2d<double>& data, Array2d<Complexd>& out)
{
	out.resize(data.getWidth(), data.getHeight());

	const int n1 = (int)data.getWidth();
	const int n2 = (int)data.getHeight();

	for(int k1 = 0; k1 < n1; ++k1)
	for(int k2 = 0; k2 < n2; ++k2)
	{
		double re_sum = 0;
		double im_sum = 0;

		for(int j1 = 0; j1 < n1; ++j1)
		for(int j2 = 0; j2 < n2; ++j2)
		{
			const double phase = NICKMATHS_2PI * (double)j1 * (double)k1 / (double)n1 + NICKMATHS_2PI * (double)j2 * (double)k2 / (double)n2;

			re_sum += data.elem(j1, j2) * cos(phase);
			im_sum += data.elem(j1, j2) * sin(phase);
		}
		out.elem(k1, k2) = Complexd(re_sum, im_sum);
	}
}


void ImageFilter::realIFT(const Array2d<Complexd>& data, Array2d<double>& real_out)
{
	real_out.resize(data.getWidth(), data.getHeight());

	const int n1 = (int)data.getWidth();
	const int n2 = (int)data.getHeight();

	for(int k1 = 0; k1 < n1; ++k1)
	for(int k2 = 0; k2 < n2; ++k2)
	{
		double re_sum = 0;

		for(int j1 = 0; j1 < n1; ++j1)
		for(int j2 = 0; j2 < n2; ++j2)
		{
			const double phase = NICKMATHS_2PI * (double)j1 * (double)k1 / (double)n1 + NICKMATHS_2PI * (double)j2 * (double)k2 / (double)n2;

			re_sum += 
				data.elem(j1, j2).re() * cos(phase) + 
				data.elem(j1, j2).im() * sin(phase);
		}
		real_out.elem(k1, k2) = 0.5 * re_sum;
	}
}




void ImageFilter::convolveImageRobinDaviesFFT(const Image& in_image, const Image& filter, Image& out)
{
#if 0
	const int W = smallestPowerOf2GE(myMax(in_image.getWidth(), filter.getWidth()));
	const int H = smallestPowerOf2GE(myMax(in_image.getHeight(), filter.getHeight()));

	assert(Maths::isPowerOfTwo(W) && Maths::isPowerOfTwo(H));

	out.resize(in_image.getWidth(), in_image.getHeight());

	rd::vector2d<std::complex<double> > in(W, H);
	rd::vector2d<std::complex<double> > in_FT(W, H);
	rd::vector2d<std::complex<double> > filter_FT(W, H);
	//rd::vector2d<std::complex<double> > product_FT(W, H);

	rd::Fft2D<double> fft2D(W, H);

	for(int comp=0; comp<3; ++comp)
	{
		// Zero pad input
		in.clear();
		for(int y=0; y<in_image.getHeight(); ++y)
			for(int x=0; x<in_image.getWidth(); ++x)
				in.at(x, y) = in_image.getPixel(x, y)[comp];
		
		// Take FT of image
		fft2D.apply(in, &in_FT);


		// Zero pad filter
		in.clear();
		for(int y=0; y<filter.getHeight(); ++y)
			for(int x=0; x<filter.getWidth(); ++x)
				in.at(x, y) = filter.getPixel(x, y /*filter.getWidth() - x - 1, filter.getHeight() - y - 1*/)[comp];

		// Take FT of filter
		fft2D.apply(in, &filter_FT);
		

		// Multiply FTs
		//for(int y=0; y<H; ++y)
		//	for(int x=0; x<W; ++x)
		//		product_FT.at(x, y) = in_FT.at(x, y) * filter_FT.at(x, y);
		in_FT *= filter_FT;

		// put IFT back in 'in'
		fft2D.applyInverse(in_FT, &in);

		const float scale = 0.25 * (float)(W * H);

		for(int y=0; y<out.getHeight(); ++y)
			for(int x=0; x<out.getWidth(); ++x)
			{
				//printVar(in.at(x, y).real());
				out.getPixel(x, y)[comp] = in.at(x, y).real() * scale;
			}
	}
#endif
}


static inline double& a(Array2d<double>& data, int k1, int k2)
{
	return data.elem(k2, k1);
}
static inline double a(const Array2d<double>& data, int k1, int k2)
{
	return data.elem(k2, k1);
}

static inline Complexd& R(Array2d<Complexd>& data, int k1, int k2)
{
	return data.elem(k2, k1);
}


void ImageFilter::realFFT(const Array2d<double>& input, Array2d<Complexd>& out)
{
	assert(Maths::isPowerOfTwo(input.getWidth()));
	assert(Maths::isPowerOfTwo(input.getHeight()));

	const int W = (int)input.getWidth();
	const int H = (int)input.getHeight();
	out.resize(input.getWidth(), input.getHeight());

	// Alloc working arrays
	const int n1 = H;
	const int n2 = W;
	//double** indata = new double*[H];
	//int* ip = new int[2 + (int)sqrt((double)myMax(n1, n2 / 2))];
	std::vector<double*> indata(H);
	std::vector<int> ip(2 + (int)sqrt((double)myMax(n1, n2 / 2)));
	ip[0] = 0; // ip[0] needs to be initialised to 0

	//double* work_area = new double[myMax(n1 / 2, n2 / 4) + n2 / 4];
	std::vector<double> work_area(myMax(n1 / 2, n2 / 4) + n2 / 4);

	// Copy input data
	Array2d<double> data = input;

	// Compute FT of input
	for(int y = 0; y < H; ++y)
		indata[y] = &data.elem(0, y);

	rdft2d(
		n1, // input data length (dim1)
		n2, // input data length (dim2)
		1, // input sign (1 for FFT)
		&indata[0],
		&ip[0],
		&work_area[0]
		);

	// Now we need to decipher the output

	//                    a[k1][2*k2] = R[k1][k2] = R[n1-k1][n2-k2], 
	//                    a[k1][2*k2+1] = I[k1][k2] = -I[n1-k1][n2-k2], 
	//                       0<k1<n1, 0<k2<n2/2, 
	for(int k1 = 1; k1 < n1; ++k1)
	for(int k2 = 1; k2 < n2 / 2; ++k2)
	{
		R(out, k1, k2) = Complexd(a(data, k1, 2*k2), a(data, k1, 2*k2+1));

		R(out, n1-k1, n2-k2) = Complexd(a(data, k1, 2*k2), -a(data, k1, 2*k2+1));
	}

	//                    a[0][2*k2] = R[0][k2] = R[0][n2-k2], 
    //                    a[0][2*k2+1] = I[0][k2] = -I[0][n2-k2], 
    //                       0<k2<n2/2, 
	for(int k2 = 1; k2 < n2 / 2; ++k2)
	{
		R(out, 0, k2) = Complexd(a(data, 0, 2*k2), a(data, 0, 2*k2+1));
		R(out, 0, n2-k2) = Complexd(a(data, 0, 2*k2), -a(data, 0, 2*k2+1));
	}	

    //                    a[k1][0] = R[k1][0] = R[n1-k1][0], 
    //                    a[k1][1] = I[k1][0] = -I[n1-k1][0], 
    //                    a[n1-k1][1] = R[k1][n2/2] = R[n1-k1][n2/2], 
    //                    a[n1-k1][0] = -I[k1][n2/2] = I[n1-k1][n2/2], 
    //                       0<k1<n1/2,
	for(int k1 = 1; k1 < n1 / 2; ++k1)
	{
		R(out, k1, 0) = Complexd(a(data, k1, 0), a(data, k1, 1));
		R(out, n1-k1, 0) = Complexd(a(data, k1, 0), -a(data, k1, 1));

		R(out, k1, n2/2) = Complexd(a(data, n1-k1, 1), -a(data, n1-k1, 0));
		R(out, n1-k1, n2/2) = Complexd(a(data, n1-k1, 1), a(data, n1-k1, 0));
	}

    //                    a[0][0] = R[0][0], 
    //                    a[0][1] = R[0][n2/2], 
    //                    a[n1/2][0] = R[n1/2][0], 
    //                    a[n1/2][1] = R[n1/2][n2/2]
	R(out, 0, 0) =			Complexd(a(data, 0, 0), 0.0);
	R(out, 0, n2/2) =		Complexd(a(data, 0, 1), 0.0);
	R(out, n1/2, 0) =		Complexd(a(data, n1/2, 0), 0.0);
	R(out, n1/2, n2/2) =	Complexd(a(data, n1/2, 1), 0.0);

	//delete[] work_area;
	//delete[] ip;
	//delete[] indata;
}


void ImageFilter::convolveImageFFTBySections(const Image& in, const Image& filter, Image& out)
{
	conPrint("-----------ImageFilter::convolveImageFFTBySections()-----------");

	const int fw = (int)filter.getWidth();
	const int fw_2 = fw / 2;

	const int block_w = 3 * fw_2;

	printVar(block_w);

	Image temp_in (block_w, block_w);
	Image temp_out(block_w, block_w);

	for(int y=0; y<in.getHeight(); y+=fw_2)
	{
		for(int x=0; x<in.getWidth(); x+=fw_2)
		{
			conPrint("");
			conPrint("Processing chunk x: " + toString(x) + ", y: " + toString(y));



			temp_in.zero();

			conPrint("blitting from begin=(" + toString(x - fw_2) + ", " + toString(y - fw_2) + "), end=(" + toString(x + fw) + ", " + toString(y + fw) + ")");

			// Copy chunk of in to temp_in
			in.blitToImage(x - fw_2, y - fw_2, x + fw, y + fw, temp_in, 0, 0);

				// saveImage(temp_in, "temp_in " + toString(x) + " " + toString(y) + ".png");

			convolveImageFFT(temp_in, filter, temp_out);

			// Blit temp_out to the correct section of out
			temp_out.blitToImage(fw_2, fw_2, fw, fw, out, x, y);
		}
	}
}


template <class ImageType>
static inline void doConvolveImageFFT(const ImageType& in, const Image& filter, ImageType& out)
{
	const bool verbose = false;

#ifdef DEBUG
	for(unsigned int i=0; i<in.numPixels(); ++i)
		assert(in.getPixel(i).isFinite());
	for(unsigned int i=0; i<filter.numPixels(); ++i)
		assert(filter.getPixel(i).isFinite());
#endif
	assert(filter.getWidth() >= 2);
	assert(filter.getHeight() >= 2);

	const int x_offset = (int)filter.getWidth()  / 2;
	const int y_offset = (int)filter.getHeight() / 2;

	const int W = smallestPowerOf2GE((int)myMax(in.getWidth(),  filter.getWidth())  + x_offset);
	const int H = smallestPowerOf2GE((int)myMax(in.getHeight(), filter.getHeight()) + y_offset);

	assert(Maths::isPowerOfTwo(W) && Maths::isPowerOfTwo(H));
	assert(W >= 2);
	assert(H >= 2);

	Array2d<double> padded_in(W, H);
	Array2d<double> padded_filter(W, H);
	Array2d<double> product(W, H);

	if(verbose) printVar(in.getWidth());
	if(verbose) printVar(filter.getWidth());
	if(verbose) printVar(x_offset);
	if(verbose) printVar((int)myMax(in.getWidth(),  filter.getWidth())  + x_offset);
	if(verbose) printVar(W);

	// Print out memory usage.
	const size_t mem_used = padded_in.getWidth() * padded_in.getHeight() * sizeof(double) * 3;
	if(verbose) conPrint("convolveImageFFT() w: " + toString(W) + ", h: " + toString(H));
	if(verbose) conPrint("convolveImageFFT() mem used: " + getNiceByteSize(mem_used));

	out.resize(in.getWidth(), in.getHeight());

	// Alloc working arrays
	const int n1 = H;
	const int n2 = W;
	//double** indata = new double*[H];

	//int* ip = new int[2 + (int)sqrt((double)myMax(n1, n2 / 2))];
	std::vector<double*> indata(H);
	std::vector<int> ip(2 + (int)sqrt((double)myMax(n1, n2 / 2)));
	ip[0] = 0; // ip[0] needs to be initialised to 0

	//double* work_area = new double[myMax(n1 / 2, n2 / 4) + n2 / 4];
	std::vector<double> work_area(myMax(n1 / 2, n2 / 4) + n2 / 4);


	for(size_t comp = 0; comp < 3; ++comp)
	{
		// Zero pad input
		padded_in.setAllElems(0.0);

		// Blit component of input to padded input
		for(size_t y = 0; y < in.getHeight(); ++y)
		for(size_t x = 0; x < in.getWidth();  ++x)
			padded_in.elem(x, y) = (double)in.getPixel(x, y)[(unsigned int)comp];

		// Zero pad filter
		padded_filter.setAllElems(0.0);

		// Blit component of filter to padded filter
		for(size_t y = 0; y < filter.getHeight(); ++y)
		for(size_t x = 0; x < filter.getWidth();  ++x)
			padded_filter.elem(x, y) = (double)filter.getPixel((filter.getWidth() - x) % filter.getWidth(), (filter.getHeight() - y) % filter.getHeight())[comp]; // Note: rotating filter around center point here.
			//padded_filter.elem(x, y) = (double)filter.getPixel(filter.getWidth() - x - 1, filter.getHeight() - y - 1)[comp]; // Note: rotating filter around center point here.


		//TEMP:
		//Array2d<Complexd> ft_in;
		//realFT(padded_in, ft_in);
		//Array2d<Complexd> ft_filter;
		//realFT(padded_filter, ft_filter);
		//// Compute slow reference product
		//Array2d<Complexd> ref_product(W, H);
		//for(int y=0; y<H; ++y)
		//	for(int x=0; x<W; ++x)
		//		ref_product.elem(x, y) = ft_in.elem(x, y) * ft_filter.elem(x, y);


		// Compute FT of input
		for(int y = 0; y < H; ++y)
			indata[y] = padded_in.rowBegin(y);

		rdft2d(
			n1, // input data length (dim1)
			n2, // input data length (dim2)
			1, // input sign (1 for FFT)
			&indata[0],
			&ip[0],
			&work_area[0]
			);

		//Ok, now FT of input image should be in 'padded_in'.

		// Compute FT of filter
		for(int y = 0; y < H; ++y)
			indata[y] = padded_filter.rowBegin(y);

		rdft2d(
			n1, // input data length (dim1)
			n2, // input data length (dim2)
			1, // input sign (1 for FFT)
			&indata[0],
			&ip[0],
			&work_area[0]
			);

		
		/*conPrint("Padded in: ");
		for(int y=0; y<H; ++y)
		{
			for(int x=0; x<W; ++x)
			{
				conPrintStr(" " + toString(padded_in.elem(x, y)));
			}
			conPrintStr("\n");
		}

		conPrint("-------------------------------");

		for(int y=0; y<H; ++y)
		{
			for(int x=0; x<W; ++x)
			{
				conPrintStr(" (" + toString(ft_in.elem(x, y).re()) + ", " + toString(ft_in.elem(x, y).im()) + ")" );
			}
			conPrintStr("\n");
		}*/
			

		//These cases have zero imaginary, so just multiply real components
		a(product, 0, 0) = a(padded_in, 0, 0) * a(padded_filter, 0, 0);
		a(product, 0, 1) = a(padded_in, 0, 1) * a(padded_filter, 0, 1);
		a(product, n1 / 2, 0) = a(padded_in, n1 / 2, 0) * a(padded_filter, n1 / 2, 0);
		a(product, n1 / 2, 1) = a(padded_in, n1 / 2, 1) * a(padded_filter, n1 / 2, 1);

		//{
		//	printVar(a(product, 0, 1));
		//	const Complexd complex = ref_product.elem(0, 0);
		//	assert(epsEqual(complex.re(), a(product, 0, 0)));
		//	//assert(epsEqual(complex.im(), a(product, 0, 1)));
		//}

		// Handle k1 = 0, 0<k2<n2/2 case:
		//	a[0][2*k2] = R[0][k2] = R[0][n2-k2], 
        //  a[0][2*k2+1] = I[0][k2] = -I[0][n2-k2], 
        //		0<k2<n2/2,
		for(int k2 = 1; k2 < n2 / 2; ++k2)
		{
			const double a_ = a(padded_in, 0, 2*k2);
			const double b = a(padded_in, 0, 2*k2+1);

			const double c = a(padded_filter, 0, 2*k2);
			const double d = a(padded_filter, 0, 2*k2+1);

			a(product, 0, 2*k2) = a_*c - b*d; // Re(out)
			a(product, 0, 2*k2+1) = a_*d + b*c; // Im(out)
		}

		// a[k1][0] = R[k1][0] = R[n1-k1][0], 
        // a[k1][1] = I[k1][0] = -I[n1-k1][0], 
        // a[n1-k1][1] = R[k1][n2/2] = R[n1-k1][n2/2], 
        // a[n1-k1][0] = -I[k1][n2/2] = I[n1-k1][n2/2], 
        //    0<k1<n1/2, 
		for(int k1 = 1; k1 < H / 2; ++k1)
		{
			{
			const double a_ = a(padded_in, k1, 0); //padded_in.elem(0, k1); // Re(in)
			const double b = a(padded_in, k1, 1); //padded_in.elem(1, k1); // Im(in)

			const double c = a(padded_filter, k1, 0); //padded_filter.elem(0, k1); // Re(filter)
			const double d = a(padded_filter, k1, 1); //padded_filter.elem(1, k1); // Im(filter)

			a(product, k1, 0) = a_*c - b*d; // Re(out)
			a(product, k1, 1) = a_*d + b*c; // Im(out)

			//const Complexd complex = ref_product.elem(0, k1);
			//assert(epsEqual(complex.re(), a(product, k1, 0)));
			//assert(epsEqual(complex.im(), a(product, k1, 1)));
			}

			// bottom half of column
			{
			const double a_ = a(padded_in, n1-k1, 1); // Re(in)
			const double b = -a(padded_in, n1-k1, 0); // Im(in)

			const double c = a(padded_filter, n1-k1, 1); // Re(filter)
			const double d = -a(padded_filter, n1-k1, 0); // Im(filter)

			const double prod_re = a_*c - b*d;
			const double prod_im = a_*d + b*c;

			a(product, n1-k1, 1) = prod_re;
			a(product, n1-k1, 0) = -prod_im;

			//const Complexd complex = ref_product.elem(n2/2, k1);
			//assert(epsEqual(complex.re(), prod_re));
			//assert(epsEqual(complex.im(), prod_im));
			}
		}

		// a[k1][2*k2] = R[k1][k2] = R[n1-k1][n2-k2], 
        // a[k1][2*k2+1] = I[k1][k2] = -I[n1-k1][n2-k2], 
        //     0<k1<n1, 0<k2<n2/2, 
		for(int k1=1; k1<n1; ++k1)
		{
			for(int k2=1; k2<n2/2; ++k2)
			{
 				const double a_ = a(padded_in, k1, k2*2);
				const double b = a(padded_in, k1, k2*2+1);

				const double c = a(padded_filter, k1, k2*2);
				const double d = a(padded_filter, k1, k2*2+1);

				a(product, k1, k2*2) = a_*c - b*d; // Re(out)
				a(product, k1, k2*2+1) = a_*d + b*c; // Im(out)

				//const double re_out = product.elem(k2*2, k1);
				//const Complexd ref_complex = ref_product.elem(0, k1);
				//assert(epsEqual(ref_product.elem(k2, k1).re(), a(product, k1, k2*2)));
				//assert(epsEqual(ref_product.elem(k2, k1).im(), a(product, k1, k2*2+1)));
			}
		}


		//TEMP: print out product
		/*conPrint("----------------------Product:---------------------------");
		for(int y=0; y<H; ++y)
		{
			for(int x=0; x<W; ++x)
			{
				conPrintStr(" " + toString(product.elem(x, y)));
			}
			conPrintStr("\n");
		}*/

		// Compute IFT of product
		for(int y = 0; y < H; ++y)
			indata[y] = product.rowBegin(y);
		
		rdft2d(
			n1, // input data length (dim1)
			n2, // input data length (dim2)
			-1, // input sign (-1 for IFFT)
			&indata[0],
			&ip[0],
			&work_area[0]
			);
		
		/*Array2d<double> reference_convolution(W, H);
		realIFT(ref_product, reference_convolution);

		conPrint("--------------reference_convolution-----------------");

		for(int y=0; y<H; ++y)
		{
			for(int x=0; x<W; ++x)
			{
				conPrintStr(" " + toString(reference_convolution.elem(x, y)));
			}
			conPrintStr("\n");
		}


		for(int y=0; y<H; ++y)
			for(int x=0; x<W; ++x)
			{
				assert(epsEqual(reference_convolution.elem(x, y), product.elem(x, y)));
			}*/

		const double scale = 2.0 / (double)(W * H);

		// Read out real coefficients
		for(unsigned int y=0; y<out.getHeight(); ++y)
			for(unsigned int x=0; x<out.getWidth(); ++x)
				out.getPixel(x, y)[(unsigned int)comp] = (float)(product.elem(
					x + x_offset, //(x + filter.getWidth()/2) % W, 
					y + y_offset //(y + filter.getHeight()/2) % H
					) * scale);
	}

	// Free working arrays
	//delete[] ip;
	//delete[] work_area;
	//delete[] indata;
}


void ImageFilter::convolveImageFFT(const Image& in, const Image& filter, Image& out)
{
	doConvolveImageFFT<Image>(in, filter, out);
}


void ImageFilter::convolveImageFFT(const Image4f& in, const Image& filter, Image4f& out)
{
	doConvolveImageFFT<Image4f>(in, filter, out);
}


void ImageFilter::convolveImageFFTSS(const Image& in, const Image& filter, Image& out, FFTPlan& plan)
{
#ifdef DEBUG
	for(size_t i = 0; i < in.numPixels(); ++i)
		assert(in.getPixel(i).isFinite());
	for(size_t i = 0; i < filter.numPixels(); ++i)
		assert(filter.getPixel(i).isFinite());
#endif
	assert(filter.getWidth() >= 2);
	assert(filter.getHeight() >= 2);

	const int x_offset = (int)filter.getWidth() / 2;
	const int y_offset = (int)filter.getHeight() / 2;

	const size_t W = smallestPowerOf2GE((int)myMax(in.getWidth(), filter.getWidth()) + x_offset);
	const size_t H = smallestPowerOf2GE((int)myMax(in.getHeight(), filter.getHeight()) + y_offset);

	assert(Maths::isPowerOfTwo(W) && Maths::isPowerOfTwo(H));
	assert(W >= 2);
	assert(H >= 2);

	// Stride between rows. FFTSS seems to like + 1 for some perverse reason.
	const size_t py = W + 1; //TEMP
	const size_t N = py * H * 2; // N = num doubles in arrays

	// If we have previously failed, don't do aperture diffraction.
	if(plan.failed_to_allocate_buffers)
	{
		out = in;
		return;
	}

	
	if(!plan.buffer_a)
	{
		// TODO: Throw an exception if buffer size is > max long value.
		plan.buffer_a = (double*)fftss_malloc((long)(py * H * sizeof(double) * 2));
		if(!plan.buffer_a)
		{
			plan.failed_to_allocate_buffers = true;
			throw Indigo::Exception("Failed to allocate buffer.");
		}
	}
	if(!plan.buffer_b)
	{
		plan.buffer_b = (double*)fftss_malloc((long)(py * H * sizeof(double) * 2));
		if(!plan.buffer_b)
		{
			plan.failed_to_allocate_buffers = true;
			throw Indigo::Exception("Failed to allocate buffer.");
		}
	}
	if(!plan.product)
	{
		plan.product = (double*)fftss_malloc((long)(py * H * sizeof(double) * 2));
		if(!plan.product)
		{
			plan.failed_to_allocate_buffers = true;
			throw Indigo::Exception("Failed to allocate buffer.");
		}
	}


	// conPrint("convolveImageFFTSS() w: " + toString(W) + ", h: " + toString(H));
	// const size_t mem_used = py * H * sizeof(double) * 2 * 3;
	// conPrint("convolveImageFFTSS() mem used: " + getNiceByteSize(mem_used));

	try
	{
		out.resize(in.getWidth(), in.getHeight());
	}
	catch(std::bad_alloc&)
	{
		throw Indigo::Exception("Failed to allocate buffer.");
	}

	if(!plan.in_plan)
	{
		#ifndef INDIGO_NO_OPENMP
		fftss_plan_with_nthreads(omp_get_max_threads());
		#endif
		plan.in_plan = fftss_plan_dft_2d((long)W, (long)H, (long)py, plan.buffer_a, plan.product,
						FFTSS_FORWARD, FFTSS_DESTROY_INPUT);
		//FFTSS_INOUT
		//FFTSS_ESTIMATE
		//FFTSS_VERBOSE
	}

	if(!plan.filter_plan)
	{
		#ifndef INDIGO_NO_OPENMP
		fftss_plan_with_nthreads(omp_get_max_threads());
		#endif
		plan.filter_plan = fftss_plan_dft_2d((long)W, (long)H, (long)py, plan.buffer_a, plan.buffer_b,
							FFTSS_FORWARD, FFTSS_DESTROY_INPUT);
	}

	if(!plan.ift_plan)
	{
		#ifndef INDIGO_NO_OPENMP
		fftss_plan_with_nthreads(omp_get_max_threads());
		#endif
		plan.ift_plan = fftss_plan_dft_2d((long)W, (long)H, (long)py, plan.product, plan.buffer_a,
						FFTSS_BACKWARD, FFTSS_DESTROY_INPUT);
		
	}

	for(int comp=0; comp<3; ++comp)
	{
		// Copy image 'in' to buffer_a
		
		// Zero pad input
		//for(int i=0; i<N; ++i)
		//	plan.buffer_a[i] = 0.0;

		const double scale = 1.0 / (W * H);

		// Blit component of input to padded input
		for(size_t y = 0; y < in.getHeight(); ++y)
		{
			for(size_t x = 0; x < in.getWidth(); ++x)
			{
				plan.buffer_a[x*2 + y*py*2] = (double)in.getPixel(x, y)[comp]; // Re
				plan.buffer_a[x*2 + y*py*2 + 1] = 0.0; // Im
			}
			for(size_t x = in.getWidth(); x < py; ++x)
				plan.buffer_a[x*2 + y*py*2] = plan.buffer_a[x*2 + y*py*2 + 1] = 0.0;
		}
		for(size_t y = in.getHeight(); y < H; ++y)
			for(size_t x = 0; x < py; ++x)
				plan.buffer_a[x*2 + y*py*2] = plan.buffer_a[x*2 + y*py*2 + 1] = 0.0;

		// Compute FT of input, writing to buffer 'product'.
		fftss_execute(plan.in_plan);

		// Copy image 'filter' to buffer_a
		// Zero pad filter
		//for(int i=0; i<N; ++i)
		//	plan.buffer_a[i] = 0.0;

		// Blit component of filter to padded filter
		for(size_t y = 0; y < filter.getHeight(); ++y)
		{
			for(size_t x = 0; x < filter.getWidth(); ++x)
			{
				//plan.buffer_a[x*2 + y*py*2] = (double)filter.getPixel(filter.getWidth() - x - 1, filter.getHeight() - y - 1)[comp]/* * scale*/; // Note: rotating filter around center point here.
				plan.buffer_a[x*2 + y*py*2] = (double)filter.getPixel((filter.getWidth() - x) % filter.getWidth(), (filter.getHeight() - y) % filter.getHeight())[comp]/* * scale*/; // Note: rotating filter around center point here.

				plan.buffer_a[x*2 + y*py*2 + 1] = 0.0; // Im
			}
			for(size_t x = filter.getWidth(); x < py; ++x)
				plan.buffer_a[x*2 + y*py*2] = plan.buffer_a[x*2 + y*py*2 + 1] = 0.0;
		}
		for(size_t y = filter.getHeight(); y < H; ++y)
			for(size_t x = 0; x < py; ++x)
				plan.buffer_a[x*2 + y*py*2] = plan.buffer_a[x*2 + y*py*2 + 1] = 0.0;

		
		// Compute FT of filter, writing to buffer_b
		fftss_execute(plan.filter_plan);

		// Form product of buffer_a (currently in product, and buffer b)
		/*
		(a + bi) * (c + di)
		= ac + adi + bci + bdi^2
		= ac + (ad + bc)i - bd
		= (ac - bd) + (ad + bc)i
		*/
		for(size_t i=0; i<N/2; ++i)
		{
			const double a = plan.product[i*2]; // re
			const double b = plan.product[i*2 + 1]; // im
			const double c = plan.buffer_b[i*2]; // re
			const double d = plan.buffer_b[i*2 + 1]; //im
			plan.product[i*2] = (a*c - b*d);
			plan.product[i*2+1] = (a*d + b*c);
		}



		//TEMP:
		/*if(comp == 0)
		{
			conPrint("convolveImageFFTSS, padded in FT:");
			for(int y=0; y<H; ++y)
			{
				for(int x=0; x<W; ++x)
				{
					conPrintStr(" (" + doubleToString(plan.padded_in[x*2 + y*py*2], 10) + ", " + doubleToString(plan.padded_in[x*2 + y*py*2 + 1], 10) + ")");
				}
				conPrintStr("\n");
			}
			conPrint("convolveImageFFTSS, padded filter FT:");
			for(int y=0; y<H; ++y)
			{
				for(int x=0; x<W; ++x)
				{
					conPrintStr(" (" + doubleToString(plan.padded_filter[x*2 + y*py*2], 10) + ", " + doubleToString(plan.padded_filter[x*2 + y*py*2 + 1], 10) + ")");
				}
				conPrintStr("\n");
			}
		}*/

	
		


		//TEMP:
		/*if(comp == 0)
		{
			conPrint("convolveImageFFTSS, product:");
			for(int y=0; y<H; ++y)
			{
				for(int x=0; x<py; ++x)
				{
					conPrintStr(" (" + doubleToString(plan.product[x*2 + y*py*2], 10) + ", " + doubleToString(plan.product[x*2 + y*py*2 + 1], 10) + ")");
				}
				conPrintStr("\n");
			}
		}*/
	
		// Compute IFT of product, which writes back to buffer_a
		
		fftss_execute(plan.ift_plan);


		/*if(comp == 0)
		{
			conPrint("convolveImageFFTSS, IFT'd product:");
			for(int y=0; y<H; ++y)
			{
				for(int x=0; x<py; ++x)
				{
					conPrintStr(" (" + toString(plan.product[x*2 + y*py*2]) + ", " + toString(plan.product[x*2 + y*py*2 + 1]) + ")");
				}
				conPrintStr("\n");
			}
		}*/
		
		// Read out real coefficients
		for(size_t y = 0; y < out.getHeight(); ++y)
		for(size_t x = 0; x < out.getWidth();  ++x)
		{
			out.getPixel(x, y)[comp] = (float)(plan.buffer_a[(x + x_offset)*2 + py*(y + y_offset)*2] * scale);
		}
	}
}


void ImageFilter::FFTSS_realFFT(const Array2d<double>& data, Array2d<Complexd>& out)
{
	Timer t;

	const int py = (int)data.getWidth() + 1;

	double* in = (double*)fftss_malloc((long)(py * data.getHeight() * sizeof(double) * 2));
	if(!in)
		throw Indigo::Exception("Failed to allocate buffer.");

	double* outbuf = (double*)fftss_malloc((long)(py * data.getHeight() * sizeof(double) * 2));
	if(!outbuf)
		throw Indigo::Exception("Failed to allocate buffer.");

	for(int i=0; i<py * (int)data.getHeight() * 2; ++i)
		in[i] = 0.0;
	//double* out = (double*)fftss_malloc(data.getWidth() * data.getHeight() * sizeof(double) * 2);

	// Copy data to in
	/*for(unsigned int i=0; i<data.getWidth() * data.getHeight(); ++i)
	{
		in[i*2    ] = data.getData()[i];
		in[i*2 + 1] = 0.0;
	}*/
	//const double scale = 1.0 / (data.getWidth() * data.getHeight());

	for(unsigned int y=0; y<data.getHeight(); ++y)
		for(unsigned int x=0; x<data.getWidth(); ++x)
		{
			in[x * 2 + y * py * 2] = data.elem(x, y)/* * scale*/; // re
			in[x * 2 + y * py * 2 + 1] = 0.0; // im
		}

	//TEMP
	//for(int i=0; i<data.getWidth() * data.getHeight(); ++i)
	//	std::cout << in[i*2] << ", " << in[i*2 + 1] << std::endl;
	#ifndef INDIGO_NO_OPENMP
	//conPrint("omp_get_max_threads: " + toString(omp_get_max_threads()));
	#endif

	t.reset();
	#ifndef INDIGO_NO_OPENMP
	fftss_plan_with_nthreads(omp_get_max_threads());
	#endif
	fftss_plan plan = fftss_plan_dft_2d((long)data.getWidth(), (long)data.getHeight(), py, in, outbuf,
                           FFTSS_FORWARD, FFTSS_VERBOSE);

	conPrint("plan: " + t.elapsedString());

	for(int i=0; i<py * (int)data.getHeight() * 2; ++i)
		in[i] = 0.0;
	for(unsigned int y=0; y<data.getHeight(); ++y)
		for(unsigned int x=0; x<data.getWidth(); ++x)
		{
			in[x * 2 + y * py * 2] = data.elem(x, y)/* * scale*/; // re
			in[x * 2 + y * py * 2 + 1] = 0.0; // im
		}
	
	t.reset();
	fftss_execute(plan);
	conPrint("execute: " + t.elapsedString());

	
	// Copy to output
	out.resize(data.getWidth(), data.getHeight());
	/*for(unsigned int i=0; i<data.getWidth() * data.getHeight(); ++i)
	{
		out.getData()[i].a = in[i*2];
		out.getData()[i].a = in[i*2 + 1];
	}*/
	//const double scale = 1.0 / (data.getWidth() * data.getHeight() * data.getWidth() * data.getHeight());

	for(unsigned int y=0; y<data.getHeight(); ++y)
		for(unsigned int x=0; x<data.getWidth(); ++x)
		{
			out.elem(x, y).a = outbuf[x * 2 + y * py * 2]/* * scale*/;
			out.elem(x, y).b = outbuf[x * 2 + y * py * 2 + 1]/* * scale*/ * -1.0;
		}

	fftss_destroy_plan(plan);
	fftss_free(in);
	fftss_free(outbuf);
}


Reference<Image> ImageFilter::convertDebevecMappingToLatLong(const Reference<Image>& in)
{
	const size_t w = in->getWidth();
	const size_t h = in->getHeight();
	Reference<Image> out(new Image(w, w));

	for(size_t y=0; y<h; ++y)
		for(size_t x=0; x<w; ++x)
		{
			float phi = -NICKMATHS_2PIf * x / w;
			float theta = NICKMATHS_PIf * y / h;
			const Vec3f p = GeometrySampling::dirForSphericalCoords<float>(phi, theta);

			const Vec3f D = Vec3f(p.x, -p.z, p.y);

			const float denom = std::sqrt(D.x*D.x + D.y*D.y);
			float r;
			if(denom && D.z >= -1.0 && D.z <= 1.0)
				r = NICKMATHS_RECIP_PIf * acos(D.z) / denom;
			else
				r = 0.0;

			assert(Maths::inHalfClosedInterval(D.x * r * 0.5 + 0.5, 0.0, 1.0));
			assert(Maths::inHalfClosedInterval(D.y * r * 0.5 + 0.5, 0.0, 1.0));
			
			float im_u = D.x*r;
			float im_v = D.y*r;

			float s = (im_u + 1) * 0.5f;
			float t = (im_v + 1) * 0.5f;

			out->setPixel(x, y, in->vec3SampleTiled(s, t));
		}

	return out;
}


#if BUILD_TESTS


static void testConvolutionWithDims(int in_w, int in_h, int f_w, int f_h)
{
	conPrint("testConvolutionWithDims()");
	printVar(in_w);
	printVar(in_h);
	printVar(f_w);
	printVar(f_h);

	MTwister rng(2);

	Timer t;

	Image in(in_w, in_h);
	for(unsigned int i=0; i<in.numPixels(); ++i)
		in.getPixel(i).set(rng.unitRandom(), rng.unitRandom(), rng.unitRandom());

	Image filter(f_w, f_h);
	for(unsigned int i=0; i<filter.numPixels(); ++i)
		filter.getPixel(i).set(rng.unitRandom(), rng.unitRandom(), rng.unitRandom());

	// Reference FT convolution
	Image ref_ft_out;
	if(in_w < 32)
		ImageFilter::slowConvolveImageFFT(in, filter, ref_ft_out);
	
	// Fast FT convolution
	t.reset();
	Image fast_ft_out;
	ImageFilter::convolveImageFFT(in, filter, fast_ft_out);
	conPrint("convolveImageFFT: elapsed:          " + t.elapsedString());

	// FFTSS Fast FT convolution
	Image fftss_ft_out;
	FFTPlan plan;
	// Compute once to get plan
	t.reset();
	ImageFilter::convolveImageFFTSS(in, filter, fftss_ft_out, plan);
	conPrint("convolveImageFFTSS plan: elapsed:    " + t.elapsedString());

	// Compute a second time, measuring speed
	t.reset();
	ImageFilter::convolveImageFFTSS(in, filter, fftss_ft_out, plan);
	conPrint("convolveImageFFTSS execute: elapsed: " + t.elapsedString());


	// Spatial convolution
	Image spatial_convolution_out;
	if(in_w < 32)
		ImageFilter::convolveImageSpatial(in, filter, spatial_convolution_out);


	testAssert(fast_ft_out.getWidth() == in.getWidth() && fast_ft_out.getHeight() == in.getHeight());
	testAssert(fftss_ft_out.getWidth() == in.getWidth() && fftss_ft_out.getHeight() == in.getHeight());

	if(in_w < 32)
	{
		testAssert(ref_ft_out.getWidth() == in.getWidth() && ref_ft_out.getHeight() == in.getHeight());
		testAssert(spatial_convolution_out.getWidth() == in.getWidth() && spatial_convolution_out.getHeight() == in.getHeight());
	}

	for(unsigned int i=0; i<fast_ft_out.numPixels(); ++i)
		for(unsigned int comp=0; comp<3; ++comp)
		{
			if(in_w < 32)
			{
				const float ref = ref_ft_out.getPixel(i)[comp];
				const float a = fast_ft_out.getPixel(i)[comp];
				//const float b = spatial_convolution_out.getPixel(i)[comp];
				const float c = fftss_ft_out.getPixel(i)[comp];

				if(!epsEqual(ref, c, 0.0001f))
				{
					printVar(ref);
					printVar(c);
				}
				testAssert(epsEqual(ref, a, 0.0001f));
				
				// NOTE: correspondence between spatial_convolution_out FFT convolution has been broken due to offsetting the filter by one in FFT convolution
				// TEMP testAssert(epsEqual(ref, b, 0.0001f));
				testAssert(epsEqual(ref, c, 0.0001f));
			}
			else
			{
				const float a = fast_ft_out.getPixel(i)[comp];
				const float c = fftss_ft_out.getPixel(i)[comp];

				testAssert(epsEqual(a, c, 0.0001f));
			}
		}
}


static void testFT(int in_w, int in_h)
{
	conPrint("testFT()");
	printVar(in_w);
	printVar(in_h);

	Timer t;

	MTwister rng(1);

	Array2d<double> in(in_w, in_h);
	for(unsigned int i=0; i<in.getHeight() * in.getWidth(); ++i)
		in.getData()[i] = rng.unitRandom();

	// Fast FT convolution
	Array2d<Complexd> fast_ft_out;
	t.reset();
	ImageFilter::realFFT(in, fast_ft_out);
	conPrint("realFFT: " + t.elapsedString());

	// FFTSS FFT
	Array2d<Complexd> fftss_ft_out;
	t.reset();
	ImageFilter::FFTSS_realFFT(in, fftss_ft_out);
	conPrint("FFTSS_realFFT: " + t.elapsedString());


	// Reference FT convolution
	Array2d<Complexd> ref_ft_out;
	t.reset();
	ImageFilter::realFT(in, ref_ft_out);
	conPrint("realFT: " + t.elapsedString());


	testAssert(fast_ft_out.getWidth() == in.getWidth() && fast_ft_out.getHeight() == in.getHeight());
	testAssert(ref_ft_out.getWidth() == in.getWidth() && ref_ft_out.getHeight() == in.getHeight());

	conPrint("--------------Fast FT-----------------");
	for(int y=0; y<in_h; ++y)
	{
		for(int x=0; x<in_w; ++x)
		{
			conPrintStr(" (" + toString(fast_ft_out.elem(x, y).re()) + ", " + toString(fast_ft_out.elem(x, y).im()) + ")" );
		}
		conPrintStr("\n");
	}

	conPrint("--------------FFTSS--------------------");
	for(int y=0; y<in_h; ++y)
	{
		for(int x=0; x<in_w; ++x)
		{
			conPrintStr(" (" + toString(fftss_ft_out.elem(x, y).re()) + ", " + toString(fftss_ft_out.elem(x, y).im()) + ")" );
		}
		conPrintStr("\n");
	}

	conPrint("--------------Reference FT-----------------");
	for(int y=0; y<in_h; ++y)
	{
		for(int x=0; x<in_w; ++x)
		{
			conPrintStr(" (" + toString(ref_ft_out.elem(x, y).re()) + ", " + toString(ref_ft_out.elem(x, y).im()) + ")" );
		}
		conPrintStr("\n");
	}

	for(unsigned int i=0; i<in.getHeight() * in.getWidth(); ++i)
	{
		const Complexd a = ref_ft_out.getData()[i];
		const Complexd b = fftss_ft_out.getData()[i];
		const Complexd c = fast_ft_out.getData()[i];
	
		testAssert(epsEqual(a.re(), b.re()));
		testAssert(epsEqual(a.re(), c.re()));
	}
}


static void performanceTestFT(int in_w, int in_h)
{
	conPrint("performanceTestFT(), " + toString(in_w) + "*" + toString(in_h));

	Timer t;

	MTwister rng(1);

	Array2d<double> in(in_w, in_h);
	for(unsigned int i=0; i<in.getHeight() * in.getWidth(); ++i)
		in.getData()[i] = rng.unitRandom();

	// Fast FT convolution
	Array2d<Complexd> fast_ft_out;
	t.reset();
	ImageFilter::realFFT(in, fast_ft_out);
	conPrint("realFFT: " + t.elapsedString());

	// FFTSS FFT
	Array2d<Complexd> fftss_ft_out;
	t.reset();
	ImageFilter::FFTSS_realFFT(in, fftss_ft_out);
	conPrint("FFTSS_realFFT: " + t.elapsedString());


	for(unsigned int i=0; i<in.getHeight() * in.getWidth(); ++i)
	{
		const Complexd b = fftss_ft_out.getData()[i];
		const Complexd c = fast_ft_out.getData()[i];
		if(!epsEqual(b.re(), c.re()))
		{
			conPrint(toString(b.re()));
			conPrint(toString(c.re()));
		}
		testAssert(epsEqual(b.re(), c.re()));
	}
}


static void testResizeImageWithScale(Reference<Image> im, float pixel_enlargement_factor, const std::string& name, Indigo::TaskManager& task_manager)
{
	printVar(pixel_enlargement_factor);

	Reference<Image> out(new Image(im->getWidth(), im->getHeight()));

	Timer timer;
	ImageFilter::resizeImage(*im, *out, 
		pixel_enlargement_factor,
		// Vec3f(1,1,1),
		0.33f, 
		0.33f,
		task_manager
		);

	conPrint("Resize took " + timer.elapsedString());

	// Check that we didn't introduce INFs or NANs.
	for(size_t i=0; i<im->numPixels(); ++i)
	{
		testAssert(im->getPixel(i).isFinite());
	}

	float in_av_lum = (float)im->averageLuminance();
	float out_av_lum = (float)out->averageLuminance();

	printVar(in_av_lum);
	printVar(out_av_lum);

	out->clampInPlace(0, 1);
	out->gammaCorrect(1 / 2.2);

	Bitmap bmp_out;
	out->copyToBitmap(bmp_out);
	PNGDecoder::write(bmp_out, "scaled_image_" + name + "_" + toString(pixel_enlargement_factor) + ".png");
}


static void testResizeImage()
{
	try
	{

	Indigo::TaskManager task_manager;

	// Test resizing image with white dot in center
	{
		// Make black image with white dot in center
		const size_t W = 1024;
		Reference<Image> im(new Image(W, W));
		im->zero();
		im->setPixel(W/2, W/2, Colour3f(1.0f));

		const std::string name = "white_dot";
		testResizeImageWithScale(im, 0.1f, name, task_manager);
		testResizeImageWithScale(im, 0.5f, name, task_manager);
		testResizeImageWithScale(im, 0.8f, name, task_manager);
		testResizeImageWithScale(im, 1.0f, name, task_manager);
		testResizeImageWithScale(im, 1.2f, name, task_manager);
		testResizeImageWithScale(im, 2.0f, name, task_manager);
		testResizeImageWithScale(im, 10.0f, name, task_manager);
	}

	{
		size_t W = 1024;
		Reference<Image> im(new Image(W, W));
		for(size_t y=0; y<W; ++y)
		for(size_t x=0; x<W; ++x)
		{
			float val = 0.5f;
			im->setPixel(x, y, Colour3f(val));
		}

		const std::string name = "grey";
		testResizeImageWithScale(im, 0.1f, name, task_manager);
		testResizeImageWithScale(im, 0.5f, name, task_manager);
		testResizeImageWithScale(im, 0.8f, name, task_manager);
		testResizeImageWithScale(im, 0.997f, name, task_manager);
		testResizeImageWithScale(im, 1.0f, name, task_manager);
		testResizeImageWithScale(im, 1.003f, name, task_manager);
		testResizeImageWithScale(im, 1.2f, name, task_manager);
		testResizeImageWithScale(im, 2.0f, name, task_manager);
		testResizeImageWithScale(im, 10.0f, name, task_manager);

	}

	{
		Map2DRef map = JPEGDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg");
		Reference<Image> im = map->convertToImage();

		const std::string name = "Italy";
		testResizeImageWithScale(im, 0.1f, name, task_manager);
		testResizeImageWithScale(im, 0.5f, name, task_manager);
		testResizeImageWithScale(im, 0.8f, name, task_manager);
		testResizeImageWithScale(im, 1.0f, name, task_manager);
		testResizeImageWithScale(im, 1.2f, name, task_manager);
		testResizeImageWithScale(im, 2.0f, name, task_manager);
		testResizeImageWithScale(im, 10.0f, name, task_manager);
	}

	/*{
		Map2DRef map = JPEGDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/ny.jpg");
		Reference<Image> im = map->convertToImage();

		const std::string name = "NY";
		testResizeImageWithScale(im, 0.1f, name);
		testResizeImageWithScale(im, 0.5f, name);
		testResizeImageWithScale(im, 0.8f, name);
		testResizeImageWithScale(im, 1.0f, name);
		testResizeImageWithScale(im, 1.2f, name);
		testResizeImageWithScale(im, 2.0f, name);
		testResizeImageWithScale(im, 10.0f, name);
	}*/

	{
		Map2DRef map = PNGDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref.png");
		Reference<Image> im = map->convertToImage();

		// Make black image with white dot in center
		/*const size_t W = 2048;
		im = Reference<Image>(new Image(W, W));
		im->zero();
		im->setPixel(W/2, W/2, Colour3f(1.0f));*/

		const std::string name = "colourchecker";
		testResizeImageWithScale(im, 0.1f, name, task_manager);
		testResizeImageWithScale(im, 0.5f, name, task_manager);
		testResizeImageWithScale(im, 0.8f, name, task_manager);
		testResizeImageWithScale(im, 1.0f, name, task_manager);
		testResizeImageWithScale(im, 1.2f, name, task_manager);
		testResizeImageWithScale(im, 2.0f, name, task_manager);
		testResizeImageWithScale(im, 10.0f, name, task_manager);
	}


	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}
}


static void makeSinImage()
{
	size_t W = 1024;
	Reference<Image> image(new Image(W, W));
	for(size_t y=0; y<W; ++y)
	for(size_t x=0; x<W; ++x)
	{
		float num_cycles = 10.0f;
		float val = std::sin(NICKMATHS_2PIf * x * num_cycles / W) * 0.5f + 0.5f;
		image->setPixel(x, y, Colour3f(val));
	}

	Bitmap bmp_out;
	image->copyToBitmap(bmp_out);
	PNGDecoder::write(bmp_out, "grating.png");
}



static void testLowResConvolve()
{
//	Reference<Map2D> map = EXRDecoder::decode("C:\\art\\indigo\\tests\\sun glare\\sun_glare_test_ssf2.exr");
//	Reference<Map2D> map = EXRDecoder::decode("C:\\art\\indigo\\tests\\sun glare\\antialias_test.exr");
//	Reference<Map2D> map = EXRDecoder::decode("C:\\art\\indigo\\tests\\sun glare\\diffraction_test.exr");
	Reference<Map2D> map = EXRDecoder::decode("C:\\art\\indigo\\tests\\sun glare\\cornellbox_jotero_with_sphere.exr");
	Reference<Image> in = map->convertToImage();

	// Make black image with white dot in center
	/*const size_t W = 1024;
	Reference<Image> in(new Image(W, W));
	in->zero();
	in->setPixel(W/2, W/2, Colour3f(1.0f));*/

	

	//Reference<Map2D> filter_map = EXRDecoder::decode("C:\\art\\indigo\\tests\\sun glare\\diffraction_filter_image.exr");
	Reference<Map2D> filter_map = EXRDecoder::decode("C:\\art\\indigo\\tests\\sun glare\\circular_diffraction_filter_image.exr");

	Reference<Image> filter = filter_map->convertToImage();

	printVar(filter->getWidth());
	printVar(filter->getHeight());

	Image out;

	Indigo::TaskManager task_manager;

	ImageFilter::lowResConvolve(*in, *filter, 
		1, // ssf
		out,
		task_manager
	);
}


static void plotImageProfileAlongScanline(const std::string& path, float y)
{
	Reference<Map2D> map = EXRDecoder::decode(path);

	size_t W = map->getMapWidth();

	Reference<Image> image(new Image(W, W));
	for(size_t x=0; x<W; ++x)
	{
		float val = map->scalarSampleTiled((float)x/W, y);

		float splat_y = W*0.5f - val * (W * 0.5f);

		image->setPixel(x, (size_t)splat_y, Colour3f(1.0f));
	}

	Bitmap bmp_out;
	image->copyToBitmap(bmp_out);
	PNGDecoder::write(bmp_out, "scanline_profile.png");
}


void ImageFilter::test()
{
	conPrint("ImageFilter::test()");

	//plotImageProfileAlongScanline("C:\\programming\\models\\SGR bump problem\\MS_G1W3.exr", 0.25f);
	//return;

	//testLowResConvolve();
	//return;

	/*Map2DRef map = JPEGDecoder::decode("C:\\art\\indigo\\thomas_GH_house\\thething_lightlayers.jpg");
	Reference<Image> im = map->convertToImage();

	const std::string name = "colourchecker";
	const float scale = 480.f / 2208.f;
	testResizeImageWithScale(im, scale, name);

	exit(0);*/

	// makeSinImage();

	//testResizeImage();
	//return;

	// exit(0);

	//performanceTestFT(1024, 1024);

	/*exit(0);

	testFT(4, 4);

	

	testFT(8, 8);

	testFT(16, 16);

	testFT(8, 16);

	testFT(16, 4);*/

	testConvolutionWithDims(4, 4, 4, 4);
	testConvolutionWithDims(2, 2, 2, 2);

	testConvolutionWithDims(3, 3, 3, 3);

	testConvolutionWithDims(16, 16, 16, 16);

	testConvolutionWithDims(19, 6, 5, 3);

#ifndef DEBUG
	testConvolutionWithDims(32, 32, 6, 6);
#endif

	testConvolutionWithDims(16, 16, 7, 7);
	
	//testConvolutionWithDims(1024, 1024, 1025, 1025);
	testConvolutionWithDims(1024, 1024, 1024, 1024);

	//testConvolutionWithDims(2048, 2048, 1025, 1025);

	//exit(1);
}


#endif // BUILD_TESTS
