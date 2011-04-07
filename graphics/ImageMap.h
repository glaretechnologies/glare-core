/*=====================================================================
ImageMap.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Fri Mar 11 13:14:38 +0000 2011
=====================================================================*/
#pragma once


#include "Map2D.h"
#include <vector>


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

};


class UInt16ComponentValueTraits
{
public:
	static inline bool isFloatingPoint() { return false; }

	// Scale from [0, 2^16) down to [0, 1)
	static inline Map2D::Value scaleValue(Map2D::Value x) { return x * (1 / (Map2D::Value)65535); }
};


class HalfComponentValueTraits
{
public:
	static inline bool isFloatingPoint() { return true; }

	static inline Map2D::Value scaleValue(Map2D::Value x) { return x; }
};


class FloatComponentValueTraits
{
public:
	static inline bool isFloatingPoint() { return true; }

	static inline Map2D::Value scaleValue(Map2D::Value x) { return x; }
};


template <class V, class ComponentValueTraits>
class ImageMap : public Map2D
{
public:
	inline ImageMap();
	inline ImageMap(unsigned int width, unsigned int height, unsigned int N);
	inline ~ImageMap();

	// X and Y are normalised image coordinates.
	inline virtual const Colour3<Value> vec3SampleTiled(Coord x, Coord y) const;

	// X and Y are normalised image coordinates.
	inline virtual Value scalarSampleTiled(Coord x, Coord y) const;


	inline virtual unsigned int getMapWidth() const { return width; }
	inline virtual unsigned int getMapHeight() const { return height; }

	inline virtual bool takesOnlyUnitIntervalValues() const { return !ComponentValueTraits::isFloatingPoint(); }

	inline virtual bool hasAlphaChannel() const { return N == 2 || N == 4; }
	inline virtual Reference<Map2D> extractAlphaChannel() const;


	V* getData() { return &data[0]; }
	inline V* getPixel(unsigned int x, unsigned int y);
	inline const V* getPixel(unsigned int x, unsigned int y) const;
private:
	unsigned int width, height, N;
	std::vector<V> data;
};


template <class V, class VTraits>
ImageMap<V, VTraits>::ImageMap() : width(0), height(0) {}


template <class V, class VTraits>
ImageMap<V, VTraits>::ImageMap(unsigned int width_, unsigned int height_, unsigned int N_)
:	width(width_), height(height_), N(N_)
{
	data.resize(width * height * N);
}


template <class V, class VTraits>
ImageMap<V, VTraits>::~ImageMap() {}


template <class V, class VTraits>
const Colour3<Map2D::Value> ImageMap<V, VTraits>::vec3SampleTiled(Coord u, Coord v) const
{
	Colour3<Value> colour_out;

	Coord intpart; // not used
	Coord u_frac_part = std::modf(u, &intpart);
	Coord v_frac_part = std::modf((Coord)1.0 - v, &intpart); // 1.0 - v because we want v=0 to be at top of image, and v=1 to be at bottom.

	if(u_frac_part < 0.0)
		u_frac_part = 1 + u_frac_part;
	if(v_frac_part < 0.0)
		v_frac_part = 1 + v_frac_part;

	assert(Maths::inHalfClosedInterval<Coord>(u_frac_part, 0.0, 1.0));
	assert(Maths::inHalfClosedInterval<Coord>(v_frac_part, 0.0, 1.0));

	// Convert from normalised image coords to pixel coordinates
	const Coord u_pixels = u_frac_part * (Coord)width;
	const Coord v_pixels = v_frac_part * (Coord)height;

	assert(Maths::inHalfClosedInterval<Coord>(u_pixels, 0.0, (Coord)width));
	assert(Maths::inHalfClosedInterval<Coord>(v_pixels, 0.0, (Coord)height));

	const unsigned int ut = (unsigned int)u_pixels;
	const unsigned int vt = (unsigned int)v_pixels;

	assert(ut >= 0 && ut < width);
	assert(vt >= 0 && vt < height);

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

		// Top left pixel
		{
			const V* pixel = getPixel(ut, vt);
			const Value factor = oneufrac * onevfrac;
			colour_out.r = pixel[0] * factor;
			colour_out.g = pixel[0] * factor;
			colour_out.b = pixel[0] * factor;
		}

		// Top right pixel
		{
			const V* pixel = getPixel(ut_1, vt);
			const Value factor = ufrac * onevfrac;
			colour_out.r += pixel[0] * factor;
			colour_out.g += pixel[0] * factor;
			colour_out.b += pixel[0] * factor;
		}

		// Bottom left pixel
		{
			const V* pixel = getPixel(ut, vt_1);
			const Value factor = oneufrac * vfrac;
			colour_out.r += pixel[0] * factor;
			colour_out.g += pixel[0] * factor;
			colour_out.b += pixel[0] * factor;
		}

		// Bottom right pixel
		{
			const V* pixel = getPixel(ut_1, vt_1);
			const Value factor = ufrac * vfrac;
			colour_out.r += pixel[0] * factor;
			colour_out.g += pixel[0] * factor;
			colour_out.b += pixel[0] * factor;
		}
		
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
	Value colour_out;

	Coord intpart; // not used
	Coord u_frac_part = std::modf(u, &intpart);
	Coord v_frac_part = std::modf((Coord)1.0 - v, &intpart); // 1.0 - v because we want v=0 to be at top of image, and v=1 to be at bottom.

	if(u_frac_part < 0.0)
		u_frac_part = 1 + u_frac_part;
	if(v_frac_part < 0.0)
		v_frac_part = 1 + v_frac_part;

	assert(Maths::inHalfClosedInterval<Coord>(u_frac_part, 0.0, 1.0));
	assert(Maths::inHalfClosedInterval<Coord>(v_frac_part, 0.0, 1.0));

	// Convert from normalised image coords to pixel coordinates
	const Coord u_pixels = u_frac_part * (Coord)width;
	const Coord v_pixels = v_frac_part * (Coord)height;

	assert(Maths::inHalfClosedInterval<Coord>(u_pixels, 0.0, (Coord)width));
	assert(Maths::inHalfClosedInterval<Coord>(v_pixels, 0.0, (Coord)height));

	const unsigned int ut = (unsigned int)u_pixels;
	const unsigned int vt = (unsigned int)v_pixels;

	assert(ut >= 0 && ut < width);
	assert(vt >= 0 && vt < height);

	const unsigned int ut_1 = (ut + 1) % width;
	const unsigned int vt_1 = (vt + 1) % height;

	const Coord ufrac = u_pixels - (Coord)ut;
	const Coord vfrac = v_pixels - (Coord)vt;
	const Coord oneufrac = 1 - ufrac;
	const Coord onevfrac = 1 - vfrac;


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
	return &data[(x + width * y) * N];
}


template <class V, class VTraits>
inline const V* ImageMap<V, VTraits>::getPixel(unsigned int x, unsigned int y) const
{
	assert(x < width && y < height);
	return &data[(x + width * y) * N];
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
