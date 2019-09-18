/*=====================================================================
ImageMap.h
-------------------
Copyright Glare Technologies Limited 2017 -
Generated at Fri Mar 11 13:14:38 +0000 2011
=====================================================================*/
#pragma once


#include "Map2D.h"
#include "Colour4f.h"
#include "image.h"
#include "GaussianImageFilter.h"
#include "../utils/AllocatorVector.h"
#include "../utils/Exception.h"
namespace Indigo { class TaskManager; }
class OutStream;
class InStream;


/*=====================================================================
ImageMap
-------------------
Tests in ImageMapTests class.
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

	static inline uint32 maxValue() { return 255u; }
};


class UInt16ComponentValueTraits
{
public:
	static inline bool isFloatingPoint() { return false; }

	// Scale from [0, 2^16) down to [0, 1)
	static inline Map2D::Value scaleValue(Map2D::Value x) { return x * (1 / (Map2D::Value)65535); }

	// Scales to [0, 1), then applies gamma correction
	static inline Map2D::Value toLinear(Map2D::Value x, Map2D::Value gamma) { return std::pow(scaleValue(x), gamma); }

	static inline uint32 maxValue() { return 65535u; }
};


class HalfComponentValueTraits
{
public:
	static inline bool isFloatingPoint() { return true; }

	static inline Map2D::Value scaleValue(Map2D::Value x) { return x; }

	static inline Map2D::Value toLinear(Map2D::Value x, Map2D::Value gamma) { return x; }

	static inline uint32 maxValue() { return 1; } // NOTE: not really correct, shouldn't be used tho.
};


class FloatComponentValueTraits
{
public:
	static inline bool isFloatingPoint() { return true; }

	static inline Map2D::Value scaleValue(Map2D::Value x) { return x; }

	static inline Map2D::Value toLinear(Map2D::Value x, Map2D::Value gamma) { return x; }

	static inline uint32 maxValue() { return 1; } // NOTE: not really correct, shouldn't be used tho.
};


template <class V, class ComponentValueTraits>
class ImageMap : public Map2D
{
public:
	inline ImageMap();
	inline ImageMap(size_t width, size_t height, size_t N); // throws Indigo::Exception
	inline virtual ~ImageMap();

	inline ImageMap& operator = (const ImageMap& other);

	inline bool operator == (const ImageMap& other) const;

	void resize(size_t width_, size_t height_, size_t N_); // throws Indigo::Exception
	void resizeNoCopy(size_t width_, size_t height_, size_t N_); // throws Indigo::Exception

	virtual float getGamma() const { return gamma; }
	void setGamma(float g) { gamma = g; }

	inline virtual const Colour4f pixelColour(size_t x, size_t y) const;

	// X and Y are normalised image coordinates.
	inline virtual const Colour4f vec3SampleTiled(Coord x, Coord y) const;

	// X and Y are normalised image coordinates.
	inline virtual Value sampleSingleChannelTiled(Coord x, Coord y, size_t channel) const;
	inline Value scalarSampleTiled(Coord x, Coord y) const { return sampleSingleChannelTiled(x, y, 0); }

	virtual Value getDerivs(Coord s, Coord t, Value& dv_ds_out, Value& dv_dt_out) const;


	inline size_t getWidth() const { return width; }
	inline size_t getHeight() const { return height; }

	inline virtual size_t getMapWidth() const { return width; }
	inline virtual size_t getMapHeight() const { return height; }
	inline virtual size_t numChannels() const { return N; }

	inline virtual bool takesOnlyUnitIntervalValues() const { return !ComponentValueTraits::isFloatingPoint(); }

	inline virtual bool hasAlphaChannel() const { return N == 2 || N == 4; }
	inline virtual Reference<Map2D> extractAlphaChannel() const;
	inline virtual bool isAlphaChannelAllWhite() const;

	inline virtual Reference<Image> convertToImage() const;

	inline virtual Reference<Map2D> extractChannelZero() const;

	virtual Reference<ImageMap<float, FloatComponentValueTraits> > extractChannelZeroLinear() const;

	inline virtual Reference<Map2D> getBlurredLinearGreyScaleImage(Indigo::TaskManager& task_manager) const;

	inline virtual Reference<ImageMap<float, FloatComponentValueTraits> > resizeToImageMapFloat(const int target_width, bool& is_linear) const;

	virtual Reference<Map2D> resizeMidQuality(const int new_width, const int new_height, Indigo::TaskManager& task_manager) const;

	inline virtual size_t getBytesPerPixel() const;

	inline virtual size_t getByteSize() const;

	inline void zero(); // Set all pixels to zero.
	inline void set(V value); // Set all pixel components to value.

	inline void blitToImage(ImageMap<V, ComponentValueTraits>& dest, int dest_start_x, int dest_start_y) const;
	inline void blitToImage(int src_start_x, int src_start_y, int src_end_x, int src_end_y, ImageMap<V, ComponentValueTraits>& dest, int dest_start_x, int dest_start_y) const;

	inline void addImage(const ImageMap<V, ComponentValueTraits>& other);

	inline void copyToImageMapUInt8(ImageMap<uint8, UInt8ComponentValueTraits>& image_out) const;

	// NOTE: not used currently.
	static void downsampleImage(const size_t ssf, const size_t margin_ssf1,
		const size_t filter_span, const float * const resize_filter, const float lower_clamping_bound,
		const ImageMap<V, ComponentValueTraits>& img_in, ImageMap<V, ComponentValueTraits>& img_out, Indigo::TaskManager& task_manager);

	double averageLuminance() const;

	void blendImage(const ImageMap<V, ComponentValueTraits>& img, const int destx, const int desty, const Colour4f& colour);

	// Get num components per pixel.
	inline size_t getN() const { return N; }

	V* getData() { return data.data(); }
	const V* getData() const { return data.data(); }
	inline V* getPixel(size_t x, size_t y);
	inline const V* getPixel(size_t x, size_t y) const;
	inline V* getPixel(size_t i);
	inline const V* getPixel(size_t i) const;
	inline size_t getDataSize() const { return data.size(); }
	inline size_t numPixels() const { return width * height; }

	void setAllocator(const Reference<glare::Allocator>& al) { data.setAllocator(al); }
	Reference<glare::Allocator>& getAllocator() { return data.getAllocator(); }

private:
	size_t width, height, N;
	glare::AllocatorVector<V, 16> data;
	float gamma, ds_over_2, dt_over_2;
};


typedef ImageMap<float, FloatComponentValueTraits> ImageMapFloat;
typedef Reference<ImageMapFloat> ImageMapFloatRef;

typedef ImageMap<uint8, UInt8ComponentValueTraits> ImageMapUInt8;
typedef Reference<ImageMapUInt8> ImageMapUInt8Ref;


template <class V, class VTraits>
void writeToStream(const ImageMap<V, VTraits>& im, OutStream& stream);
template <class V, class VTraits>
void readFromStream(InStream& stream, ImageMap<V, VTraits>& image);


template <class V, class VTraits>
ImageMap<V, VTraits>::ImageMap()
:	width(0), height(0), N(0), gamma(2.2f)
{
}


template <class V, class VTraits>
ImageMap<V, VTraits>::ImageMap(size_t width_, size_t height_, size_t N_)
:	width(width_), height(height_), N(N_), gamma(2.2f), ds_over_2(0.5f / width_), dt_over_2(0.5f / height_)
{
	try
	{
		data.resizeNoCopy(width * height * N);
	}
	catch(std::bad_alloc&)
	{
		throw Indigo::Exception("Failed to create image (memory allocation failure)");
	}
}


template <class V, class VTraits>
ImageMap<V, VTraits>::~ImageMap() {}


template <class V, class VTraits>
ImageMap<V, VTraits>& ImageMap<V, VTraits>::operator = (const ImageMap<V, VTraits>& other)
{
	if(this == &other)
		return *this;
	
	width = other.width;
	height = other.height;
	N = other.N;
	data = other.data;
	gamma = other.gamma;
	ds_over_2 = other.ds_over_2;
	dt_over_2 = other.dt_over_2;
	return *this;
}


template <class V, class VTraits>
bool ImageMap<V, VTraits>::operator == (const ImageMap& other) const
{
	if(width != other.width || height != other.height || N != other.N)
		return false;

	return data == other.data;
}


template <class V, class VTraits>
void ImageMap<V, VTraits>::resize(size_t width_, size_t height_, size_t N_)
{
	width = width_;
	height = height_;
	N = N_;
	ds_over_2 = 0.5f / width_;
	dt_over_2 = 0.5f / height_;

	try
	{
		data.resize(width * height * N);
	}
	catch(std::bad_alloc&)
	{
		throw Indigo::Exception("Failed to create image (memory allocation failure)");
	}
}


template <class V, class VTraits>
void ImageMap<V, VTraits>::resizeNoCopy(size_t width_, size_t height_, size_t N_)
{
	width = width_;
	height = height_;
	N = N_;
	ds_over_2 = 0.5f / width_;
	dt_over_2 = 0.5f / height_;

	try
	{
		data.resizeNoCopy(width * height * N);
	}
	catch(std::bad_alloc&)
	{
		throw Indigo::Exception("Failed to create image (memory allocation failure)");
	}
}


template <class V, class VTraits>
const Colour4f ImageMap<V, VTraits>::pixelColour(size_t x, size_t y) const
{
	const V* const pixel = getPixel(x, y);
	if(N < 3)
		return Colour4f(VTraits::scaleValue(pixel[0]));
	else
		return Colour4f(pixel[0], pixel[1], pixel[2], 0.f) * VTraits::scaleValue(1);
}


template <class V, class VTraits>
const Colour4f ImageMap<V, VTraits>::vec3SampleTiled(Coord u, Coord v) const
{
	Colour4f colour_out;

	// Get fractional normalised image coordinates
	Vec4f normed_coords = Vec4f(u, -v, 0, 0); // Normalised coordinates with v flipped, to go from +v up to +v down.
	Vec4f normed_frac_part = normed_coords - floor(normed_coords); // Fractional part of normed coords, in [0, 1].

	Vec4i dims((int)width, (int)height, 0, 0); // (width, height)		[int]
	Vec4i dims_minus_1 = dims - Vec4i(1); // (width-1, height-1)		[int]

	Vec4f f_pixels = mul(normed_frac_part, toVec4f(dims)); // unnormalised floating point pixel coordinates (pixel_x, pixel_y), in [0, width] x [0, height]  [float]
	
	Vec4i i_pixels = min(toVec4i(f_pixels), dims_minus_1); // truncate pixel coords to integers and clamp to (width-1, height-1).
	Vec4i i_pixels_1 = toVec4i(f_pixels) + Vec4i(1); // pixels + 1, not wrapped yet.
	Vec4i wrapped_i_pixels_1 = select(i_pixels_1, Vec4i(0), /*mask=*/i_pixels_1 < dims); // wrapped_i_pixels_1 = (pixels + 1) <= width ? (pixels + 1) : 0

	// Fractional coords in the pixel:
	Vec4f frac = f_pixels - toVec4f(i_pixels); 
	Vec4f one_frac = Vec4f(1.f) - frac;

	int ut = i_pixels[0];
	int vt = i_pixels[1];
	int ut_1 = wrapped_i_pixels_1[0];
	int vt_1 = wrapped_i_pixels_1[1];
	assert(ut >= 0 && ut < width && vt >= 0 && vt < height);
	assert(ut_1 >= 0 && ut_1 < width && vt_1 >= 0 && vt_1 < height);

	const Coord ufrac = frac[0];
	const Coord vfrac = frac[1];
	const Coord oneufrac = one_frac[0];
	const Coord onevfrac = one_frac[1];

	const V* const use_data = data.data();

	const V* const top_left_pixel  = use_data + (ut   + width * vt  ) * N;
	const V* const top_right_pixel = use_data + (ut_1 + width * vt  ) * N;
	const V* const bot_left_pixel  = use_data + (ut   + width * vt_1) * N;
	const V* const bot_right_pixel = use_data + (ut_1 + width * vt_1) * N;

	const Value a = oneufrac * onevfrac; // Top left pixel weight
	const Value b = ufrac * onevfrac; // Top right pixel weight
	const Value c = oneufrac * vfrac; // Bottom left pixel weight
	const Value d = ufrac * vfrac; // Bottom right pixel weight

	if(N < 3)
	{
		// This is either grey, alpha or grey with alpha.
		// Either way just use the zeroth channel.
		colour_out = Colour4f(VTraits::scaleValue(a * top_left_pixel[0] + b * top_right_pixel[0] + c * bot_left_pixel[0] + d * bot_right_pixel[0]));
	}
	else // else if(N >= 3)
	{
		colour_out = (
			Colour4f(top_left_pixel [0], top_left_pixel [1], top_left_pixel [2], 0) * a + 
			Colour4f(top_right_pixel[0], top_right_pixel[1], top_right_pixel[2], 0) * b +
			Colour4f(bot_left_pixel [0], bot_left_pixel [1], bot_left_pixel [2], 0) * c +
			Colour4f(bot_right_pixel[0], bot_right_pixel[1], bot_right_pixel[2], 0) * d
			) * VTraits::scaleValue(1.f);
	}
	
	return colour_out;
}


