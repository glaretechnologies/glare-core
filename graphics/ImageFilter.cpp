/*=====================================================================
ImageFilter.cpp
---------------
File created by ClassTemplate on Sat Aug 05 19:05:41 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "ImageFilter.h"

#include "image.h"
#include "../utils/array2d.h"
#include "../utils/stringutils.h"
#include "../indigo/globals.h"
#include "../maths/vec2.h"
#include "../maths/Matrix2.h"
//#include "../utils/timer.h"
#include "../maths/mathstypes.h"


ImageFilter::ImageFilter()
{
	
}


ImageFilter::~ImageFilter()
{
	
}


void ImageFilter::gaussianFilter(const Image& in, Image& out, float standard_deviation/*, int kernel_radius*/)
{
	assert(in.getHeight() == out.getHeight() && in.getWidth() == out.getWidth());

	const double rad_needed = Maths::inverse1DGaussian(0.00001f, standard_deviation);

	const double z = Maths::eval1DGaussian(rad_needed, standard_deviation);

	const int pixel_rad = myMax(1, (int)rad_needed);//kernel_radius;

	//conPrint("gaussianFilter: using pixel radius of " + toString(pixel_rad));




	const int lookup_size = pixel_rad + pixel_rad + 1;
	//build filter lookup table
	float* filter_weights = new float[lookup_size];
	//float filter_weights[10];
	for(int x=0; x<lookup_size; ++x)
	{
		const float dist = (float)x - (float)pixel_rad;
		filter_weights[x] = Maths::eval1DGaussian(dist, standard_deviation);
		//printVar(x);
		//printVar(filter_weights[x]);
		if(filter_weights[x] < 1.0e-12f)
			filter_weights[x] = 0.0f;
	}

	//normalise filter kernel
	float sumweight = 0.0f;
	for(int y=0; y<lookup_size; ++y)
		for(int x=0; x<lookup_size; ++x)
			sumweight += filter_weights[x]*filter_weights[y];

	for(int x=0; x<lookup_size; ++x)
		filter_weights[x] *= sqrt(1.0f / sumweight);

	//check weights are properly normalised now
	sumweight = 0.0f;
	for(int y=0; y<lookup_size; ++y)
		for(int x=0; x<lookup_size; ++x)
			sumweight += filter_weights[x]*filter_weights[y];

	assert(::epsEqual(sumweight, 1.0f));

	//------------------------------------------------------------------------
	//try separable blur
	//------------------------------------------------------------------------
	//blur in x direction
	Image temp(in.getWidth(), in.getHeight());
	temp.zero();

	for(int y=0; y<in.getHeight(); ++y)
		for(int x=0; x<in.getWidth(); ++x)
		{
			const int minx = myMax(0, x - pixel_rad);
			const int maxx = myMin(in.getWidth(), x + pixel_rad + 1);

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
			const int maxy = myMin(in.getHeight(), y + pixel_rad + 1);

			const Image::ColourType incol = temp.getPixel(x, y);

			//int weightindex = miny - y + pixel_rad;
			for(int ty=miny; ty<maxy; ++ty)
			{
				//const int dy = ty - y + pixel_rad;
				//assert(dy >= 0 && dy < lookup_size);

				out.getPixel(x, ty).addMult(incol, filter_weights[ty - y + pixel_rad]);
			}
		}

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




static void horizontalGaussianBlur(const Image& in, Image& out, float standard_deviation)
{
	assert(standard_deviation > 0.f);
	out.resize(in.getWidth(), in.getHeight());

	const float rad_needed = Maths::inverse1DGaussian(0.00001f, standard_deviation);

	const float z = Maths::eval1DGaussian(rad_needed, standard_deviation);

	const int pixel_rad = myMax(1, (int)rad_needed);//kernel_radius;

	const int lookup_size = pixel_rad + pixel_rad + 1;
	//build filter lookup table
	float* filter_weights = new float[lookup_size];
	for(int x=0; x<lookup_size; ++x)
	{
		const float dist = (float)x - (float)pixel_rad;
		filter_weights[x] = Maths::eval1DGaussian(dist, standard_deviation);
		if(filter_weights[x] < 1.0e-12f)
			filter_weights[x] = 0.0f;
	}

	//normalise filter kernel
	float sumweight = 0.0f;
	for(int x=0; x<lookup_size; ++x)
		sumweight += filter_weights[x];

	for(int x=0; x<lookup_size; ++x)
		filter_weights[x] *= 1.0f / sumweight;

	//check weights are properly normalised now
	sumweight = 0.0f;
	for(int x=0; x<lookup_size; ++x)
		sumweight += filter_weights[x];

	assert(::epsEqual(sumweight, 1.0f));

	//------------------------------------------------------------------------
	//blur in x direction
	//------------------------------------------------------------------------
	out.zero();

	for(int y=0; y<in.getHeight(); ++y)
		for(int x=0; x<in.getWidth(); ++x)
		{
			const int minx = myMax(0, x - pixel_rad);
			const int maxx = myMin(in.getWidth(), x + pixel_rad + 1);

			const Image::ColourType incol = in.getPixel(x, y);

			//int weightindex = minx - x + pixel_rad;
			for(int tx=minx; tx<maxx; ++tx)
			{
				//const int dx = tx - x + pixel_rad;
				//assert(dx >= 0 && dx < lookup_size);

				out.getPixel(tx, y).addMult(incol, filter_weights[tx - x + pixel_rad]);
			}
	}

	delete[] filter_weights;
}



#if 0
void ImageFilter::gaussianFilter(const Image& in, Image& out, float standard_deviation, int kernel_radius)
{ 
	assert(in.getHeight() == out.getHeight() && in.getWidth() == out.getWidth());

	const int pixel_rad = kernel_radius;
	const int lookup_size = pixel_rad + pixel_rad + 1;
	//build filter lookup table
	float* filter_weights = new float[lookup_size];
	//float filter_weights[10];
	for(int x=0; x<lookup_size; ++x)
	{
		const float dist = (float)x - (float)pixel_rad;
		filter_weights[x] = Maths::eval1DGaussian(dist, standard_deviation);
//		printVar(x);
		//printVar(filter_weights[x]);
		if(filter_weights[x] < 1.0e-12f)
			filter_weights[x] = 0.0f;
	}

		//normalise filter kernel
	float sumweight = 0.0f;
	for(int y=0; y<lookup_size; ++y)
		for(int x=0; x<lookup_size; ++x)
			sumweight += filter_weights[x]*filter_weights[y];

	for(int x=0; x<lookup_size; ++x)
		filter_weights[x] *= sqrt(1.0f / sumweight);

	sumweight = 0.0f;
	for(int y=0; y<lookup_size; ++y)
		for(int x=0; x<lookup_size; ++x)
			sumweight += filter_weights[x]*filter_weights[y];

	assert(::epsEqual(sumweight, 1.0f));

	//for each pixel in the source image
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
	}

	delete[] filter_weights;
}

