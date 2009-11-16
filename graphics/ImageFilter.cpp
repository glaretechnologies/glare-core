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
#include "../utils/MTwister.h" // just for testing
//#include "fft2d.h"
#include "../indigo/TestUtils.h"
#include "fftss.h"
#include "../utils/timer.h"

// Defined in fft4f2d.c
extern "C" void rdft2d(int n1, int n2, int isgn, double **a, int *ip, double *w);


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
			const int maxx = myMin((int)in.getWidth(), x + pixel_rad + 1);

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
			const int maxx = myMin((int)in.getWidth(), x + pixel_rad + 1);

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
		const int maxy = myMin((int)in.getHeight(), y + pixel_rad + 1);

		for(int x=0; x<in.getWidth(); ++x)
		{
			//get min and max of current filter rect along x axis
			const int minx = myMax(0, x - pixel_rad);
			const int maxx = myMin((int)in.getWidth(), x + pixel_rad + 1);
	
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

	for(int y=0; y<(int)out.getHeight(); ++y)
	{
		for(int x=0; x<(int)out.getWidth(); ++x)
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
	for(int y=0; y<(int)out.getHeight(); ++y)
		for(int x=0; x<(int)out.getWidth(); ++x)
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

static int smallestPowerOf2GE(int x)
{
	int y = 1;
	while(y < x)
		y *= 2;
	return y;
}

void ImageFilter::convolveImage(const Image& in, const Image& filter, Image& out)
{
	if((filter.getWidth() * filter.getHeight()) > 9)
	{
		convolveImageFFT(in, filter, out);
	}
	else
	{
		convolveImageSpatial(in, filter, out);
	}
}

void ImageFilter::convolveImageSpatial(const Image& in, const Image& filter, Image& result_out)
{
	result_out.resize(in.getWidth(), in.getHeight());

	const int rneg_x = (filter.getWidth() % 2 == 0) ? filter.getWidth() / 2 - 1 : filter.getWidth() / 2;
	const int rneg_y = (filter.getHeight() % 2 == 0) ? filter.getHeight() / 2 - 1 : filter.getHeight() / 2;

	const int rpos_x = filter.getWidth() - rneg_x;
	const int rpos_y = filter.getHeight() - rneg_y;

	assert(rneg_x + rpos_x == filter.getWidth());
	assert(rneg_y + rpos_y == filter.getHeight());

	const int W = result_out.getWidth();
	const int H = result_out.getHeight();

	// For each pixel of result image
	for(int y=0; y<H; ++y)
	{
		const int unclipped_src_y_min = y - rneg_y;
		const int src_y_min = myMax(0, y - rneg_y);
		const int src_y_max = myMin(H, y + rpos_y);

		//printVar(y);

		for(int x=0; x<W; ++x)
		{
			const int unclipped_src_x_min = x - rneg_x;
			const int src_x_min = myMax(0, x - rneg_x);
			const int src_x_max = myMin(W, x + rpos_x);

			Colour3f c(0.0f);

			// For each pixel in filter support of source image
			for(int sy=src_y_min; sy<src_y_max; ++sy)
			{
				//const int filter_y = (sy - y) + filter_h_2;
				const int filter_y = sy - unclipped_src_y_min;
				assert(filter_y >= 0 && filter_y < filter.getHeight());		

				for(int sx=src_x_min; sx<src_x_max; ++sx)
				{
					//const int filter_x = (sx - x) + filter_w_2;
					const int filter_x = sx - unclipped_src_x_min;
					assert(filter_x >= 0 && filter_x < filter.getWidth());	

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
	const int x_offset = filter.getWidth() / 2;
	const int y_offset = filter.getHeight() / 2;

	const int W = smallestPowerOf2GE(myMax(in.getWidth(), filter.getWidth()) + x_offset);
	const int H = smallestPowerOf2GE(myMax(in.getHeight(), filter.getHeight()) + y_offset);

	Array2d<double> padded_in(W, H);
	Array2d<double> padded_filter(W, H);
	Array2d<double> padded_convolution(W, H);

	out.resize(in.getWidth(), in.getHeight());

	for(int comp=0; comp<3; ++comp)
	{
		// Zero pad input
		padded_in.setAllElems(0.0);

		// Blit component of input to padded input
		for(int y=0; y<in.getHeight(); ++y)
			for(int x=0; x<in.getWidth(); ++x)
				padded_in.elem(x, y) = (double)in.getPixel(x, y)[comp];

		// Zero pad filter
		padded_filter.setAllElems(0.0);

		// Blit component of filter to padded filter
		for(int y=0; y<filter.getHeight(); ++y)
			for(int x=0; x<filter.getWidth(); ++x)
				padded_filter.elem(x, y) = (double)filter.getPixel(filter.getWidth() - 1 - x, filter.getHeight() - 1 - y)[comp];

		Array2d<Complexd> ft_in;
		realFT(padded_in, ft_in);

		Array2d<Complexd> ft_filter;
		realFT(padded_filter, ft_filter);

		Array2d<Complexd> product(W, H);

		for(int y=0; y<H; ++y)
			for(int x=0; x<W; ++x)
				product.elem(x, y) = ft_in.elem(x, y) * ft_filter.elem(x, y);

		realIFT(product, padded_convolution);

		const double scale = 2.0 / (double)(W * H);

		for(int y=0; y<out.getHeight(); ++y)
			for(int x=0; x<out.getWidth(); ++x)
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

	const int n1 = data.getWidth();
	const int n2 = data.getHeight();

	for(int k1=0; k1<n1; ++k1)
		for(int k2=0; k2<n2; ++k2)
		{
			double re_sum = 0.0;
			double im_sum = 0.0;
			for(int j1=0; j1<n1; ++j1)
				for(int j2=0; j2<n2; ++j2)
				{
					const double phase = NICKMATHS_2PI*(double)j1*(double)k1/(double)n1 + NICKMATHS_2PI*(double)j2*(double)k2/(double)n2;

					re_sum += data.elem(j1, j2) * cos(phase);
					im_sum += data.elem(j1, j2) * sin(phase);
				}
			out.elem(k1, k2) = Complexd(re_sum, im_sum);
		}
}

void ImageFilter::realIFT(const Array2d<Complexd>& data, Array2d<double>& real_out)
{
	real_out.resize(data.getWidth(), data.getHeight());

	const int n1 = data.getWidth();
	const int n2 = data.getHeight();

	for(int k1=0; k1<n1; ++k1)
		for(int k2=0; k2<n2; ++k2)
		{
			double re_sum = 0.0;
			for(int j1=0; j1<n1; ++j1)
				for(int j2=0; j2<n2; ++j2)
				{
					const double phase = NICKMATHS_2PI*(double)j1*(double)k1/(double)n1 + NICKMATHS_2PI*(double)j2*(double)k2/(double)n2;

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

	const int W = input.getWidth();
	const int H = input.getHeight();

	out.resize(W, H);

	// Alloc working arrays
	const int n1 = H;
	const int n2 = W;
	double** indata = new double*[H];

	int* ip = new int[2 + (int)sqrt((double)myMax(n1, n2/2))];
	ip[0] = 0; // ip[0] needs to be initialised to 0

	double* work_area = new double[myMax(n1/2, n2/4) + n2/4];

	// Copy input data
	Array2d<double> data = input;

	// Compute FT of input
	for(int y=0; y<H; ++y)
		indata[y] = &data.elem(0, y);

	rdft2d(
		n1, // input data length (dim1)
		n2, // input data length (dim2)
		1, // input sign (1 for FFT)
		indata,
		ip,
		work_area
		);

	// Now we need to decipher the output

	//                    a[k1][2*k2] = R[k1][k2] = R[n1-k1][n2-k2], 
	//                    a[k1][2*k2+1] = I[k1][k2] = -I[n1-k1][n2-k2], 
	//                       0<k1<n1, 0<k2<n2/2, 
	for(int k1=1; k1<n1; ++k1)
		for(int k2=1; k2<n2/2; ++k2)
		{
			R(out, k1, k2) = Complexd(a(data, k1, 2*k2), a(data, k1, 2*k2+1));

			R(out, n1-k1, n2-k2) = Complexd(a(data, k1, 2*k2), -a(data, k1, 2*k2+1));
		}

	//                    a[0][2*k2] = R[0][k2] = R[0][n2-k2], 
    //                    a[0][2*k2+1] = I[0][k2] = -I[0][n2-k2], 
    //                       0<k2<n2/2, 
	for(int k2=1; k2<n2/2; ++k2)
	{
		R(out, 0, k2) = Complexd(a(data, 0, 2*k2), a(data, 0, 2*k2+1));
		R(out, 0, n2-k2) = Complexd(a(data, 0, 2*k2), -a(data, 0, 2*k2+1));
	}	

    //                    a[k1][0] = R[k1][0] = R[n1-k1][0], 
    //                    a[k1][1] = I[k1][0] = -I[n1-k1][0], 
    //                    a[n1-k1][1] = R[k1][n2/2] = R[n1-k1][n2/2], 
    //                    a[n1-k1][0] = -I[k1][n2/2] = I[n1-k1][n2/2], 
    //                       0<k1<n1/2,

	for(int k1=1; k1<n1/2; ++k1)
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


	delete[] work_area;
	delete[] ip;
	delete[] indata;
}




void ImageFilter::convolveImageFFT(const Image& in, const Image& filter, Image& out)
{
#ifdef DEBUG
	for(unsigned int i=0; i<in.numPixels(); ++i)
		assert(in.getPixel(i).isFinite());
	for(unsigned int i=0; i<filter.numPixels(); ++i)
		assert(filter.getPixel(i).isFinite());
#endif
	assert(filter.getWidth() >= 2);
	assert(filter.getHeight() >= 2);

	const int x_offset = filter.getWidth() / 2;
	const int y_offset = filter.getHeight() / 2;

	const int W = smallestPowerOf2GE(myMax(in.getWidth(), filter.getWidth()) + x_offset);
	const int H = smallestPowerOf2GE(myMax(in.getHeight(), filter.getHeight()) + y_offset);

	assert(Maths::isPowerOfTwo(W) && Maths::isPowerOfTwo(H));
	assert(W >= 2);
	assert(H >= 2);

	Array2d<double> padded_in(W, H);
	Array2d<double> padded_filter(W, H);
	Array2d<double> product(W, H);

	out.resize(in.getWidth(), in.getHeight());

	// Alloc working arrays
	const int n1 = H;
	const int n2 = W;
	double** indata = new double*[H];

	int* ip = new int[2 + (int)sqrt((double)myMax(n1, n2/2))];
	ip[0] = 0; // ip[0] needs to be initialised to 0

	double* work_area = new double[myMax(n1/2, n2/4) + n2/4];


	for(int comp=0; comp<3; ++comp)
	{
		// Zero pad input
		padded_in.setAllElems(0.0);

		// Blit component of input to padded input
		for(int y=0; y<in.getHeight(); ++y)
			for(int x=0; x<in.getWidth(); ++x)
				padded_in.elem(x, y) = (double)in.getPixel(x, y)[comp];

		// Zero pad filter
		padded_filter.setAllElems(0.0);

		// Blit component of filter to padded filter
		for(int y=0; y<filter.getHeight(); ++y)
			for(int x=0; x<filter.getWidth(); ++x)
				padded_filter.elem(x, y) = (double)filter.getPixel(filter.getWidth() - x - 1, filter.getHeight() - y - 1)[comp]; // Note: rotating filter around center point here.


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
		for(int y=0; y<H; ++y)
			indata[y] = padded_in.rowBegin(y);

		rdft2d(
			n1, // input data length (dim1)
			n2, // input data length (dim2)
			1, // input sign (1 for FFT)
			indata,
			ip,
			work_area
			);

		//Ok, now FT of input image should be in 'padded_in'.

		// Compute FT of filter
		for(int y=0; y<H; ++y)
			indata[y] = padded_filter.rowBegin(y);

		rdft2d(
			n1, // input data length (dim1)
			n2, // input data length (dim2)
			1, // input sign (1 for FFT)
			indata,
			ip,
			work_area
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
		a(product, n1/2, 0) = a(padded_in, n1/2, 0) * a(padded_filter, n1/2, 0);
		a(product, n1/2, 1) = a(padded_in, n1/2, 1) * a(padded_filter, n1/2, 1);

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
		for(int k2=1; k2<n2/2; ++k2)
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
		for(int k1=1; k1<H/2; ++k1)
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
		for(int y=0; y<H; ++y)
			indata[y] = product.rowBegin(y);
		
		rdft2d(
			n1, // input data length (dim1)
			n2, // input data length (dim2)
			-1, // input sign (-1 for IFFT)
			indata,
			ip,
			work_area
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
				out.getPixel(x, y)[comp] = (float)(product.elem(
					x + x_offset, //(x + filter.getWidth()/2) % W, 
					y + y_offset //(y + filter.getHeight()/2) % H
					) * scale);
	}

	// Free working arrays
	delete[] ip;
	delete[] work_area;
	delete[] indata;
}


#include <iostream>//TEMP


void ImageFilter::FFTSS_realFFT(const Array2d<double>& data, Array2d<Complexd>& out)
{
	Timer t;

	const int py = data.getWidth() + 1;

	double* in = (double*)fftss_malloc(py * data.getHeight() * sizeof(double) * 2);

	for(int i=0; i<py * data.getHeight() * 2; ++i)
		in[i] = 0.0;
	//double* out = (double*)fftss_malloc(data.getWidth() * data.getHeight() * sizeof(double) * 2);

	// Copy data to in
	/*for(unsigned int i=0; i<data.getWidth() * data.getHeight(); ++i)
	{
		in[i*2    ] = data.getData()[i];
		in[i*2 + 1] = 0.0;
	}*/
	for(unsigned int y=0; y<data.getHeight(); ++y)
		for(unsigned int x=0; x<data.getWidth(); ++x)
		{
			in[x * 2 + y * py * 2] = data.elem(x, y); // re
			in[x * 2 + y * py * 2 + 1] = 0.0; // im
		}

	//TEMP
	//for(int i=0; i<data.getWidth() * data.getHeight(); ++i)
	//	std::cout << in[i*2] << ", " << in[i*2 + 1] << std::endl;

	t.reset();
	fftss_plan_with_nthreads(2);
	fftss_plan plan = fftss_plan_dft_2d(data.getWidth(), data.getHeight(), py, in, in,
                           FFTSS_FORWARD, FFTSS_INOUT|FFTSS_VERBOSE);

	conPrint("plan: " + t.elapsedString());
	
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
	const double scale = 1.0 / (data.getWidth() * data.getHeight() * data.getWidth() * data.getHeight());

	for(unsigned int y=0; y<data.getHeight(); ++y)
		for(unsigned int x=0; x<data.getWidth(); ++x)
		{
			out.elem(x, y).a = in[x * 2 + y * py * 2] * scale;
			out.elem(x, y).b = in[x * 2 + y * py * 2 + 1] * scale * -1.0;
		}

	fftss_destroy_plan(plan);
	fftss_free(in);
}


static void testConvolutionWithDims(int in_w, int in_h, int f_w, int f_h)
{
	conPrint("testWithDims()");
	printVar(in_w);
	printVar(in_h);
	printVar(f_w);
	printVar(f_h);

	MTwister rng(2);

	Image in(in_w, in_h);
	for(unsigned int i=0; i<in.numPixels(); ++i)
		in.getPixel(i).set(rng.unitRandom(), rng.unitRandom(), rng.unitRandom());

	Image filter(f_w, f_h);
	for(unsigned int i=0; i<filter.numPixels(); ++i)
		filter.getPixel(i).set(rng.unitRandom(), rng.unitRandom(), rng.unitRandom());

	// Fast FT convolution
	Image fast_ft_out;
	ImageFilter::convolveImageFFT(in, filter, fast_ft_out);

	// Reference FT convolution
	Image ref_ft_out;
	ImageFilter::slowConvolveImageFFT(in, filter, ref_ft_out);

	// Spatial convolution
	Image spatial_convolution_out;
	ImageFilter::convolveImageSpatial(in, filter, spatial_convolution_out);


	testAssert(fast_ft_out.getWidth() == in.getWidth() && fast_ft_out.getHeight() == in.getHeight());
	testAssert(ref_ft_out.getWidth() == in.getWidth() && ref_ft_out.getHeight() == in.getHeight());
	testAssert(spatial_convolution_out.getWidth() == in.getWidth() && spatial_convolution_out.getHeight() == in.getHeight());

	for(unsigned int i=0; i<fast_ft_out.numPixels(); ++i)
		for(unsigned int c=0; c<3; ++c)
		{
			testAssert(epsEqual(fast_ft_out.getPixel(i)[c], ref_ft_out.getPixel(i)[c]));

			const float a = ref_ft_out.getPixel(i)[c];
			const float b = spatial_convolution_out.getPixel(i)[c];
			testAssert(epsEqual(spatial_convolution_out.getPixel(i)[c], ref_ft_out.getPixel(i)[c], 0.0001f));
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

	/*conPrint("--------------Fast FT-----------------");
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
	}*/

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
		testAssert(epsEqual(b.re(), c.re()));
	}
}


void ImageFilter::test()
{
	conPrint("ImageFilter::test()");

	performanceTestFT(1024, 1024);

	exit(0);

	testFT(4, 4);

	testFT(8, 8);

	testFT(16, 16);

	testFT(8, 16);

	testFT(16, 4);

	testConvolutionWithDims(2, 2, 2, 2);

	testConvolutionWithDims(3, 3, 3, 3);

	testConvolutionWithDims(16, 16, 16, 16);

	testConvolutionWithDims(19, 6, 5, 3);

#ifndef DEBUG
	testConvolutionWithDims(32, 32, 6, 6);
#endif

	testConvolutionWithDims(16, 16, 7, 7);
}
