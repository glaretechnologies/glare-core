/*=====================================================================
GaussianImageFilter.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-08-16 17:22:50 +0100
=====================================================================*/
#include "GaussianImageFilter.h"


#include "ImageMap.h"
#include "image.h"
#include "../utils/TaskManager.h"
#include "../utils/Task.h"


/*
struct GaussianImageFilterTaskClosure
{
	GaussianImageFilterTaskClosure(Image& temp_, const Image& in_, Image& out_) : temp(temp_), in(in_), out(out_) {}

	Image& temp;
	const Image& in;
	Image& out;
	const float* filter_weights;
	int pixel_rad;
};


class XBlurTask : public Indigo::Task
{
public:
	XBlurTask(const GaussianImageFilterTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		const int pixel_rad = closure.pixel_rad;
		const int w = (int)closure.in.getWidth();
		const float* const filter_weights = closure.filter_weights;
		const Image& in = closure.in;
		Image& temp = closure.temp;

		for(int dy = begin; dy < end; ++dy)
			for(int dx = 0; dx < w; ++dx)
			{
				Image::ColourType c(0);

				const int start_x = dx - pixel_rad;
				const int end_x = dx + pixel_rad + 1;

				// Do input off left side of image: x < 0, so x is equal (mod w) to w + x
				for(int x = start_x; x < 0; ++x)
					c.addMult(in.getPixel(w + x, dy), filter_weights[x - start_x]);

				// Do 0 <= x < w
				for(int x = myMax(0, start_x); x < myMin(w, end_x); ++x)
					c.addMult(in.getPixel(x, dy), filter_weights[x - start_x]);

				// Do w < x, so x is equal (mod w) to x - w
				for(int x = w; x < end_x; ++x)
					c.addMult(in.getPixel(x - w, dy), filter_weights[x - start_x]);

				temp.setPixel(dx, dy, c);
			}
	}

	const GaussianImageFilterTaskClosure& closure;
	int begin, end;
};


class YBlurTask : public Indigo::Task
{
public:
	YBlurTask(const GaussianImageFilterTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		const int pixel_rad = closure.pixel_rad;
		const int h = (int)closure.in.getHeight();
		const int w = (int)closure.in.getWidth();
		const float* const filter_weights = closure.filter_weights;
		const Image& temp = closure.temp;
		Image& out = closure.out;

		for(int dy = begin; dy < end; ++dy)
			for(int dx = 0; dx < w; ++dx)
			{
				Image::ColourType c(0);
				
				const int start_y = dy - pixel_rad;
				const int end_y = dy + pixel_rad + 1;

				// y < 0
				for(int y = start_y; y<0; ++y)
					c.addMult(temp.getPixel(dx, h + y), filter_weights[y - start_y]);

				// Do 0 <= y < h
				for(int y = myMax(0, start_y); y < myMin(h, end_y); ++y)
					c.addMult(temp.getPixel(dx, y), filter_weights[y - start_y]);

				// Do h < y
				for(int y = h; y < end_y; ++y)
					c.addMult(temp.getPixel(dx, y - h), filter_weights[y - start_y]);

				out.setPixel(dx, dy, c);
			}
	}

	const GaussianImageFilterTaskClosure& closure;
	int begin, end;
};


void GaussianImageFilter::gaussianFilter(const Image& in, Image& out, float standard_deviation, Indigo::TaskManager& task_manager)
{
	assert(in.getHeight() == out.getHeight() && in.getWidth() == out.getWidth());

	const double rad_needed = Maths::inverse1DGaussian(0.00001f, standard_deviation);

	//const double z = Maths::eval1DGaussian(rad_needed, standard_deviation);

	const int pixel_rad = myClamp<int>(
		(int)rad_needed,
		1, // lower bound
		myMin((int)in.getWidth() - 1, (int)in.getHeight() - 1) // upper bound
		);

	//conPrint("gaussianFilter: using pixel radius of " + toString(pixel_rad));


	const int lookup_size = pixel_rad + pixel_rad + 1;
	// Build filter lookup table
	float* filter_weights = new float[lookup_size];
	for(int x = 0; x < lookup_size; ++x)
	{
		const float dist = (float)x - (float)pixel_rad;
		filter_weights[x] = (float)Maths::eval1DGaussian(dist, standard_deviation);
		if(filter_weights[x] < 1.0e-12f)
			filter_weights[x] = 0.0f;
	}

	// Normalise filter kernel
	float sumweight = 0.0f;
	for(int y = 0; y < lookup_size; ++y)
		for(int x = 0; x < lookup_size; ++x)
			sumweight += filter_weights[x]*filter_weights[y];

	for(int x = 0; x < lookup_size; ++x)
		filter_weights[x] *= sqrt(1.0f / sumweight);

	// Check weights are properly normalised now
	sumweight = 0.0f;
	for(int y = 0; y < lookup_size; ++y)
		for(int x = 0; x < lookup_size; ++x)
			sumweight += filter_weights[x]*filter_weights[y];

	assert(::epsEqual(sumweight, 1.0f));

	//------------------------------------------------------------------------
	// Do separable blur
	//------------------------------------------------------------------------
	Image temp(in.getWidth(), in.getHeight());

	GaussianImageFilterTaskClosure closure(temp, in, out);
	closure.filter_weights = filter_weights;
	closure.pixel_rad = pixel_rad;

	// Blur in x direction, reading from 'in' and writing to 'temp'.
	const int h = (int)in.getHeight();
	task_manager.runParallelForTasks<XBlurTask, GaussianImageFilterTaskClosure>(closure, 0, h);

	// Blur in y direction, reading from 'temp' and writing to 'out'.
	task_manager.runParallelForTasks<YBlurTask, GaussianImageFilterTaskClosure>(closure, 0, h);

	delete[] filter_weights;
}



struct GaussianImageFilterTaskClosure
{
	GaussianImageFilterTaskClosure(Image& temp_, const Image& in_, Image& out_) : temp(temp_), in(in_), out(out_) {}

	Image& temp;
	const Image& in;
	Image& out;
	const float* filter_weights;
	int pixel_rad;
};


class XBlurTask : public Indigo::Task
{
public:
	XBlurTask(const GaussianImageFilterTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		const int pixel_rad = closure.pixel_rad;
		const int w = (int)closure.in.getWidth();
		const float* const filter_weights = closure.filter_weights;
		const Image& in = closure.in;
		Image& temp = closure.temp;

		for(int dy = begin; dy < end; ++dy)
			for(int dx = 0; dx < w; ++dx)
			{
				Image::ColourType c(0);

				const int start_x = dx - pixel_rad;
				const int end_x = dx + pixel_rad + 1;

				// Do input off left side of image: x < 0, so x is equal (mod w) to w + x
				for(int x = start_x; x < 0; ++x)
					c.addMult(in.getPixel(w + x, dy), filter_weights[x - start_x]);

				// Do 0 <= x < w
				for(int x = myMax(0, start_x); x < myMin(w, end_x); ++x)
					c.addMult(in.getPixel(x, dy), filter_weights[x - start_x]);

				// Do w < x, so x is equal (mod w) to x - w
				for(int x = w; x < end_x; ++x)
					c.addMult(in.getPixel(x - w, dy), filter_weights[x - start_x]);

				temp.setPixel(dx, dy, c);
			}
	}

	const GaussianImageFilterTaskClosure& closure;
	int begin, end;
};


class YBlurTask : public Indigo::Task
{
public:
	YBlurTask(const GaussianImageFilterTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		const int pixel_rad = closure.pixel_rad;
		const int h = (int)closure.in.getHeight();
		const int w = (int)closure.in.getWidth();
		const float* const filter_weights = closure.filter_weights;
		const Image& temp = closure.temp;
		Image& out = closure.out;

		for(int dy = begin; dy < end; ++dy)
			for(int dx = 0; dx < w; ++dx)
			{
				Image::ColourType c(0);
				
				const int start_y = dy - pixel_rad;
				const int end_y = dy + pixel_rad + 1;

				// y < 0
				for(int y = start_y; y<0; ++y)
					c.addMult(temp.getPixel(dx, h + y), filter_weights[y - start_y]);

				// Do 0 <= y < h
				for(int y = myMax(0, start_y); y < myMin(h, end_y); ++y)
					c.addMult(temp.getPixel(dx, y), filter_weights[y - start_y]);

				// Do h < y
				for(int y = h; y < end_y; ++y)
					c.addMult(temp.getPixel(dx, y - h), filter_weights[y - start_y]);

				out.setPixel(dx, dy, c);
			}
	}

	const GaussianImageFilterTaskClosure& closure;
	int begin, end;
};


void GaussianImageFilter::gaussianFilter(const Image& in, Image& out, float standard_deviation, Indigo::TaskManager& task_manager)
{
	assert(in.getHeight() == out.getHeight() && in.getWidth() == out.getWidth());

	const double rad_needed = Maths::inverse1DGaussian(0.00001f, standard_deviation);

	//const double z = Maths::eval1DGaussian(rad_needed, standard_deviation);

	const int pixel_rad = myClamp<int>(
		(int)rad_needed,
		1, // lower bound
		myMin((int)in.getWidth() - 1, (int)in.getHeight() - 1) // upper bound
		);

	//conPrint("gaussianFilter: using pixel radius of " + toString(pixel_rad));


	const int lookup_size = pixel_rad + pixel_rad + 1;
	// Build filter lookup table
	float* filter_weights = new float[lookup_size];
	for(int x = 0; x < lookup_size; ++x)
	{
		const float dist = (float)x - (float)pixel_rad;
		filter_weights[x] = (float)Maths::eval1DGaussian(dist, standard_deviation);
		if(filter_weights[x] < 1.0e-12f)
			filter_weights[x] = 0.0f;
	}

	// Normalise filter kernel
	float sumweight = 0.0f;
	for(int y = 0; y < lookup_size; ++y)
		for(int x = 0; x < lookup_size; ++x)
			sumweight += filter_weights[x]*filter_weights[y];

	for(int x = 0; x < lookup_size; ++x)
		filter_weights[x] *= sqrt(1.0f / sumweight);

	// Check weights are properly normalised now
	sumweight = 0.0f;
	for(int y = 0; y < lookup_size; ++y)
		for(int x = 0; x < lookup_size; ++x)
			sumweight += filter_weights[x]*filter_weights[y];

	assert(::epsEqual(sumweight, 1.0f));

	//------------------------------------------------------------------------
	// Do separable blur
	//------------------------------------------------------------------------
	Image temp(in.getWidth(), in.getHeight());

	GaussianImageFilterTaskClosure closure(temp, in, out);
	closure.filter_weights = filter_weights;
	closure.pixel_rad = pixel_rad;

	// Blur in x direction, reading from 'in' and writing to 'temp'.
	const int h = (int)in.getHeight();
	task_manager.runParallelForTasks<XBlurTask, GaussianImageFilterTaskClosure>(closure, 0, h);

	// Blur in y direction, reading from 'temp' and writing to 'out'.
	task_manager.runParallelForTasks<YBlurTask, GaussianImageFilterTaskClosure>(closure, 0, h);

	delete[] filter_weights;
}
*/


