#include "image.h"


#include "GaussianImageFilter.h"
#include "BoxFilterFunction.h"
#include "bitmap.h"
#include "../maths/vec2.h"
#include "../utils/StringUtils.h"
#include "../utils/FileUtils.h"
#include "../utils/FileHandle.h"
#include "../utils/Exception.h"
#include "../utils/TaskManager.h"
#include "../utils/Task.h"
#include "../utils/OutStream.h"
#include "../utils/InStream.h"
#include <fstream>
#include <limits>
#include <cmath>
#include <cassert>
#include <stdio.h>


Image::Image()
:	pixels(0, 0)
{
}


Image::Image(const Image& other)
{
	this->pixels = other.pixels;
}


Image::Image(size_t width, size_t height)
{
	try
	{
		pixels.resize(width, height);
	}
	catch(std::bad_alloc& )
	{
		const size_t alloc_size = width * height * sizeof(ColourType);
		throw Indigo::Exception("Failed to create image (memory allocation failure of " + ::getNiceByteSize(alloc_size) + ")");
	}
}


Image::~Image()
{}


Image& Image::operator = (const Image& other)
{
	if(&other == this)
		return *this;

	this->pixels = other.pixels;

	return *this;
}


// will throw ImageExcep if bytespp != 3
void Image::setFromBitmap(const Bitmap& bmp, float image_gamma)
{
	if(bmp.getBytesPP() != 1 && bmp.getBytesPP() != 3)
		throw Indigo::Exception("Image bytes per pixel must be 1 or 3.");

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
		throw Indigo::Exception("BytesPP != 3");

	if(x1 < 0 || y1 < 0 || x1 >= x2 || y1 >= y2 || x2 > (int)getWidth() || y2 > (int)getHeight())
		throw Indigo::Exception("Region coordinates are invalid");

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
	set(ColourType(0));
}


void Image::set(const ColourType& c)
{
	const size_t num = numPixels();
	for(size_t i = 0; i < num; ++i)
		getPixel(i) = c;
}


void Image::resize(size_t newwidth, size_t newheight)
{
	if(getWidth() == newwidth && getHeight() == newheight)
		return;

	try
	{
		pixels.resize(newwidth, newheight);
	}
	catch(std::bad_alloc& )
	{
		const size_t alloc_size = newwidth * newheight * sizeof(ColourType);
		throw Indigo::Exception("Failed to create image (memory allocation failure of " + ::getNiceByteSize(alloc_size) + ")");
	}
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
	const int use_src_start_x = myMax(0, src_start_x);
	const int use_src_start_y = myMax(0, src_start_y);

	src_end_x = myMin(src_end_x, (int)getWidth());
	src_end_y = myMin(src_end_y, (int)getHeight());

	const int d_h = (int)dest.getHeight();
	const int d_w = (int)dest.getWidth();

	for(int y = use_src_start_y; y < src_end_y; ++y)
	for(int x = use_src_start_x; x < src_end_x; ++x)
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


void Image::addImage(const Image& other)
{
	if(other.getWidth() != getWidth() || other.getHeight() != getHeight())
		throw Indigo::Exception("Dimensions not the same");

	const size_t N = numPixels();
	
	for(size_t i=0; i<N;  ++i)
		getPixel(i) += other.getPixel(i);
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
	Array2D<float> filter(filter_width, filter_width);
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


double Image::averageValue() const
{
	double sum = 0;
	for(size_t i = 0; i < numPixels(); ++i)
		sum += (double)getPixel(i).averageVal();
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


static const uint32 IMAGE_SERIALISATION_VERSION = 1;


void writeToStream(const Image& im, OutStream& stream, StreamShouldAbortCallback* should_abort_callback)
{
	stream.writeUInt32(IMAGE_SERIALISATION_VERSION);
	stream.writeUInt32((uint32)im.getWidth());
	stream.writeUInt32((uint32)im.getHeight());
	stream.writeData(&im.getPixel(0).r, im.getWidth() * im.getHeight() * sizeof(Image::ColourType), should_abort_callback);
}


void readFromStream(InStream& stream, Image& image, StreamShouldAbortCallback* should_abort_callback)
{
	const uint32 v = stream.readUInt32();
	if(v != IMAGE_SERIALISATION_VERSION)
		throw Indigo::Exception("Unknown version " + toString(v) + ", expected " + toString(IMAGE_SERIALISATION_VERSION) + ".");

	const size_t w = stream.readUInt32();
	const size_t h = stream.readUInt32();

	// TODO: handle max image size

	image.resize(w, h);
	stream.readData((void*)&image.getPixel(0).r, w * h * sizeof(Image::ColourType), should_abort_callback);
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/BufferInStream.h"
#include "../utils/BufferOutStream.h"
#include "../utils/Timer.h"
#include "../indigo/globals.h"


void Image::test()
{
	try
	{
		// Make Image
		size_t w = 100;
		size_t h = 120;
		Image im(w, h);
		for(int y=0; y<h; ++y)
		for(int x=0; x<w; ++x)
			im.setPixel(x, y, Colour3f((float)x, (float)y, (float)x));

		// Write to stream
		BufferOutStream buf;
		writeToStream(im, buf, NULL);

		// Read back from stream
		BufferInStream in_stream(buf.buf);

		Image image2;
		readFromStream(in_stream, image2, NULL);

		testAssert(image2.getWidth() == w);
		testAssert(image2.getHeight() == h);

		for(int y=0; y<h; ++y)
		for(int x=0; x<w; ++x)
		{
			testAssert(image2.getPixel(x, y) == Colour3f((float)x, (float)y, (float)x));
		}

		testAssert(in_stream.endOfStream());
	}
	catch(Indigo::Exception& e)
	{
		failTest(e.what());
	}
}


#endif // BUILD_TESTS
