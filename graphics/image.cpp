#include "image.h"


#include "../indigo/RendererSettings.h"
#include "../utils/stringutils.h"
#include "../utils/fileutils.h"
#include "../utils/FileHandle.h"
#include "../utils/Exception.h"
#include "../maths/vec2.h"
#include "../utils/TaskManager.h"
#include "../utils/Task.h"
#include "GaussianImageFilter.h"
#include "BoxFilterFunction.h"
#include "bitmap.h"
#include <fstream>
#include <limits>
#include <cmath>
#include <cassert>
#include <stdio.h>


Image::Image()
:	pixels(0, 0)
{
}

Image::Image(size_t width_, size_t height_)
:	pixels(width_, height_)
{
}


Image::~Image()
{}


Image& Image::operator = (const Image& other)
{
	if(&other == this)
		return *this;

	if(getWidth() != other.getWidth() || getHeight() != other.getHeight())
	{
		pixels.resize(other.getWidth(), other.getHeight());
	}

	this->pixels = other.pixels;

	return *this;
}


// will throw ImageExcep if bytespp != 3
void Image::setFromBitmap(const Bitmap& bmp, float image_gamma)
{
	if(bmp.getBytesPP() != 1 && bmp.getBytesPP() != 3)
		throw ImageExcep("Image bytes per pixel must be 1 or 3.");

	resize(bmp.getWidth(), bmp.getHeight());

	if(bmp.getBytesPP() == 1)
	{
		const float factor = 1.0f / 255.0f;
		for(size_t y = 0; y < bmp.getHeight(); ++y)
		for(size_t x = 0; x < bmp.getWidth();  ++x)
		{
			setPixel(x, y, Colour3f(
				std::pow((float)*bmp.getPixel(x, y) * factor, image_gamma)
			));
		}
	}
	else
	{
		assert(bmp.getBytesPP() == 3);

		const float factor = 1.0f / 255.0f;
		for(size_t y = 0; y < bmp.getHeight(); ++y)
		for(size_t x = 0; x < bmp.getWidth();  ++x)
		{
			setPixel(x, y,
				Colour3f(
					std::pow((float)bmp.getPixel(x, y)[0] * factor, image_gamma),
					std::pow((float)bmp.getPixel(x, y)[1] * factor, image_gamma),
					std::pow((float)bmp.getPixel(x, y)[2] * factor, image_gamma)
					)
				);
		}
	}
}


// Will throw ImageExcep if bytespp != 3 && bytespp != 4
void Image::copyRegionToBitmap(Bitmap& bmp_out, int x1, int y1, int x2, int y2) const
{
	if(bmp_out.getBytesPP() != 3 && bmp_out.getBytesPP() != 4)
		throw ImageExcep("BytesPP != 3");

	if(x1 < 0 || y1 < 0 || x1 >= x2 || y1 >= y2 || x2 > (int)getWidth() || y2 > (int)getHeight())
		throw ImageExcep("Region coordinates are invalid");

	const int out_width = x2 - x1;
	const int out_height = y2 - y1;

	bmp_out.resize(out_width, out_height, bmp_out.getBytesPP());

	for(int y = y1; y < y2; ++y)
	for(int x = x1; x < x2; ++x)
	{
		unsigned char* pixel = bmp_out.getPixelNonConst(x - x1, y - y1);
		pixel[0] = (unsigned char)(getPixel(x, y).r * 255.0f);
		pixel[1] = (unsigned char)(getPixel(x, y).g * 255.0f);
		pixel[2] = (unsigned char)(getPixel(x, y).b * 255.0f);
	}
}


void Image::copyToBitmap(Bitmap& bmp_out) const
{
	bmp_out.resize(getWidth(), getHeight(), 3);

	for(size_t y = 0; y < getHeight(); ++y)
	for(size_t x = 0; x < getWidth();  ++x)
	{
		const ColourType& p = getPixel(x, y);

		unsigned char* pixel = bmp_out.getPixelNonConst(x, y);
		pixel[0] = (unsigned char)(p.r * 255.0f);
		pixel[1] = (unsigned char)(p.g * 255.0f);
		pixel[2] = (unsigned char)(p.b * 255.0f);
	}
}


void Image::zero()
{
	set(0);
}