//=================================================


struct GaussianImageFilterTaskClosure
{
	GaussianImageFilterTaskClosure(ImageMapFloat& temp_, const ImageMapFloat& in_, ImageMapFloat& out_) : temp(temp_), in(in_), out(out_) {}

	ImageMapFloat& temp;
	const ImageMapFloat& in;
	ImageMapFloat& out;
	const float* filter_weights;
	int pixel_rad;
};


class XBlurTask : public Indigo::Task
{
public:
	XBlurTask(const GaussianImageFilterTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		const int pixel_rad = closure.pixel_rad;
		const int w = (int)closure.in.getWidth();
		const float* const filter_weights = closure.filter_weights;
		const ImageMapFloat& in = closure.in;
		ImageMapFloat& temp = closure.temp;

		for(int dy = begin; dy < end; ++dy)
			for(int dx = 0; dx < w; ++dx)
			{
				float c = 0;

				const int start_x = dx - pixel_rad;
				const int end_x = dx + pixel_rad + 1;

				// Do input off left side of image: x < 0, so x is equal (mod w) to w + x
				for(int x = start_x; x < 0; ++x)
					c += in.getPixel(w + x, dy)[0] * filter_weights[x - start_x];

				// Do 0 <= x < w
				for(int x = myMax(0, start_x); x < myMin(w, end_x); ++x)
					c += in.getPixel(x, dy)[0] * filter_weights[x - start_x];

				// Do w < x, so x is equal (mod w) to x - w
				for(int x = w; x < end_x; ++x)
					c += in.getPixel(x - w, dy)[0] * filter_weights[x - start_x];

				temp.getPixel(dx, dy)[0] = c;
			}
	}