#endif

void ImageFilter::chiuFilter(const Image& in, Image& out, float radius, bool include_center)
{
	assert(in.getHeight() == out.getHeight() && in.getWidth() == out.getWidth());

	const int pixel_rad = (int)radius;// + 1;
	const int lookup_size = pixel_rad + pixel_rad + 1;
	//build filter lookup table
	//std::vector<float> weights(lookup_size * lookup_size);
	Array2d<float> weights(lookup_size, lookup_size);
	
	//float weights[lookup_size][lookup_size];
	for(int y=0; y<lookup_size; ++y)
		for(int x=0; x<lookup_size; ++x)
		{
			if(x == pixel_rad && y == pixel_rad)
			{
				weights.elem(x, y) = include_center ? 1.0f : 0.0f;
			}
			else
			{
				const float dx = (float)(x - pixel_rad);
				const float dy = (float)(y - pixel_rad);
				const float dist = sqrt(dx*dx + dy*dy);
				const float weight = pow(myMax(0.0f, 1.0f - dist / radius), 4.0f);
				weights.elem(x, y) = weight;
			}
		}

	float sumweight = 0.0f;
	for(int y=0; y<lookup_size; ++y)
		for(int x=0; x<lookup_size; ++x)
			sumweight += weights.elem(x, y);

	//normalise filter kernel
	for(int y=0; y<lookup_size; ++y)
		for(int x=0; x<lookup_size; ++x)
		{
			weights.elem(x, y) /= sumweight;
			//conPrint("weights[" + toString(x) + "][" + toString(y) + "]: " + toString(weights.elem(x, y)));
		}

	//check is normalised correctly
	sumweight = 0.0f;
	for(int y=0; y<lookup_size; ++y)
		for(int x=0; x<lookup_size; ++x)
			sumweight += weights.elem(x, y);

	assert(::epsEqual(sumweight, 1.0f));

	//for each pixel in the source image
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
					const int dx=x-tx+pixel_rad;
					const int dy=y-ty+pixel_rad;
					assert(dx >= 0 && dx < lookup_size);
					assert(dy >= 0 && dy < lookup_size);
					const float factor = weights.elem(dx, dy);

					out.getPixel(tx, ty).addMult(in.getPixel(x, y), factor);
				}

		}
	}
}