template <class V, class VTraits>
Map2D::Value ImageMap<V, VTraits>::sampleSingleChannelTiled(Coord u, Coord v, size_t channel) const
{
	assert(channel < N);

	// Get fractional normalised image coordinates
	Vec4f normed_coords = Vec4f(u, -v, 0, 0); // Normalised coordinates with v flipped, to go from +v up to +v down.
	Vec4f normed_frac_part = normed_coords - floor(normed_coords); // Fractional part of normed coords, in [0, 1].

	Vec4i dims((int)width, (int)height, 0, 0); // (width, height)		[int]
	Vec4i dims_minus_1 = dims - Vec4i(1); // (width-1, height-1)		[int]

	Vec4f f_pixels = mul(normed_frac_part, toVec4f(dims)); // unnormalised floating point pixel coordinates (pixel_x, pixel_y), in [0, width] x [0, height]  [float]

	Vec4i i_pixels = min(toVec4i(f_pixels), dims_minus_1); // truncate pixel coords to integers and clamp to (width-1, height-1).
	Vec4i i_pixels_1 = toVec4i(f_pixels) + Vec4i(1); // pixels + 1, not wrapped yet.
	Vec4i wrapped_i_pixels_1 = select(i_pixels_1, Vec4i(0), /*mask=*/i_pixels_1 < dims); // wrapped_i_pixels_1 = (pixels + 1) <= width ? (pixels + 1) : 0

	// Fractional coords in the pixel:
	Vec4f frac = f_pixels - toVec4f(i_pixels);
	Vec4f one_frac = Vec4f(1.f) - frac;

	int ut = i_pixels[0];
	int vt = i_pixels[1];
	int ut_1 = wrapped_i_pixels_1[0];
	int vt_1 = wrapped_i_pixels_1[1];
	assert(ut >= 0 && ut < width && vt >= 0 && vt < height);
	assert(ut_1 >= 0 && ut_1 < width && vt_1 >= 0 && vt_1 < height);

	const Coord ufrac = frac[0];
	const Coord vfrac = frac[1];
	const Coord oneufrac = one_frac[0];
	const Coord onevfrac = one_frac[1];

	const V* const use_data = data.data();

	const V* const top_left_pixel  = use_data + (ut   + width * vt  ) * N + channel;
	const V* const top_right_pixel = use_data + (ut_1 + width * vt  ) * N + channel;
	const V* const bot_left_pixel  = use_data + (ut   + width * vt_1) * N + channel;
	const V* const bot_right_pixel = use_data + (ut_1 + width * vt_1) * N + channel;

	const Value a = oneufrac * onevfrac; // Top left pixel weight
	const Value b = ufrac * onevfrac; // Top right pixel weight
	const Value c = oneufrac * vfrac; // Bottom left pixel weight
	const Value d = ufrac * vfrac; // Bottom right pixel weight

	return VTraits::scaleValue(a * top_left_pixel[0] + b * top_right_pixel[0] + c * bot_left_pixel[0] + d * bot_right_pixel[0]);
}


