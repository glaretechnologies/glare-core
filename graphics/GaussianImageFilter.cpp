/*=====================================================================
GaussianImageFilter.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-08-16 17:22:50 +0100
=====================================================================*/
#include "GaussianImageFilter.h"


#include "image.h"
#include "../utils/TaskManager.h"
#include "../utils/Task.h"


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
		int pixel_rad = closure.pixel_rad;
		
		const int h = (int)closure.in.getHeight();
		const int w = (int)closure.in.getWidth();

		for(int dy = begin; dy < end; ++dy)
			for(int dx = 0; dx < w; ++dx)
			{
				Image::ColourType c(0);

				const int start_x = dx - pixel_rad;
				const int end_x = dx + pixel_rad + 1;

				// Do input off left side of image: x < 0, so x is equal (mod w) to w + x
				for(int x = start_x; x < 0; ++x)
					c.addMult(closure.in.getPixel(w + x, dy), closure.filter_weights[x - start_x]);

				// Do 0 <= x < w
				for(int x = myMax(0, start_x); x < myMin(w, end_x); ++x)
					c.addMult(closure.in.getPixel(x, dy), closure.filter_weights[x - start_x]);

				// Do w < x, so x is equal (mod w) to x - w
				for(int x = w; x < end_x; ++x)
					c.addMult(closure.in.getPixel(x - w, dy), closure.filter_weights[x - start_x]);

				closure.temp.setPixel(dx, dy, c);
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
		int pixel_rad = closure.pixel_rad;
		
		const int h = (int)closure.in.getHeight();
		const int w = (int)closure.in.getWidth();

		for(int dy = 0; dy < h; ++dy)
			for(int dx = 0; dx < w; ++dx)
			{
				Image::ColourType c(0);

				const int start_y = dy - pixel_rad;
				const int end_y = dy + pixel_rad + 1;

				// y < 0
				for(int y = start_y; y<0; ++y)
					c.addMult(closure.temp.getPixel(dx, h + y), closure.filter_weights[y - start_y]);

				// Do 0 <= y < h
				for(int y = myMax(0, start_y); y < myMin(h, end_y); ++y)
					c.addMult(closure.temp.getPixel(dx, y), closure.filter_weights[y - start_y]);

				// Do h < y
				for(int y = h; y < end_y; ++y)
					c.addMult(closure.temp.getPixel(dx, y - h), closure.filter_weights[y - start_y]);

				closure.out.setPixel(dx, dy, c);
			}
	}

	const GaussianImageFilterTaskClosure& closure;
	int begin, end;
};


void GaussianImageFilter::gaussianFilter(const Image& in, Image& out, float standard_deviation, Indigo::TaskManager& task_manager)
{
	assert(in.getHeight() == out.getHeight() && in.getWidth() == out.getWidth());

	const double rad_needed = Maths::inverse1DGaussian(0.00001f, standard_deviation);

	const double z = Maths::eval1DGaussian(rad_needed, standard_deviation);

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

	//normalise filter kernel
	float sumweight = 0.0f;
	for(int y = 0; y < lookup_size; ++y)
		for(int x = 0; x < lookup_size; ++x)
			sumweight += filter_weights[x]*filter_weights[y];

	for(int x = 0; x < lookup_size; ++x)
		filter_weights[x] *= sqrt(1.0f / sumweight);

	//check weights are properly normalised now
	sumweight = 0.0f;
	for(int y = 0; y < lookup_size; ++y)
		for(int x = 0; x < lookup_size; ++x)
			sumweight += filter_weights[x]*filter_weights[y];

	assert(::epsEqual(sumweight, 1.0f));

	//------------------------------------------------------------------------
	//try separable blur
	//------------------------------------------------------------------------
	//blur in x direction
	Image temp(in.getWidth(), in.getHeight());
	//temp.zero();

	const int h = (int)in.getHeight();
	const int w = (int)in.getWidth();

/*#ifndef INDIGO_NO_OPENMP
#pragma omp parallel for
#endif
	for(int dy = 0; dy < h; ++dy)
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
*/

	GaussianImageFilterTaskClosure closure(temp, in, out);
	closure.filter_weights = filter_weights;
	closure.pixel_rad = pixel_rad;

	// Blur in x direction
	task_manager.runParallelForTasks<XBlurTask, GaussianImageFilterTaskClosure>(closure, 0, h);

	// Blur in y direction
	task_manager.runParallelForTasks<YBlurTask, GaussianImageFilterTaskClosure>(closure, 0, h);

/*#ifndef INDIGO_NO_OPENMP
#pragma omp parallel for
#endif
		for(int dy = 0; dy < h; ++dy)
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
*/

			/*for(int y=0; y<in.getHeight(); ++y)
			for(int x=0; x<in.getWidth(); ++x)
			{
			//const int minx = myMax(0, x - pixel_rad);
			//const int maxx = myMin((int)in.getWidth(), x + pixel_rad + 1);
			const int minx = x - pixel_rad;
			const int maxx = x + pixel_rad + 1;

			const Image::ColourType incol = in.getPixel(x, y);

			//int weightindex = minx - x + pixel_rad;
			for(int tx=minx; tx<maxx; ++tx)
			{
			//const int dx = tx - x + pixel_rad;
			//assert(dx >= 0 && dx < lookup_size);

			temp.getPixel(tx, y).addMult(incol, filter_weights[tx - x + pixel_rad]);
			}
			}

			//temp is now in blurred in x direction
			//temp;

			//blur in y direction
			for(int y=0; y<in.getHeight(); ++y)
			for(int x=0; x<in.getWidth(); ++x)
			{
			const int miny = myMax(0, y - pixel_rad);
			const int maxy = myMin((int)in.getHeight(), y + pixel_rad + 1);

			const Image::ColourType incol = temp.getPixel(x, y);

			//int weightindex = miny - y + pixel_rad;
			for(int ty=miny; ty<maxy; ++ty)
			{
			//const int dy = ty - y + pixel_rad;
			//assert(dy >= 0 && dy < lookup_size);

			out.getPixel(x, ty).addMult(incol, filter_weights[ty - y + pixel_rad]);
			}
			}*/

			/*//for each pixel in the source image
			for(int y=0; y<in.getHeight(); ++y)
			{
			//get min and max of current filter rect along y axis
			const int miny = myMax(0, y - pixel_rad);
			const int maxy = myMin(in.getHeight(), y + pixel_rad + 1);

			for(int x=0; x<in.getWidth(); ++x)
			{
			//get min and max of current filter rect along x axis
			const int minx = myMax(0, x - pixel_rad);
			const int maxx = myMin(in.getWidth(), x + pixel_rad + 1);

			//for each pixel in the out image, in the filter radius
			for(int ty=miny; ty<maxy; ++ty)
			for(int tx=minx; tx<maxx; ++tx)
			{
			const int dx = tx - x + pixel_rad;
			const int dy = ty - y + pixel_rad;
			assert(dx >= 0 && dx < lookup_size);
			assert(dy >= 0 && dy < lookup_size);
			//printVar(dx);
			//printVar(dy);
			const float factor = filter_weights[dx]*filter_weights[dy];

			out.getPixel(tx, ty).addMult(in.getPixel(x, y), factor);
			}

			}
			}*/

	delete[] filter_weights;
}