static const Image::ColourType bilinearSampleImage(const Image& im, const Vec2f& pos)
{
	//const Vec2 pos(normed_pos.x * (float)im.getWidth(), normed_pos.y * (float)im.getHeight());

	const int min_x = myMax(0, (int)floor(pos.x));
	const int max_x = myMin(min_x+1, (int)im.getWidth()-1);

	const int min_y = myMax(0, (int)floor(pos.y));
	const int max_y = myMin(min_y+1, (int)im.getHeight()-1);

	Image::ColourType sum(0,0,0);
	for(int y=min_y; y<=max_y; ++y)
	{
		const float d_y = pos.y - (float)y;
		const float y_fac = myMax(0.f, 1.f - (float)fabs(d_y));

		for(int x=min_x; x<=max_x; ++x)
		{
			const float d_x = pos.x - (float)x;
			const float x_fac = myMax(0.f, 1.f - (float)fabs(d_x));

			sum.addMult(im.getPixel(x, y), x_fac * y_fac);
		}
	}
	return sum;
}


void ImageFilter::chromaticAberration(const Image& in, Image& out, float amount)
{
	assert(in.getHeight() == out.getHeight() && in.getWidth() == out.getWidth());
	out.zero();

	for(int y=0; y<out.getHeight(); ++y)
	{
		for(int x=0; x<out.getWidth(); ++x)
		{
			const Vec2f normed_pos(x/(float)out.getWidth(), y/(float)out.getHeight());
			const Vec2f offset = normed_pos - Vec2f(0.5f, 0.5f);
			const float offset_term = offset.length();
			/*const Vec2 rb_offet = offset * (1.f + amount);
			const Vec2 g_offet = offset * (1.f - amount);
			const Vec2 rb_pos = Vec2(0.5f, 0.5f) + rb_offet;
			const Vec2 g_pos = Vec2(0.5f, 0.5f) + g_offet;*/
			const Vec2f rb_pos = Vec2f(0.5f, 0.5f) + offset * (1.f + offset_term*amount);
			const Vec2f g_pos = Vec2f(0.5f, 0.5f) + offset * (1.f - offset_term*amount);

			out.getPixel(x, y) += Image::ColourType(1.f, 0.f, 1.f) * bilinearSampleImage(in, Vec2f(rb_pos.x*(float)out.getWidth(), rb_pos.y*(float)out.getHeight()));
			out.getPixel(x, y) += Image::ColourType(0.f, 1.f, 0.f) * bilinearSampleImage(in, Vec2f(g_pos.x*(float)out.getWidth(), g_pos.y*(float)out.getHeight()));
		}
	}

}

//rotates image around center
//angle is in radians
static void rotateImage(const Image& in, Image& out, float angle)
{
	out.resize(in.getWidth(), in.getHeight());

	const Matrix2f m(cos(-angle), -sin(-angle), sin(-angle), cos(-angle));

	const Vec2f center((float)in.getWidth() * 0.5f, (float)in.getHeight()  * 0.5f);

	//for each output pixel...
	for(int y=0; y<out.getHeight(); ++y)
		for(int x=0; x<out.getWidth(); ++x)
		{
			//get floating point vector from center of image.
			const Vec2f d = Vec2f((float)x, (float)y) - center;
			const Vec2f rot_d = (m * d) + center;//rotate it

			out.setPixel(x, y, bilinearSampleImage(in, rot_d));//Vec2(rot_d.x / (float)in.getWidth(), rot_d.y / (float)in.getHeight())));
		}
}



void ImageFilter::glareFilter(const Image& in, Image& out, int num_blades, float standard_deviation)
{
	out.resize(in.getWidth(), in.getHeight());
	out.zero();

	Image temp;
	Image temp2;
	for(int i=0; i<num_blades; ++i)
	{
		//Put rotated input image into temp
		const float angle = (float)i * NICKMATHS_2PI / (float)num_blades;
		rotateImage(in, temp, angle);

		//Put blurred temp into temp2
		horizontalGaussianBlur(temp, temp2, standard_deviation);

		//Rotate back into temp
		rotateImage(temp2, temp, -angle);

		//Accumulate onto output image
		out.addImage(temp, 0, 0);
	}

	out.scale(1.f / (float)num_blades);
}