// s and t are normalised image coordinates.
// Returns texture value (v) at (s, t)
// Also returns dv/ds and dv/dt.
template <class V, class VTraits>
Map2D::Value ImageMap<V, VTraits>::getDerivs(Coord s, Coord t, Value& dv_ds_out, Value& dv_dt_out) const
{
	// Get fractional normalised image coordinates
	const Coord s_frac_part = Maths::fract(s - ds_over_2);
	const Coord t_frac_part = Maths::fract(-t - dt_over_2);

	// Convert from normalised image coords to pixel coordinates
	const Coord s_pixels = s_frac_part * (Coord)width;
	const Coord t_pixels = t_frac_part * (Coord)height;

	// Get pixel indices
	const size_t ut = myMin((size_t)s_pixels, width - 1);
	const size_t vt = myMin((size_t)t_pixels, height - 1);

	assert(ut < width && vt < height);

	const size_t ut_1 = (ut + 1) >= width  ? 0 : ut + 1;
	const size_t vt_1 = (vt + 1) >= height ? 0 : vt + 1;
	const size_t ut_2 = (ut_1 + 1) >= width  ? 0 : ut_1 + 1;
	const size_t vt_2 = (vt_1 + 1) >= height ? 0 : vt_1 + 1;
	assert(ut_1 < width && vt_1 < height && ut_2 < width && vt_2 < height);

	const V* const use_data = data.data();

	const float f0 = use_data[(ut   + width * vt  ) * N];
	const float f1 = use_data[(ut_1 + width * vt  ) * N];
	const float f2 = use_data[(ut_2 + width * vt  ) * N];
	const float f3 = use_data[(ut   + width * vt_1) * N];
	const float f4 = use_data[(ut_1 + width * vt_1) * N];
	const float f5 = use_data[(ut_2 + width * vt_1) * N];
	const float f6 = use_data[(ut   + width * vt_2) * N];
	const float f7 = use_data[(ut_1 + width * vt_2) * N];
	const float f8 = use_data[(ut_2 + width * vt_2) * N];

	float f_p1_minus_f_p0;
	float f_p3_minus_f_p2;
	float f_centre;
	Coord centre_v;

	// Compute f_p1_minus_f_p0 - the 'horizontal' central difference numerator.
	const Coord top_u = s_pixels - (Coord)ut;
	const Coord offset_v = t_pixels - (Coord)vt + 0.5f; // Samples are offset 0.5 downwards from top left of square.
	if(offset_v < 1.f)
	{
		const Coord u = top_u;
		const Coord v = offset_v;
		centre_v = offset_v;
		const Coord oneu = 1 - u;
		const Coord onev = 1 - offset_v;
		f_p1_minus_f_p0 = VTraits::scaleValue(-oneu*onev*f0 + (1-2*u)*onev*f1 + u*onev*f2 - oneu*v*f3 + (1-2*u)*v*f4 + u*v*f5);
	}
	else // Else if f0 and f1 are in the bottom pixels, 'move' the pixel filter support down by one pixel:
	{
		const Coord u = top_u;
		const Coord v = offset_v - 1.f;
		centre_v = v;
		const Coord oneu = 1 - u;
		const Coord onev = 1 - v;
		f_p1_minus_f_p0 = VTraits::scaleValue(-oneu*onev*f3 + (1-2*u)*onev*f4 + u*onev*f5 - oneu*v*f6 + (1-2*u)*v*f7 + u*v*f8);
	}

	assert(centre_v >= 0 && centre_v <= 1);
	const float one_centre_v = 1 - centre_v;

	// Compute f_p3_minus_f_p2 - the 'vertical' central difference numerator.
	const Coord offset_u = s_pixels - (Coord)ut + 0.5f; // Samples are offset 0.5 rightwards from top left of square.
	const Coord v = t_pixels - (Coord)vt;
	if(offset_u < 1.f) // if f2 and f3 are in the left pixels, pixel filter support is the left pixels:
	{
		const Coord u = offset_u;
		const Coord oneu = 1 - u;
		const Coord onev = 1 - v;
		f_p3_minus_f_p2 = VTraits::scaleValue(-oneu*onev*f0 - u*onev*f1 + oneu*(1-2*v)*f3 + u*(1-2*v)*f4 + oneu*v*f6 + u*v*f7);

		if(offset_v < 1.f) // If centre needs to read from top pixel block:
			f_centre = VTraits::scaleValue(oneu*one_centre_v*f0 + u*one_centre_v*f1 + oneu*centre_v*f3 + u*centre_v*f4); // Centre point is interpolated from upper left pixel (f0, f1, f3, f4)
		else // Else if centre needs to read from bottom pixel block:
			f_centre = VTraits::scaleValue(oneu*one_centre_v*f3 + u*one_centre_v*f4 + oneu*centre_v*f6 + u*centre_v*f7); // Centre point is interpolated from bottom left pixel (f3, f4, f6, f7)
	}
	else // Else if f2 and f3 are in the right pixels, 'move' the pixel filter support right by one pixel:
	{
		const Coord u = offset_u - 1.f;
		const Coord oneu = 1 - u;
		const Coord onev = 1 - v;
		f_p3_minus_f_p2 = VTraits::scaleValue(-oneu*onev*f1 - u*onev*f2 + oneu*(1-2*v)*f4 + u*(1-2*v)*f5 + oneu*v*f7 + u*v*f8);

		if(offset_v < 1.f) // If centre needs to read from top pixel block:
			f_centre = VTraits::scaleValue(oneu*one_centre_v*f1 + u*one_centre_v*f2 + oneu*centre_v*f4 + u*centre_v*f5); // Centre point is interpolated from upper right pixel (f1, f2, f4, f5)
		else // Else if centre needs to read from bottom pixel block:
			f_centre = VTraits::scaleValue(oneu*one_centre_v*f4 + u*one_centre_v*f5 + oneu*centre_v*f7 + u*centre_v*f8); // Centre point is interpolated from bottom right pixel (f4, f5, f7, f8)
	}

	// dv/ds = (f(p1) - f(p0)) / ds,     but ds = 1 / width,   so dv/ds = (f(p1) - f(p0)) * width.   Likewise for dv/dt.
	dv_ds_out = f_p1_minus_f_p0 * width;
	dv_dt_out = -f_p3_minus_f_p2 * height;
	return f_centre;
}