void Image::set(const float s)
{
	const ColourType s_colour(s, s, s);

	const size_t num = numPixels();
	for(size_t i = 0; i < num; ++i)
		getPixel(i) = s_colour;
}


void Image::resize(size_t newwidth, size_t newheight)
{
	if(getWidth() == newwidth && getHeight() == newheight)
		return;

	pixels.resize(newwidth, newheight);
}


void Image::posClamp()
{
	const size_t num = numPixels();
	for(size_t i = 0; i < num; ++i)
		getPixel(i).positiveClipComponents();
}


void Image::clampInPlace(float min, float max)
{
	const size_t num = numPixels();
	for(size_t i = 0; i < num; ++i)
		getPixel(i).clampInPlace(min, max);
}


void Image::gammaCorrect(float exponent)
{
	const size_t num = numPixels();
	for(size_t i = 0; i < num; ++i)
	{
		const ColourType colour = getPixel(i);
		ColourType newcolour(
			std::pow(colour.r, exponent),
			std::pow(colour.g, exponent),
			std::pow(colour.b, exponent)
		);

		getPixel(i) = newcolour;
	}
}


void Image::scale(float factor)
{
	const size_t num = numPixels();
	for(size_t i = 0; i < num; ++i)
		getPixel(i) *= factor;
}


void Image::blitToImage(Image& dest, int destx, int desty) const
{
	const int s_h = (int)getHeight();
	const int s_w = (int)getWidth();
	const int d_h = (int)dest.getHeight();
	const int d_w = (int)dest.getWidth();

	for(int y = 0; y < s_h; ++y)
	for(int x = 0; x < s_w;  ++x)
	{
		const int dx = x + destx;
		const int dy = y + desty;

		if(dx >= 0 && dx < d_w && dy >= 0 && dy < d_h)
			dest.setPixel(dx, dy, getPixel(x, y));
	}
}


void Image::blitToImage(int src_start_x, int src_start_y, int src_end_x, int src_end_y, Image& dest, int destx, int desty) const
{
	src_start_x = myMax(0, src_start_x);
	src_start_y = myMax(0, src_start_y);

	src_end_x = myMin(src_end_x, (int)getWidth());
	src_end_y = myMin(src_end_y, (int)getHeight());

	const int d_h = (int)dest.getHeight();
	const int d_w = (int)dest.getWidth();

	for(int y = src_start_y; y < src_end_y; ++y)
	for(int x = src_start_x; x < src_end_x; ++x)
	{
		const int dx = (x - src_start_x) + destx;
		const int dy = (y - src_start_y) + desty;

		if(dx >= 0 && dx < d_w && dy >= 0 && dy < d_h)
			dest.setPixel(dx, dy, getPixel(x, y));
	}
}


void Image::addImage(const Image& img, const int destx, const int desty, const float alpha/* = 1*/)
{
	const int h = (int)getHeight();
	const int w = (int)getWidth();

	for(int y = 0; y < h; ++y)
	for(int x = 0; x < w;  ++x)
	{
		const int dx = x + destx;
		const int dy = y + desty;

		if(dx >= 0 && dx < w && dy >= 0 && dy < h)
			getPixel(dx, dy) += img.getPixel(x, y) * alpha;
	}
}


void Image::blendImage(const Image& img, const int destx, const int desty, const Colour3f& solid_colour, const float alpha/* = 1*/)
{
	const int h = (int)getHeight();
	const int w = (int)getWidth();

	for(int y = 0; y < (int)img.getHeight(); ++y)
	for(int x = 0; x < (int)img.getWidth();  ++x)
	{
		const int dx = x + destx;
		const int dy = y + desty;

		if(dx >= 0 && dx < w && dy >= 0 && dy < h)
			setPixel(dx, dy, solid_colour * img.getPixel(x, y).r * alpha + getPixel(dx, dy) * (1 - img.getPixel(x, y).r * alpha));
	}
}


