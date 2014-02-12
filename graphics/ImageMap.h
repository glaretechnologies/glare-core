/*=====================================================================
ImageMap.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Fri Mar 11 13:14:38 +0000 2011
=====================================================================*/
#pragma once


#include "Map2D.h"
#include "image.h"
#include "GaussianImageFilter.h"
#include "../utils/Vector.h"
#include "../utils/Exception.h"
namespace Indigo { class TaskManager; }
class OutStream;
class InStream;
class StreamShouldAbortCallback;


// #define IMAGE_MAP_TILED 1


/*=====================================================================
ImageMap
-------------------

=====================================================================*/

class UInt8ComponentValueTraits
{
public:
	static inline bool isFloatingPoint() { return false; }

	// Scale from [0, 2^8) down to [0, 1)
	// NOTE: divide by 255 now to be consistent with Texture.  BlendMaterial efficient blending when t = 0 or 1 depends on dividing by 255 as well.
	static inline Map2D::Value scaleValue(Map2D::Value x) { return x * (1 / (Map2D::Value)255); }

	// Scales to [0, 1), then applies gamma correction
	static inline Map2D::Value toLinear(Map2D::Value x, Map2D::Value gamma) { return std::pow(scaleValue(x), gamma); }
};


class UInt16ComponentValueTraits
{
public:
	static inline bool isFloatingPoint() { return false; }

	// Scale from [0, 2^16) down to [0, 1)
	static inline Map2D::Value scaleValue(Map2D::Value x) { return x * (1 / (Map2D::Value)65535); }

	// Scales to [0, 1), then applies gamma correction
	static inline Map2D::Value toLinear(Map2D::Value x, Map2D::Value gamma) { return std::pow(scaleValue(x), gamma); }
};


class HalfComponentValueTraits
{
public:
	static inline bool isFloatingPoint() { return true; }

	static inline Map2D::Value scaleValue(Map2D::Value x) { return x; }

	static inline Map2D::Value toLinear(Map2D::Value x, Map2D::Value gamma) { return x; }
};


class FloatComponentValueTraits
{
public:
	static inline bool isFloatingPoint() { return true; }

	static inline Map2D::Value scaleValue(Map2D::Value x) { return x; }

	static inline Map2D::Value toLinear(Map2D::Value x, Map2D::Value gamma) { return x; }
};


template <class V, class ComponentValueTraits>
class ImageMap : public Map2D
{
public:
	inline ImageMap();
	inline ImageMap(unsigned int width, unsigned int height, unsigned int N); // throws Indigo::Exception
	inline ~ImageMap();

	void resize(unsigned int width_, unsigned int height_, unsigned int N_); // throws Indigo::Exception

	float getGamma() const { return gamma; }
	void setGamma(float g) { gamma = g; }

	inline virtual const Colour3<Value> pixelColour(size_t x, size_t y) const;

	// X and Y are normalised image coordinates.
	inline virtual const Colour3<Value> vec3SampleTiled(Coord x, Coord y) const;

	// X and Y are normalised image coordinates.
	inline virtual Value scalarSampleTiled(Coord x, Coord y) const;


	inline unsigned int getWidth() const { return width; }
	inline unsigned int getHeight() const { return height; }

	inline virtual unsigned int getMapWidth() const { return width; }
	inline virtual unsigned int getMapHeight() const { return height; }

	inline virtual bool takesOnlyUnitIntervalValues() const { return !ComponentValueTraits::isFloatingPoint(); }

	inline virtual bool hasAlphaChannel() const { return N == 2 || N == 4; }
	inline virtual Reference<Map2D> extractAlphaChannel() const;

	inline virtual Reference<Image> convertToImage() const;

	inline virtual Reference<Map2D> getBlurredLinearGreyScaleImage(Indigo::TaskManager& task_manager) const;

	inline virtual Reference<Map2D> resizeToImage(const int width, bool& is_linear) const;

	inline virtual unsigned int getBytesPerPixel() const;