template <class V, class VTraits>
inline V* ImageMap<V, VTraits>::getPixel(size_t x, size_t y)
{
	assert(x < width && y < height);

	return &data[(x + width * y) * N];
}


template <class V, class VTraits>
inline const V* ImageMap<V, VTraits>::getPixel(size_t x, size_t y) const
{
	assert(x < width && y < height);

	return &data[(x + width * y) * N];
}


template <class V, class VTraits>
inline V* ImageMap<V, VTraits>::getPixel(size_t i)
{
	assert(i < width * height);

	return &data[i * N];
}


template <class V, class VTraits>
inline const V* ImageMap<V, VTraits>::getPixel(size_t i) const
{
	assert(i < width * height);

	return &data[i * N];
}


template <class V, class VTraits>
Reference<Map2D> ImageMap<V, VTraits>::extractAlphaChannel() const
{
	ImageMap<V, VTraits>* alpha_map = new ImageMap<V, VTraits>(width, height, 1);
	for(size_t y=0; y<height; ++y)
		for(size_t x=0; x<width; ++x)
		{	
			alpha_map->getPixel(x, y)[0] = this->getPixel(x, y)[N-1];
		}

	return Reference<Map2D>(alpha_map);
}


template <class V, class VTraits>
bool ImageMap<V, VTraits>::isAlphaChannelAllWhite() const
{
	if(!hasAlphaChannel())
		return true;

	for(size_t y=0; y<height; ++y)
		for(size_t x=0; x<width; ++x)
			if(getPixel(x, y)[N-1] != VTraits::maxValue())
				return false;

	return true;
}