void Image::mulImage(const Image& img, const int destx, const int desty, const float alpha/* = 1*/, bool invert/* = false*/)
{
	const int h = (int)getHeight();
	const int w = (int)getWidth();

	for(int y = 0; y < (int)img.getHeight(); ++y)
	for(int x = 0; x < (int)img.getWidth();  ++x)
	{
		const int dx = x + destx;
		const int dy = y + desty;

		if(dx >= 0 && dx < w && dy >= 0 && dy < h)
		{
			const float inv_alpha = ((invert) ? 1 - img.getPixel(x, y).r : img.getPixel(x, y).r) * alpha;
			setPixel(dx, dy, getPixel(dx, dy) * (1 - alpha) + getPixel(dx, dy) * inv_alpha);
		}
	}
}


void Image::subImage(const Image& img, int destx, int desty)
{
	const int h = (int)getHeight();
	const int w = (int)getWidth();

	for(int y = 0; y < (int)img.getHeight(); ++y)
	for(int x = 0; x < (int)img.getWidth();  ++x)
	{
		int dx = x + destx;
		int dy = y + desty;

		if(dx >= 0 && dx < w && dy >= 0 && dy < h)
			getPixel(dx, dy).addMult(img.getPixel(x, y), -1.0f); //NOTE: slow :)
	}
}


void Image::overwriteImage(const Image& img, int destx, int desty)
{
	const int h = (int)getHeight();
	const int w = (int)getWidth();

	for(int y = 0; y < (int)img.getHeight(); ++y)
	for(int x = 0; x < (int)img.getWidth();  ++x)
	{
		const int dx = x + destx;
		const int dy = y + desty;
		if(dx >= 0 && dx < w && dy >= 0 && dy < h)
			setPixel(dx, dy, img.getPixel(x, y));
	}
}


// Make the average pixel luminance == 1
void Image::normalise()
{
	if(getHeight() == 0 || getWidth() == 0)
		return;

	double av_lum = 0;
	for(size_t i = 0; i < numPixels(); ++i)
		av_lum += (double)getPixel(i).luminance();
	av_lum /= (double)(numPixels());

	const float factor = (float)(1 / av_lum);
	for(size_t i = 0; i < numPixels(); ++i)
		getPixel(i) *= factor;
}


void Image::loadFromHDR(const std::string& pathname, int width_, int height_)
{
}


/*void Image::collapseSizeBoxFilter(int factor, int border_width)
{
	BoxFilterFunction box_filter_func;
	this->collapseImage(factor, border_width, box_filter_func);
}*/


// trims off border before collapsing
void Image::collapseSizeBoxFilter(int factor/*, int border_width*/)
{
	Image out;
	BoxFilterFunction ff;
	collapseImage(
		factor,
		0, // border
		ff,
		1.0f, // max component value
		*this,
		out
		);

	*this = out;
}