	inline size_t getByteSize() const;

	inline void zero(); // Set all pixels to zero.
	inline void set(V value); // Set all pixel components to value.

	inline void blitToImage(ImageMap<V, ComponentValueTraits>& dest, int destx, int desty) const;

	// Get num components per pixel.
	inline unsigned int getN() const { return N; }

	V* getData() { return &data[0]; }
	const V* getData() const { return &data[0]; }
	inline V* getPixel(unsigned int x, unsigned int y);
	inline const V* getPixel(unsigned int x, unsigned int y) const;

private:
#if IMAGE_MAP_TILED
	unsigned int w_blocks, h_blocks;
#endif
	unsigned int width, height, N;
	js::Vector<V, 16> data;
	float gamma;
};


typedef ImageMap<float, FloatComponentValueTraits> ImageMapFloat;
typedef Reference<ImageMapFloat> ImageMapFloatRef;


template <class V, class VTraits>
void writeToStream(const ImageMap<V, VTraits>& im, OutStream& stream, StreamShouldAbortCallback* should_abort_callback);
template <class V, class VTraits>
void readFromStream(InStream& stream, ImageMap<V, VTraits>& image, StreamShouldAbortCallback* should_abort_callback);


template <class V, class VTraits>
ImageMap<V, VTraits>::ImageMap()
:	width(0), height(0), N(0), gamma(2.2f)
{
}


template <class V, class VTraits>
ImageMap<V, VTraits>::ImageMap(unsigned int width_, unsigned int height_, unsigned int N_)
:	width(width_), height(height_), N(N_), gamma(2.2f)
{
	try
	{
#if IMAGE_MAP_TILED
		w_blocks = width / 8 + 1; // TEMP fixme
		h_blocks = height / 8 + 1;
		data.resize(w_blocks * h_blocks * 64 * N);
#else
		//data.resize(width * height * N);
		data.resizeUninitialised(width * height * N);
#endif
	}
	catch(std::bad_alloc&)
	{
		throw Indigo::Exception("Failed to create image (memory allocation failure)");
	}
}


template <class V, class VTraits>
ImageMap<V, VTraits>::~ImageMap() {}


template <class V, class VTraits>
void ImageMap<V, VTraits>::resize(unsigned int width_, unsigned int height_, unsigned int N_)
{
	width = width_;
	height = height_;
	N = N_;

	try
	{
		//data.resize(width * height * N);
		data.resizeUninitialised(width * height * N);
	}
	catch(std::bad_alloc&)
	{
		throw Indigo::Exception("Failed to create image (memory allocation failure)");
	}
}


template <class V, class VTraits>
const Colour3<Map2D::Value> ImageMap<V, VTraits>::pixelColour(size_t x_, size_t y_) const
{
	unsigned int x = (unsigned int)x_;
	unsigned int y = (unsigned int)y_;
	Colour3<Value> colour_out;
	if(N < 3)
		colour_out.r = colour_out.g = colour_out.b = VTraits::scaleValue(getPixel(x, y)[0]);
	else
	{
		colour_out.r = VTraits::scaleValue(getPixel(x, y)[0]);
		colour_out.g = VTraits::scaleValue(getPixel(x, y)[1]);
		colour_out.b = VTraits::scaleValue(getPixel(x, y)[2]);
	}

	return colour_out;
}


