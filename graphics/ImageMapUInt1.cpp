/*=====================================================================
ImageMapUInt1.cpp
-----------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "ImageMapUInt1.h"


#include "image.h"
#include "ImageMap.h"


ImageMapUInt1::ImageMapUInt1()
:	width(0), height(0)
{
}


ImageMapUInt1::ImageMapUInt1(size_t width_, size_t height_, glare::Allocator* mem_allocator_)
:	width(width_), height(height_)
{
	//if(mem_allocator_)
	//	data.setAllocator(mem_allocator_);

	try
	{
		data.resizeNoCopy(width * height);
	}
	catch(std::bad_alloc&)
	{
		throw glare::Exception("Failed to create image (memory allocation failure)");
	}
}


ImageMapUInt1::~ImageMapUInt1() {}


ImageMapUInt1& ImageMapUInt1::operator = (const ImageMapUInt1& other)
{
	if(this == &other)
		return *this;
	
	width = other.width;
	height = other.height;
	data = other.data;
	return *this;
}


bool ImageMapUInt1::operator == (const ImageMapUInt1& other) const
{
	if(width != other.width || height != other.height)
		return false;

	return data == other.data;
}


void ImageMapUInt1::zero()
{
	data.setAllBits(0);
}


void ImageMapUInt1::resize(size_t width_, size_t height_)
{
	width = width_;
	height = height_;

	try
	{
		data.resize(width * height);
	}
	catch(std::bad_alloc&)
	{
		throw glare::Exception("Failed to create image (memory allocation failure)");
	}
}


void ImageMapUInt1::resizeNoCopy(size_t width_, size_t height_)
{
	width = width_;
	height = height_;
	
	try
	{
		data.resizeNoCopy(width * height);
	}
	catch(std::bad_alloc&)
	{
		throw glare::Exception("Failed to create image (memory allocation failure)");
	}
}


const Colour4f ImageMapUInt1::pixelColour(size_t x, size_t y) const
{
	assert(0);
	return Colour4f(0);
}


const Colour4f ImageMapUInt1::vec3Sample(Coord u, Coord v, bool wrap) const
{
	assert(0);
	return Colour4f(0);
}


Map2D::Value ImageMapUInt1::sampleSingleChannelTiled(Coord u, Coord v, size_t channel) const
{
	assert(channel == 0);

	// Get fractional normalised image coordinates
	Vec4f normed_coords = Vec4f(u, -v, 0, 0); // Normalised coordinates with v flipped, to go from +v up to +v down.
	Vec4f normed_frac_part = normed_coords - floor(normed_coords); // Fractional part of normed coords, in [0, 1].

	Vec4i dims((int)width, (int)height, 0, 0); // (width, height)		[int]
	Vec4i dims_minus_1 = dims - Vec4i(1); // (width-1, height-1)		[int]

	Vec4f f_pixels = mul(normed_frac_part, toVec4f(dims)); // unnormalised floating point pixel coordinates (pixel_x, pixel_y), in [0, width] x [0, height]  [float]
	
	// We max with 0 here because otherwise Inf or NaN texture coordinates can result in out of bounds reads.
	Vec4i i_pixels_clamped = max(Vec4i(0), toVec4i(f_pixels));
	Vec4i i_pixels = min(i_pixels_clamped, dims_minus_1); // truncate pixel coords to integers and clamp to (width-1, height-1).
	Vec4i i_pixels_1 = i_pixels_clamped + Vec4i(1); // pixels + 1, not wrapped yet.
	Vec4i wrapped_i_pixels_1 = select(i_pixels_1, Vec4i(0), /*mask=*/i_pixels_1 < dims); // wrapped_i_pixels_1 = (pixels + 1) <= width ? (pixels + 1) : 0

	// Fractional coords in the pixel:
	Vec4f frac = f_pixels - toVec4f(i_pixels);
	Vec4f one_frac = Vec4f(1.f) - frac;

	int ut = i_pixels[0];
	int vt = i_pixels[1];
	int ut_1 = wrapped_i_pixels_1[0];
	int vt_1 = wrapped_i_pixels_1[1];
	assert(ut >= 0 && ut < (int)width && vt >= 0 && vt < (int)height);
	assert(ut_1 >= 0 && ut_1 < (int)width && vt_1 >= 0 && vt_1 < (int)height);

	const Coord ufrac = frac[0];
	const Coord vfrac = frac[1];
	const Coord oneufrac = one_frac[0];
	const Coord onevfrac = one_frac[1];


	const Value top_left_pixel  = (Value)data.getBit(ut   + width * vt  );
	const Value top_right_pixel = (Value)data.getBit(ut_1 + width * vt  );
	const Value bot_left_pixel  = (Value)data.getBit(ut   + width * vt_1);
	const Value bot_right_pixel = (Value)data.getBit(ut_1 + width * vt_1);

	const Value a = oneufrac * onevfrac; // Top left pixel weight
	const Value b = ufrac * onevfrac; // Top right pixel weight
	const Value c = oneufrac * vfrac; // Bottom left pixel weight
	const Value d = ufrac * vfrac; // Bottom right pixel weight
	
	return a * top_left_pixel + b * top_right_pixel + c * bot_left_pixel + d * bot_right_pixel;
}


Map2D::Value ImageMapUInt1::sampleSingleChannelHighQual(Coord u, Coord v, size_t channel, bool wrap) const
{
	assert(0);
	return Map2D::Value(0);
}


void ImageMapUInt1::sampleAllChannels(Coord u, Coord v, Value* res_out) const
{
	assert(0);
}


Map2D::Value ImageMapUInt1::getDerivs(Coord s, Coord t, Value& dv_ds_out, Value& dv_dt_out) const
{
	assert(0);
	return Map2D::Value(0);
}


Reference<Map2D> ImageMapUInt1::extractAlphaChannel() const
{
	assert(0);
	return nullptr;
}


Reference<ImageMap<float, FloatComponentValueTraits> > ImageMapUInt1::extractChannelZeroLinear() const
{
	assert(0);
	return nullptr;
}


Reference<Map2D> ImageMapUInt1::extractChannelZero() const
{
	assert(0);
	return nullptr;
}


Reference<Map2D> ImageMapUInt1::resizeMidQuality(const int new_width, const int new_height, glare::TaskManager* task_manager) const
{
	assert(0);
	return nullptr;
}


#if IMAGE_CLASS_SUPPORT

Reference<Image> ImageMapUInt1::convertToImage() const
{
	assert(0);
	return nullptr;
}

#endif // IMAGE_CLASS_SUPPORT


#if MAP2D_FILTERING_SUPPORT

Reference<Map2D> ImageMapUInt1::getBlurredLinearGreyScaleImage(glare::TaskManager& task_manager) const
{
	assert(0);
	return nullptr;
}



Reference<ImageMap<float, FloatComponentValueTraits> > ImageMapUInt1::resizeToImageMapFloat(const int target_width, bool& is_linear_out) const
{
	assert(0);
	return nullptr;
}

#endif // MAP2D_FILTERING_SUPPORT