void Image::collapseImage(int factor, int border_width, const FilterFunction& filter_function, float max_component_value, const Image& in, Image& out)
{
	assert(border_width >= 0);
	assert((int)in.getWidth()  > border_width * 2);
	assert((int)in.getHeight() > border_width * 2);
	assert((in.getWidth() - (border_width * 2)) % factor == 0);

	//Image out((width - (border_width * 2)) / factor, (height - (border_width * 2)) / factor);
	out.resize((in.getWidth() - (border_width * 2)) / factor, (in.getHeight() - (border_width * 2)) / factor);

	const double radius_src = filter_function.supportRadius() * (double)factor;
	const int filter_width = (int)ceil(radius_src * 2.0); // neg_rad_src + pos_rad_src;

	//double sum = 0.0;
	Array2d<float> filter(filter_width, filter_width);
	for(int y = 0; y < filter_width; ++y)
	{
		const double pos_y = (double)y + 0.5; // y coordinate in src pixels
		const double abs_dy_src = fabs(pos_y - radius_src);

		for(int x = 0; x < filter_width; ++x)
		{
			const double pos_x = (double)x + 0.5; // x coordinate in src pixels
			const double abs_dx_src = fabs(pos_x - radius_src);

			filter.elem(x, y) = (float)(filter_function.eval(abs_dx_src / (double)factor) *
										filter_function.eval(abs_dy_src / (double)factor) / (double)(factor * factor));
			//sum += (double)filter.elem(x, y);
		}
	}

	//#ifndef INDIGO_NO_OPENMP
	//#pragma omp parallel for
	//#endif
	for(int y=0; y<(int)out.getHeight(); ++y)
	{
		// Get the y-range of pixels in the source image that lie in the filter support for the destination pixel.
		const int support_y = (y * factor) + (int)floor((0.5 - filter_function.supportRadius()) * (double)factor) + border_width;
		const int src_y_min = myMax(0, support_y);
		const int src_y_max = myMin((int)in.getHeight(), support_y + filter_width);

		int support_x = (int)floor((0.5 - filter_function.supportRadius()) * (double)factor) + border_width;

		for(int x=0; x<(int)out.getWidth(); ++x)
		{
			// Get the x-range of pixels in the source image that lie in the filter support for the destination pixel.
			const int src_x_min = myMax(0, support_x);
			const int src_x_max = myMin((int)in.getWidth(), support_x + filter_width);

			Colour3f c(0.0f); // Running sum of source pixel value * filter value.

			// For each pixel in filter support of source image
			for(int sy=src_y_min; sy<src_y_max; ++sy)
			{
				const int filter_y = sy - support_y; //(sy - src_y_center) + neg_rad;
				assert(filter_y >= 0 && filter_y < filter_width);

				for(int sx=src_x_min; sx<src_x_max; ++sx)
				{
					const int filter_x = sx - support_x; //(sx - src_x_center) + neg_rad;
					assert(filter_x >= 0 && filter_x < filter_width);

					assert(in.getPixel(sx, sy).r >= 0.0 && in.getPixel(sx, sy).g >= 0.0 && in.getPixel(sx, sy).b >= 0.0);
					assert(isFinite(in.getPixel(sx, sy).r) && isFinite(in.getPixel(sx, sy).g) && isFinite(in.getPixel(sx, sy).b));

					c.addMult(in.getPixel(sx, sy), filter.elem(filter_x, filter_y));
				}
			}

			//assert(c.r >= 0.0 && c.g >= 0.0 && c.b >= 0.0);
			assert(isFinite(c.r) && isFinite(c.g) && isFinite(c.b));

			c.clampInPlace(0.0f, max_component_value); // Make sure components can't go below zero or above max_component_value
			out.setPixel(x, y, c);

			support_x += factor;
		}
	}
}


struct DownsampleImageTaskClosure
{
	Image::ColourType const * in_buffer;
	Image::ColourType		 * out_buffer;
	const float * resize_filter;
	ptrdiff_t factor, border_width, in_xres, in_yres, filter_bound, out_xres, out_yres;
	float pre_clamp;
};