template <class V, class VTraits>
Reference<Map2D> ImageMap<V, VTraits>::extractChannelZero() const
{
	ImageMap<V, VTraits>* new_map = new ImageMap<V, VTraits>(width, height, 1);
	for(size_t y=0; y<height; ++y)
		for(size_t x=0; x<width; ++x)
		{	
			new_map->getPixel(x, y)[0] = this->getPixel(x, y)[0];
		}

	return Reference<Map2D>(new_map);
}


template <class V, class VTraits>
Reference<ImageMapFloat> ImageMap<V, VTraits>::extractChannelZeroLinear() const
{
	ImageMapFloat* new_map = new ImageMap<float, FloatComponentValueTraits>(width, height, 1);
	for(size_t y=0; y<height; ++y)
		for(size_t x=0; x<width; ++x)
		{
			new_map->getPixel(x, y)[0] = VTraits::toLinear(this->getPixel(x, y)[0], this->gamma);
		}

	return Reference<ImageMapFloat>(new_map);
}


template <class V, class VTraits>
Reference<Image> ImageMap<V, VTraits>::convertToImage() const
{
	Image* image = new Image(width, height);
	for(size_t y=0; y<height; ++y)
		for(size_t x=0; x<width; ++x)
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
	size_t use_N = 1; // Number of components to average over when computing the greyscale value.
	if(N == 1)
		use_N = 1;
	else if(N == 2)
		use_N = 1;
	else 
		use_N = 3;

	const float N_scale = 1.f / (float)use_N;

	ImageMapFloat img(width, height, 1);
	for(size_t y=0; y<height; ++y)
		for(size_t x=0; x<width; ++x)
		{
			float val = 0;
			for(size_t c=0; c<use_N; ++c)
				val += this->getPixel(x, y)[c];

			img.getPixel(x, y)[0] = VTraits::toLinear(val * N_scale, this->gamma);
		}


	// Blur the floating point image
	Reference<ImageMapFloat> blurred_img = new ImageMapFloat(width, height, 1);
	GaussianImageFilter::gaussianFilter(
		img, 
		*blurred_img, 
		(float)myMax(width, height) * 0.01f, // standard dev in pixels
		task_manager
	);

	return blurred_img;
}