template <class V, class VTraits>
const Colour3<Map2D::Value> ImageMap<V, VTraits>::vec3SampleTiled(Coord u, Coord v) const
{
	Colour3<Value> colour_out;

	// Get fractional normalised image coordinates
	Coord u_frac_part = Maths::fract(u);
	Coord v_frac_part = Maths::fract(1 - v);

	// Convert from normalised image coords to pixel coordinates
	const Coord u_pixels = u_frac_part * (Coord)width;
	const Coord v_pixels = v_frac_part * (Coord)height;

	// Get pixel indices
	const unsigned int ut = myMin((unsigned int)u_pixels, width - 1);
	const unsigned int vt = myMin((unsigned int)v_pixels, height - 1);

	assert(ut < width && vt < height);

	const unsigned int ut_1 = (ut + 1) % width;
	const unsigned int vt_1 = (vt + 1) % height;

	const Coord ufrac = u_pixels - (Coord)ut;
	const Coord vfrac = v_pixels - (Coord)vt;
	const Coord oneufrac = 1 - ufrac;
	const Coord onevfrac = 1 - vfrac;

	if(N < 3)
	{
		// This is either grey, alpha or grey with alpha.
		// Either way just use zeroth channel
		
		Value val;

		// Top left pixel
		{
			const V* pixel = getPixel(ut, vt);
			const Value factor = oneufrac * onevfrac;
			val = pixel[0] * factor;
		}

		// Top right pixel
		{
			const V* pixel = getPixel(ut_1, vt);
			const Value factor = ufrac * onevfrac;
			val += pixel[0] * factor;
		}

		// Bottom left pixel
		{
			const V* pixel = getPixel(ut, vt_1);
			const Value factor = oneufrac * vfrac;
			val += pixel[0] * factor;
		}

		// Bottom right pixel
		{
			const V* pixel = getPixel(ut_1, vt_1);
			const Value factor = ufrac * vfrac;
			val += pixel[0] * factor;
		}

		colour_out.r = val;
		colour_out.g = val;
		colour_out.b = val;
	}
	else if(N >= 3)
	{
		// This map is either RGB or RGB with alpha
		// Ignore alpha and just return the interpolated RGB colour.

		// Top left pixel
		{
			const V* pixel = getPixel(ut, vt);
			const Value factor = oneufrac * onevfrac;
			colour_out.r = pixel[0] * factor;
			colour_out.g = pixel[1] * factor;
			colour_out.b = pixel[2] * factor;
		}

		// Top right pixel
		{
			const V* pixel = getPixel(ut_1, vt);
			const Value factor = ufrac * onevfrac;
			colour_out.r += pixel[0] * factor;
			colour_out.g += pixel[1] * factor;
			colour_out.b += pixel[2] * factor;
		}

		// Bottom left pixel
		{
			const V* pixel = getPixel(ut, vt_1);
			const Value factor = oneufrac * vfrac;
			colour_out.r += pixel[0] * factor;
			colour_out.g += pixel[1] * factor;
			colour_out.b += pixel[2] * factor;
		}

		// Bottom right pixel
		{
			const V* pixel = getPixel(ut_1, vt_1);
			const Value factor = ufrac * vfrac;
			colour_out.r += pixel[0] * factor;
			colour_out.g += pixel[1] * factor;
			colour_out.b += pixel[2] * factor;
		}	
	}
	else
	{
		assert(0);
	}

	colour_out.r = VTraits::scaleValue(colour_out.r);
	colour_out.g = VTraits::scaleValue(colour_out.g);
	colour_out.b = VTraits::scaleValue(colour_out.b);
	return colour_out;
}