	const GaussianImageFilterTaskClosure& closure;
	int begin, end;
};


class YBlurTask : public Indigo::Task
{
public:
	YBlurTask(const GaussianImageFilterTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		const int pixel_rad = closure.pixel_rad;
		const int h = (int)closure.in.getHeight();
		const int w = (int)closure.in.getWidth();
		const float* const filter_weights = closure.filter_weights;
		const ImageMapFloat& temp = closure.temp;
		ImageMapFloat& out = closure.out;

		for(int dy = begin; dy < end; ++dy)
			for(int dx = 0; dx < w; ++dx)
			{
				float c = 0;

				const int start_y = dy - pixel_rad;
				const int end_y = dy + pixel_rad + 1;

				// y < 0
				for(int y = start_y; y<0; ++y)
					c += temp.getPixel(dx, h + y)[0] * filter_weights[y - start_y];

				// Do 0 <= y < h
				for(int y = myMax(0, start_y); y < myMin(h, end_y); ++y)
					c += temp.getPixel(dx, y)[0] * filter_weights[y - start_y];

				// Do h < y
				for(int y = h; y < end_y; ++y)
					c += temp.getPixel(dx, y - h)[0] * filter_weights[y - start_y];

				out.getPixel(dx, dy)[0] = c;
			}
	}