template <class V, class VTraits>
Reference<ImageMap<float, FloatComponentValueTraits> > ImageMap<V, VTraits>::resizeToImageMapFloat(const int target_width, bool& is_linear_out) const
{
	// For this implementation we will use a tent (bilinear) filter, with normalisation.
	// Tent filter gives reasonable resulting image quality, is separable (and therefore fast), and has a small support.
	// We need to do normalisation however, to avoid banding/spotting artifacts when resizing uniform images with the tent filter.
	// Note that if we use a higher quality filter like Mitchell Netravali, we can avoid the renormalisation.
	// But M.N. has a support radius twice as large (2 px), so we'll stick with the fast tent filter.

	// ImageMap can be both, so check if its a floating point image
	is_linear_out = VTraits::isFloatingPoint();

	int tex_xres, tex_yres; // New tex xres, yres

	if(this->getMapHeight() > this->getMapWidth())
	{
		tex_xres = (int)((float)this->getMapWidth() * (float)target_width / (float)this->getMapHeight());
		tex_yres = (int)target_width;
	}
	else
	{
		tex_xres = (int)target_width;
		tex_yres = (int)((float)this->getMapHeight() * (float)target_width / (float)this->getMapWidth());
	}

	const float scale_factor = (float)this->getMapWidth() / tex_xres;
	const float filter_r = myMax(1.f, scale_factor); // Make sure filter_r is at least 1, or we will end up with gaps when upsizing the image!
	const float recip_filter_r = 1 / filter_r;
	const float max_val_scale = 1.f / VTraits::maxValue();
	const float filter_r_plus_1  = filter_r + 1.f;
	const float filter_r_minus_1 = filter_r - 1.f;

	const int src_w = (int)this->getMapWidth();
	const int src_h = (int)this->getMapHeight();

	ImageMapFloat* image;

	if(this->getN() >= 3)
	{
		image = new ImageMapFloat(tex_xres, tex_yres, 3);

		for(int y = 0; y < tex_yres; ++y)
			for(int x = 0; x < tex_xres; ++x)
			{
				const float src_x = x * scale_factor;
				const float src_y = y * scale_factor;

				const int src_begin_x = myMax(0, (int)(src_x - filter_r_minus_1));
				const int src_end_x   = myMin(src_w, (int)(src_x + filter_r_plus_1));
				const int src_begin_y = myMax(0, (int)(src_y - filter_r_minus_1));
				const int src_end_y   = myMin(src_h, (int)(src_y + filter_r_plus_1));

				Colour4f sum(0.f);
				for(int sy = src_begin_y; sy < src_end_y; ++sy)
					for(int sx = src_begin_x; sx < src_end_x; ++sx)
					{
						const float dx = (float)sx - src_x;
						const float dy = (float)sy - src_y;
						const float fabs_dx = std::fabs(dx);
						const float fabs_dy = std::fabs(dy);
						const float filter_val = myMax(1 - fabs_dx * recip_filter_r, 0.f) * myMax(1 - fabs_dy * recip_filter_r, 0.f);
						Colour4f px_col(
							(float)this->getPixel(sx, sy)[0],
							(float)this->getPixel(sx, sy)[1],
							(float)this->getPixel(sx, sy)[2],
							1.f
						);
						sum += px_col * filter_val;
					}

				const Colour4f col = sum * (max_val_scale / sum[3]); // Normalise and apply max_val_scale.
				image->getPixel(x, y)[0] = col[0];
				image->getPixel(x, y)[1] = col[1];
				image->getPixel(x, y)[2] = col[2];
			}
	}
	else
	{
		image = new ImageMapFloat(tex_xres, tex_yres, 1);

		for(int y = 0; y < tex_yres; ++y)
			for(int x = 0; x < tex_xres; ++x)
			{
				const float src_x = x * scale_factor;
				const float src_y = y * scale_factor;

				const int src_begin_x = myMax(0, (int)(src_x - filter_r_minus_1));
				const int src_end_x   = myMin(src_w, (int)(src_x + filter_r_plus_1));
				const int src_begin_y = myMax(0, (int)(src_y - filter_r_minus_1));
				const int src_end_y   = myMin(src_h, (int)(src_y + filter_r_plus_1));

				Colour4f sum(0.f);
				for(int sy = src_begin_y; sy < src_end_y; ++sy)
					for(int sx = src_begin_x; sx < src_end_x; ++sx)
					{
						const float dx = (float)sx - src_x;
						const float dy = (float)sy - src_y;
						const float fabs_dx = std::fabs(dx);
						const float fabs_dy = std::fabs(dy);
						const float filter_val = myMax(1 - fabs_dx * recip_filter_r, 0.f) * myMax(1 - fabs_dy * recip_filter_r, 0.f);
						Colour4f px_col(
							(float)this->getPixel(sx, sy)[0],
							1.f,
							1.f,
							1.f
						);
						sum += px_col * filter_val;
					}

				const Colour4f col = sum * (max_val_scale / sum[3]); // Normalise and apply max_val_scale.
				image->getPixel(x, y)[0] = col[0];
			}
	}

	return Reference<ImageMapFloat>(image);
}