template <class V, class VTraits>
Map2D::Value ImageMap<V, VTraits>::scalarSampleTiled(Coord u, Coord v) const
{
	// Get fractional normalised image coordinates
	Coord u_frac_part = Maths::fract(u);
	Coord v_frac_part = Maths::fract(1 - v);

	// Convert from normalised image coords to pixel coordinates
	const Coord u_pixels = u_frac_part * (Coord)width;
	const Coord v_pixels = v_frac_part * (Coord)height;

	// Get pixel indices
	const unsigned int ut = myMin((unsigned int)u_pixels, width - 1);
	const unsigned int vt = myMin((unsigned int)v_pixels, height - 1);

	assert(ut < width && vt < height);

	const unsigned int ut_1 = (ut + 1) % width;
	const unsigned int vt_1 = (vt + 1) % height;

	const Coord ufrac = u_pixels - (Coord)ut;
	const Coord vfrac = v_pixels - (Coord)vt;
	const Coord oneufrac = 1 - ufrac;
	const Coord onevfrac = 1 - vfrac;

	Value colour_out;

	// Top left pixel
	{
		const V* pixel = getPixel(ut, vt);
		const Value factor = oneufrac * onevfrac;
		//for(unsigned int i=0; i<N; ++i)
		//	colour_out = pixel[i] * factor;
		colour_out = pixel[0] * factor;
	}

	// Top right pixel
	{
		const V* pixel = getPixel(ut_1, vt);
		const Value factor = ufrac * onevfrac;
		//for(unsigned int i=0; i<N; ++i)
		//	colour_out += pixel[i] * factor;
		colour_out += pixel[0] * factor;
	}

	// Bottom left pixel
	{
		const V* pixel = getPixel(ut, vt_1);
		const Value factor = oneufrac * vfrac;
		//for(unsigned int i=0; i<N; ++i)
		//	colour_out += pixel[i] * factor;
		colour_out += pixel[0] * factor;
	}

	// Bottom right pixel
	{
		const V* pixel = getPixel(ut_1, vt_1);
		const Value factor = ufrac * vfrac;
		//for(unsigned int i=0; i<N; ++i)
		//	colour_out += pixel[i] * factor;
		colour_out += pixel[0] * factor;
	}

	// Divide by number of colour components
	//const Value N_factor = ((Value)1 / (Value)N); // TODO: test this is computed at compile time.

	return VTraits::scaleValue(colour_out)/* * N_factor*/;
}


template <class V, class VTraits>
inline V* ImageMap<V, VTraits>::getPixel(unsigned int x, unsigned int y)
{
	assert(x < width && y < height);

#if IMAGE_MAP_TILED
	uint32 block_x = x >> 3;
	uint32 block_y = y >> 3;

	uint32 inblock_x = x & 7;
	uint32 inblock_y = y & 7;

	return &data[((block_y * w_blocks + block_x) * 64 + (inblock_y*8) + inblock_x) * N];
#else
	return &data[(x + width * y) * N];
#endif
}


template <class V, class VTraits>
inline const V* ImageMap<V, VTraits>::getPixel(unsigned int x, unsigned int y) const
{
	assert(x < width && y < height);

#if IMAGE_MAP_TILED
	uint32 block_x = x >> 3;
	uint32 block_y = y >> 3;

	uint32 inblock_x = x & 7;
	uint32 inblock_y = y & 7;

	return &data[((block_y * w_blocks + block_x) * 64 + (inblock_y*8) + inblock_x) * N];
#else
	return &data[(x + width * y) * N];
#endif
}


template <class V, class VTraits>
Reference<Map2D> ImageMap<V, VTraits>::extractAlphaChannel() const
{
	ImageMap<V, VTraits>* alpha_map = new ImageMap<V, VTraits>(width, height, 1);
	for(unsigned int y=0; y<height; ++y)
		for(unsigned int x=0; x<width; ++x)
		{	
			alpha_map->getPixel(x, y)[0] = this->getPixel(x, y)[N-1];
		}

	return Reference<Map2D>(alpha_map);
}


template <class V, class VTraits>
Reference<Image> ImageMap<V, VTraits>::convertToImage() const
{
	Image* image = new Image(width, height);
	for(unsigned int y=0; y<height; ++y)
		for(unsigned int x=0; x<width; ++x)
		{
			if(N < 3)
			{
				image->setPixel(x, y, Colour3f(
					VTraits::toLinear(this->getPixel(x, y)[0], this->gamma)
				));
			}
			else
			{
				image->setPixel(x, y, Colour3f(
					VTraits::toLinear(this->getPixel(x, y)[0], this->gamma),
					VTraits::toLinear(this->getPixel(x, y)[1], this->gamma),
					VTraits::toLinear(this->getPixel(x, y)[2], this->gamma)
				));
			}
		}
	return Reference<Image>(image);
}