	const GaussianImageFilterTaskClosure& closure;
	int begin, end;
};


void GaussianImageFilter::gaussianFilter(const ImageMapFloat& in, ImageMapFloat& out, float standard_deviation, Indigo::TaskManager& task_manager)
{
	assert(in.getN() == 1 && out.getN() == 1);
	assert(in.getHeight() == out.getHeight() && in.getWidth() == out.getWidth());

	const double rad_needed = Maths::inverse1DGaussian(0.00001f, standard_deviation);

	//const double z = Maths::eval1DGaussian(rad_needed, standard_deviation);

	const int pixel_rad = myClamp<int>(
		(int)rad_needed,
		1, // lower bound
		myMin((int)in.getWidth() - 1, (int)in.getHeight() - 1) // upper bound
		);

	//conPrint("gaussianFilter: using pixel radius of " + toString(pixel_rad));


	const int lookup_size = pixel_rad + pixel_rad + 1;
	// Build filter lookup table
	float* filter_weights = new float[lookup_size];
	for(int x = 0; x < lookup_size; ++x)
	{
		const float dist = (float)x - (float)pixel_rad;
		filter_weights[x] = (float)Maths::eval1DGaussian(dist, standard_deviation);
		if(filter_weights[x] < 1.0e-12f)
			filter_weights[x] = 0.0f;
	}

	// Normalise filter kernel
	float sumweight = 0.0f;
	for(int y = 0; y < lookup_size; ++y)
		for(int x = 0; x < lookup_size; ++x)
			sumweight += filter_weights[x]*filter_weights[y];

	for(int x = 0; x < lookup_size; ++x)
		filter_weights[x] *= sqrt(1.0f / sumweight);

	// Check weights are properly normalised now
	sumweight = 0.0f;
	for(int y = 0; y < lookup_size; ++y)
		for(int x = 0; x < lookup_size; ++x)
			sumweight += filter_weights[x]*filter_weights[y];

	assert(::epsEqual(sumweight, 1.0f));

	//------------------------------------------------------------------------
	// Do separable blur
	//------------------------------------------------------------------------
	ImageMapFloat temp(in.getWidth(), in.getHeight(), 1);

	GaussianImageFilterTaskClosure closure(temp, in, out);
	closure.filter_weights = filter_weights;
	closure.pixel_rad = pixel_rad;

	// Blur in x direction, reading from 'in' and writing to 'temp'.
	const int h = (int)in.getHeight();
	task_manager.runParallelForTasks<XBlurTask, GaussianImageFilterTaskClosure>(closure, 0, h);

	// Blur in y direction, reading from 'temp' and writing to 'out'.
	task_manager.runParallelForTasks<YBlurTask, GaussianImageFilterTaskClosure>(closure, 0, h);

	delete[] filter_weights;
}


#if BUILD_TESTS


#include "jpegdecoder.h"
#include "PNGDecoder.h"
#include "bitmap.h"
#include "imformatdecoder.h"
#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"


void GaussianImageFilter::test()
{
	/*try
	{
		// Load JPEG, convert to Image
		Map2DRef map = JPEGDecoder::decode(".", TestUtils::getIndigoTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg");
		//Reference<Image> im = map->convertToImage();

		// Gaussian blur it
		Indigo::TaskManager task_manager;

		Timer timer;

		Reference<Image> blurred = new Image(im->getWidth(), im->getHeight());
		gaussianFilter(*im, *blurred, 
			2.0f, // std dev (pixels)
			task_manager
		);

		conPrint("Blur took " + timer.elapsedString());
		conPrint("Average luminance: " + toString(blurred->averageLuminance()));

		if(false)
		{
			// Save it to disk
			blurred->clampInPlace(0, 1);
			blurred->gammaCorrect(1 / 2.2);
			Bitmap bmp_out;
			blurred->copyToBitmap(bmp_out);
			PNGDecoder::write(bmp_out, "blurred.png");
		}
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}*/
}


#endif // BUILD_TESTS