class DownsampleImageTask : public Indigo::Task
{
public:
	DownsampleImageTask(const DownsampleImageTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin((int)begin_), end((int)end_) {}

	virtual void run(size_t thread_index)
	{
		// Copy to local variables for performance reasons.
		Image::ColourType const * const in_buffer  = closure.in_buffer;
		Image::ColourType		* const out_buffer = closure.out_buffer;
		const float * const resize_filter = closure.resize_filter;
		const ptrdiff_t factor = closure.factor;
		const ptrdiff_t border_width = closure.border_width;
		const ptrdiff_t in_xres = closure.in_xres;
		const ptrdiff_t in_yres = closure.in_yres;
		const ptrdiff_t filter_bound = closure.filter_bound;
		const ptrdiff_t out_xres = closure.out_xres;
		const ptrdiff_t out_yres = closure.out_yres;
		const float pre_clamp = closure.pre_clamp;

		for(int y = begin; y < end; ++y)
		for(int x = 0; x < out_xres; ++x)
		{
			const ptrdiff_t u_min = (x + border_width) * factor + factor / 2 - filter_bound; assert(u_min >= 0);
			const ptrdiff_t v_min = (y + border_width) * factor + factor / 2 - filter_bound; assert(v_min >= 0);
			const ptrdiff_t u_max = (x + border_width) * factor + factor / 2 + filter_bound; assert(u_max < in_xres);
			const ptrdiff_t v_max = (y + border_width) * factor + factor / 2 + filter_bound; assert(v_max < in_yres);

			Image::ColourType weighted_sum(0);
			uint32 filter_addr = 0;
			for(ptrdiff_t v = v_min; v <= v_max; ++v)
			for(ptrdiff_t u = u_min; u <= u_max; ++u)
			{
				const ptrdiff_t addr = v * in_xres + u;
				assert(addr >= 0 && addr < (ptrdiff_t)(in_xres * in_yres)/*img_in.numPixels()*/);

				weighted_sum.addMult(in_buffer[addr], resize_filter[filter_addr++]);
			}

			assert(isFinite(weighted_sum.r) && isFinite(weighted_sum.g) && isFinite(weighted_sum.b));

			weighted_sum.clampInPlace(0.0f, pre_clamp); // Make sure components can't go below zero or above pre_clamp
			out_buffer[y * out_xres + x] = weighted_sum;
		}
	}

	const DownsampleImageTaskClosure& closure;
	int begin, end;
};


void Image::downsampleImage(const ptrdiff_t factor, const ptrdiff_t border_width,
							const ptrdiff_t filter_span, const float * const resize_filter, const float pre_clamp,
							const Image& img_in, Image& img_out, Indigo::TaskManager& task_manager)
{
	assert(border_width >= 0);						// have padding pixels
	assert((int)img_in.getWidth()  > border_width * 2);	// have at least one interior pixel in x
	assert((int)img_in.getHeight() > border_width * 2);	// have at least one interior pixel in y
	assert(img_in.getWidth()  % factor == 0);		// padded image is multiple of supersampling factor
	assert(img_in.getHeight() % factor == 0);		// padded image is multiple of supersampling factor

	assert(filter_span > 0);
	assert(resize_filter != 0);

	const ptrdiff_t in_xres  = (ptrdiff_t)img_in.getWidth();
	const ptrdiff_t in_yres  = (ptrdiff_t)img_in.getHeight();
	const ptrdiff_t filter_bound = filter_span / 2 - 1;

	const ptrdiff_t out_xres = (ptrdiff_t)RendererSettings::computeFinalWidth((int)img_in.getWidth(), (int)factor);
	const ptrdiff_t out_yres = (ptrdiff_t)RendererSettings::computeFinalHeight((int)img_in.getHeight(), (int)factor);
	img_out.resize((size_t)out_xres, (size_t)out_yres);

	ColourType const * const in_buffer  = &img_in.getPixel(0, 0);
	ColourType		 * const out_buffer = &img_out.getPixel(0, 0);

	/*#ifndef INDIGO_NO_OPENMP
	#pragma omp parallel for
	#endif
	for(int y = 0; y < out_yres; ++y)
	for(int x = 0; x < out_xres; ++x)
	{
		const ptrdiff_t u_min = (x + border_width) * factor + factor / 2 - filter_bound; assert(u_min >= 0);
		const ptrdiff_t v_min = (y + border_width) * factor + factor / 2 - filter_bound; assert(v_min >= 0);
		const ptrdiff_t u_max = (x + border_width) * factor + factor / 2 + filter_bound; assert(u_max < (ptrdiff_t)img_in.getWidth());
		const ptrdiff_t v_max = (y + border_width) * factor + factor / 2 + filter_bound; assert(v_max < (ptrdiff_t)img_in.getHeight());

		ColourType weighted_sum(0);
		uint32 filter_addr = 0;
		for(ptrdiff_t v = v_min; v <= v_max; ++v)
		for(ptrdiff_t u = u_min; u <= u_max; ++u)
		{
			const ptrdiff_t addr = v * in_xres + u;
			assert(addr >= 0 && addr < (ptrdiff_t)img_in.numPixels());

			weighted_sum.addMult(in_buffer[addr], resize_filter[filter_addr++]);
		}

		assert(isFinite(weighted_sum.r) && isFinite(weighted_sum.g) && isFinite(weighted_sum.b));

		weighted_sum.clampInPlace(0.0f, pre_clamp); // Make sure components can't go below zero or above pre_clamp
		out_buffer[y * out_xres + x] = weighted_sum;
	}*/

	DownsampleImageTaskClosure closure;
	closure.in_buffer = in_buffer;
	closure.out_buffer = out_buffer;
	closure.resize_filter = resize_filter;
	closure.factor = factor;
	closure.border_width = border_width;
	closure.in_xres = in_xres;
	closure.in_yres = in_yres;
	closure.filter_bound = filter_bound;
	closure.out_xres = out_xres;
	closure.out_yres = out_yres;
	closure.pre_clamp = pre_clamp;

	// Blur in x direction
	task_manager.runParallelForTasks<DownsampleImageTask, DownsampleImageTaskClosure>(closure, 0, out_yres);
}


size_t Image::getByteSize() const
{
	return numPixels() * 3 * sizeof(float);
}


float Image::minLuminance() const
{
	float minlum = std::numeric_limits<float>::max();
	for(size_t i = 0; i < numPixels(); ++i)
		minlum = myMin(minlum, getPixel(i).luminance());
	return minlum;
}


float Image::maxLuminance() const
{
	float maxlum = -std::numeric_limits<float>::max();
	for(size_t i = 0; i < numPixels(); ++i)
		maxlum = myMax(maxlum, getPixel(i).luminance());
	return maxlum;
}


double Image::averageLuminance() const
{
	double sum = 0;
	for(size_t i = 0; i < numPixels(); ++i)
		sum += (double)getPixel(i).luminance();
	return sum / (double)numPixels();
}


float Image::minPixelComponent() const
{
	float x = std::numeric_limits<float>::max();
	for(size_t i = 0; i < numPixels(); ++i)
		x = myMin(x, myMin(getPixel(i).r, myMin(getPixel(i).g, getPixel(i).b)));
	return x;
}


float Image::maxPixelComponent() const
{
	float x = -std::numeric_limits<float>::max();
	for(size_t i = 0; i < numPixels(); ++i)
		x = myMax(x, myMax(getPixel(i).r, myMax(getPixel(i).g, getPixel(i).b)));
	return x;
}


const Colour3<Image::Value> Image::vec3SampleTiled(Coord u, Coord v) const
{
	//return sampleTiled((float)x, (float)y).toColour3d();

	Colour3<Value> colour_out;

	Coord intpart; // not used
	Coord u_frac_part = std::modf(u, &intpart);
	Coord v_frac_part = std::modf(1 - v, &intpart); // 1.0 - v because we want v=0 to be at top of image, and v=1 to be at bottom.

	if(u_frac_part < 0)
		u_frac_part = 1 + u_frac_part;
	if(v_frac_part < 0)
		v_frac_part = 1 + v_frac_part;
	assert(Maths::inHalfClosedInterval<Coord>(u_frac_part, 0.0, 1.0));
	assert(Maths::inHalfClosedInterval<Coord>(v_frac_part, 0.0, 1.0));

	// Convert from normalised image coords to pixel coordinates
	const Coord u_pixels = u_frac_part * (Coord)getWidth();
	const Coord v_pixels = v_frac_part * (Coord)getHeight();
	assert(Maths::inHalfClosedInterval<Coord>(u_pixels, 0.0, (Coord)getWidth()));
	assert(Maths::inHalfClosedInterval<Coord>(v_pixels, 0.0, (Coord)getHeight()));

	const unsigned int ut = (unsigned int)u_pixels;
	const unsigned int vt = (unsigned int)v_pixels;
	assert(ut >= 0 && ut < getWidth());
	assert(vt >= 0 && vt < getHeight());

	const unsigned int ut_1 = (ut + 1) % getWidth();
	const unsigned int vt_1 = (vt + 1) % getHeight();

	const Coord ufrac = u_pixels - (Coord)ut;
	const Coord vfrac = v_pixels - (Coord)vt;
	const Coord oneufrac = 1 - ufrac;
	const Coord onevfrac = 1 - vfrac;

	// Top left pixel
	{
		const float* pixel = getPixel(ut, vt).data();
		const Value factor = oneufrac * onevfrac;
		colour_out.r = pixel[0] * factor;
		colour_out.g = pixel[1] * factor;
		colour_out.b = pixel[2] * factor;
	}

	// Top right pixel
	{
		const float* pixel = getPixel(ut_1, vt).data();
		const Value factor = ufrac * onevfrac;
		colour_out.r += pixel[0] * factor;
		colour_out.g += pixel[1] * factor;
		colour_out.b += pixel[2] * factor;
	}

	// Bottom left pixel
	{
		const float* pixel = getPixel(ut, vt_1).data();
		const Value factor = oneufrac * vfrac;
		colour_out.r += pixel[0] * factor;
		colour_out.g += pixel[1] * factor;
		colour_out.b += pixel[2] * factor;
	}

	// Bottom right pixel
	{
		const float* pixel = getPixel(ut_1, vt_1).data();
		const Value factor = ufrac * vfrac;
		colour_out.r += pixel[0] * factor;
		colour_out.g += pixel[1] * factor;
		colour_out.b += pixel[2] * factor;
	}

	return colour_out;
}


Image::Value Image::scalarSampleTiled(Coord x, Coord y) const
{
	const Colour3<Value> col = vec3SampleTiled(x, y);
	return (col.r + col.g + col.b) * static_cast<Image::Value>(1.0 / 3.0);
}


Reference<Image> Image::convertToImage() const
{
	// Return copy of this image.
	return Reference<Image>(new Image(*this));
}


Reference<Map2D> Image::getBlurredLinearGreyScaleImage(Indigo::TaskManager& task_manager) const
{
	// Blur the image
	Image blurred_img(getWidth(), getHeight());
	GaussianImageFilter::gaussianFilter(
		*this, 
		blurred_img, 
		(float)myMax(getWidth(), getHeight()) * 0.01f, // standard dev in pixels
		task_manager
		);

	return Reference<Map2D>(new Image(blurred_img));
}


Reference<Map2D> Image::resizeToImage(const int target, bool& is_linear) const
{
	// Image class always loads fp data, so should always be in linear space
	is_linear = true;

	size_t tex_xres, tex_yres;
	
	if(this->getHeight() > this->getWidth())
	{
		tex_xres = (size_t)((float)this->getWidth() * (float)target / (float)this->getHeight());
		tex_yres = (size_t)target;
	}
	else
	{
		tex_xres = (size_t)target;
		tex_yres = (size_t)((float)this->getHeight() * (float)target / (float)this->getWidth());
	}

	const float inv_tex_xres = 1.0f / tex_xres;
	const float inv_tex_yres = 1.0f / tex_yres;

	Image* image = new Image(tex_xres, tex_yres);
	Reference<Map2D> map_2d = Reference<Map2D>(image);

	for(size_t y = 0; y < tex_yres; ++y)
	for(size_t x = 0; x < tex_xres; ++x)
	{
		const ColourType texel = this->vec3SampleTiled(x * inv_tex_xres, (tex_yres - y - 1) * inv_tex_yres);

		image->setPixel(x, y, texel);
	}

	return map_2d;
}


unsigned int Image::getBytesPerPixel() const
{
	return sizeof(ColourType);
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../indigo/RendererSettings.h"
#include "../graphics/MitchellNetravaliFilterFunction.h"


void Image::test()
{
	Indigo::TaskManager task_manager;

	const int mitchell_b_steps = 6;

	for(int mitchell_b_int = 0; mitchell_b_int < mitchell_b_steps; ++mitchell_b_int)
	for(int supersample_factor = 2; supersample_factor < 5; ++supersample_factor)
	for(int sqrt_image_size = 1; sqrt_image_size < 3; ++sqrt_image_size)
	{
		// Compute Mitchell-Netravali B, C values from the relation 2C + B = 1
		const double mitchell_b = (double)mitchell_b_int / (double)(mitchell_b_steps - 1);
		const double mitchell_c = (1 - mitchell_b) / 2.0;

		MitchellNetravaliFilterFunction filter(mitchell_b, mitchell_c);

		const int src_xres = RendererSettings::computeFullWidth(sqrt_image_size, supersample_factor);
		const int src_yres = RendererSettings::computeFullHeight(sqrt_image_size, supersample_factor);
		const int dst_xres = sqrt_image_size;
		const int dst_yres = sqrt_image_size;

		Image src(src_xres, src_yres), dst(dst_xres, dst_yres);

		src.set(1);

		Image::downsampleImage(
			supersample_factor,	// factor
			RendererSettings::getMargin(),	// border width
			filter.getFilterSpan(supersample_factor),
			filter.getFilterData(supersample_factor),
			1000000.0f, // max component value
			src, // in
			dst, // out
			task_manager
			);

		testAssert(::epsEqual(dst.maxPixelComponent(), 1.0f));
		//std::cout << "supersample = " << supersample_factor << ", size = " << sqrt_image_size << "x" << sqrt_image_size << ", B = " << mitchell_b << ", C = " << mitchell_c << ", max pixel component = " << dst.maxPixelComponent() << std::endl;
	}
}


#endif // BUILD_TESTS