template <class V, class VTraits>
Reference<Map2D> ImageMap<V, VTraits>::getBlurredLinearGreyScaleImage(Indigo::TaskManager& task_manager) const
{
	// Convert this low-bit-depth texture to a 32 bit floating point image.

	// We don't want to include the alpha channel in our blurred greyscale image.
	unsigned int use_N = 1;
	if(N == 1)
		use_N = 1;
	else if(N == 2)
		use_N = 1;
	else 
		use_N = 3;

	const float N_scale = 1.f / (float)use_N;

	Image img(width, height);
	for(unsigned int y=0; y<height; ++y)
		for(unsigned int x=0; x<width; ++x)
		{
			float val = 0;
			for(unsigned int c=0; c<use_N; ++c)
				val += this->getPixel(x, y)[c];

			img.setPixel(x, y, Colour3f(VTraits::toLinear(val * N_scale, this->gamma)));
		}


	// Blur the floating point image
	Image blurred_img(width, height);
	GaussianImageFilter::gaussianFilter(
		img, 
		blurred_img, 
		(float)myMax(width, height) * 0.01f, // standard dev in pixels
		task_manager
		);

	return Reference<Map2D>(new Image(blurred_img));
}


template <class V, class VTraits>
Reference<Map2D> ImageMap<V, VTraits>::resizeToImage(const int target, bool& is_linear) const
{
	// ImageMap can be both, so check if its a floating point image
	is_linear = VTraits::isFloatingPoint();

	size_t tex_xres, tex_yres;
	
	if(this->getMapHeight() > this->getMapWidth())
	{
		tex_xres = (size_t)((float)this->getMapWidth() * (float)target / (float)this->getMapHeight());
		tex_yres = (size_t)target;
	}
	else
	{
		tex_xres = (size_t)target;
		tex_yres = (size_t)((float)this->getMapHeight() * (float)target / (float)this->getMapWidth());
	}

	const float inv_tex_xres = 1.0f / tex_xres;
	const float inv_tex_yres = 1.0f / tex_yres;

	Image* image = new Image(tex_xres, tex_yres);
	Reference<Map2D> map_2d = Reference<Map2D>(image);

	for(size_t y = 0; y < tex_yres; ++y)
	for(size_t x = 0; x < tex_xres; ++x)
	{
		const Colour3<float> texel = this->vec3SampleTiled(x * inv_tex_xres, (tex_yres - y - 1) * inv_tex_yres);

		image->setPixel(x, y, texel);
	}

	return map_2d;
}


template <class V, class VTraits>
unsigned int ImageMap<V, VTraits>::getBytesPerPixel() const
{
	return sizeof(V) * N;
}


template <class V, class VTraits>
size_t ImageMap<V, VTraits>::getByteSize() const
{
	return getWidth() * getHeight() * getN() * sizeof(V);
}


template <class V, class VTraits>
inline void ImageMap<V, VTraits>::set(V value) // Set all pixel components to value.
{
	for(size_t i=0; i<data.size(); ++i)
		data[i] = value;
}


template <class V, class VTraits>
void ImageMap<V, VTraits>::zero()
{
	set(0);
}


template <class V, class VTraits>
void ImageMap<V, VTraits>::blitToImage(ImageMap<V, VTraits>& dest, int destx, int desty) const
{
	assert(N == dest.N);

	const int s_h = (int)getHeight();
	const int s_w = (int)getWidth();
	const int d_h = (int)dest.getHeight();
	const int d_w = (int)dest.getWidth();

	// NOTE: this can be optimised to remove the destination pixel valid check.
		
	for(int y = 0; y < s_h; ++y)
	for(int x = 0; x < s_w; ++x)
	{
		const int dx = x + destx;
		const int dy = y + desty;

		if(dx >= 0 && dx < d_w && dy >= 0 && dy < d_h)
			for(unsigned int c=0; c<N; ++c)
				dest.getPixel(x, y)[c] = getPixel(x, y)[c];
	}
}