template <class V, class VTraits>
size_t ImageMap<V, VTraits>::getBytesPerPixel() const
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
void ImageMap<V, VTraits>::blitToImage(ImageMap<V, VTraits>& dest, int dest_start_x, int dest_start_y) const
{
	blitToImage(0, 0, (int)getWidth(), (int)getHeight(), dest, dest_start_x, dest_start_y);
}


template <class V, class VTraits>
void ImageMap<V, VTraits>::blitToImage(int src_start_x, int src_start_y, int src_end_x, int src_end_y, ImageMap<V, VTraits>& dest, int dest_start_x, int dest_start_y) const
{
	assert(N == dest.N);

	const int x_offset = dest_start_x - src_start_x; // Offset to go from src pixel coords to dest pixel coords
	const int y_offset = dest_start_y - src_start_y;

	// Clip area to blit from src image so that it lies in the bounds of dest image.
	// When dx = 0, sx = dx - offset = 0 - offset
	// Also clip so that it lies in src image, e.g. so that sx >= 0.
	const int clipped_src_start_x = myMax(src_start_x, 0, -x_offset);
	const int clipped_src_start_y = myMax(src_start_y, 0, -y_offset);

	const int clipped_src_end_x = myMin(src_end_x, (int)getWidth(),  (int)dest.getWidth()  - x_offset);
	const int clipped_src_end_y = myMin(src_end_y, (int)getHeight(), (int)dest.getHeight() - y_offset);

	// If clipped region is empty, return.
	if(clipped_src_start_x >= clipped_src_end_x || clipped_src_start_y >= clipped_src_end_y)
		return;

	for(int sy = clipped_src_start_y; sy < clipped_src_end_y; ++sy)
	for(int sx = clipped_src_start_x; sx < clipped_src_end_x; ++sx)
	{
		const int dx = sx + x_offset;
		const int dy = sy + y_offset;

		for(size_t c=0; c<N; ++c)
			dest.getPixel(dx, dy)[c] = getPixel(sx, sy)[c];
	}
}


template <class V, class VTraits>
void ImageMap<V, VTraits>::addImage(const ImageMap<V, VTraits>& other)
{
	assert(width == other.width && height == other.height && N == other.N);
	const size_t sz = data.size();
	if(sz != other.data.size())
		return;

	for(size_t i=0; i<sz; ++i)
		data[i] += other.data[i];
}


template <class V, class VTraits>
void ImageMap<V, VTraits>::copyToImageMapUInt8(ImageMapUInt8& image_out) const
{
	image_out.resize(getWidth(), getHeight(), getN());

	const float scale = (float)UInt8ComponentValueTraits::maxValue() / VTraits::maxValue();

	const size_t sz = getDataSize();
	for(size_t i=0; i<sz; ++i)
		image_out.getData()[i] = (unsigned char)(myClamp<float>(getData()[i] * scale, 0.f, 255.1f));
}
