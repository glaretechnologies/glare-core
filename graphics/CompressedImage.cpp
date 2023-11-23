/*=====================================================================
CompressedImage.cpp
-------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "CompressedImage.h"


#include "ImageMap.h"
#include "../utils/Exception.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


CompressedImage::CompressedImage(size_t width_, size_t height_, size_t N_)
:	width(width_), height(height_), N(N_), gamma(2.2f), ds_over_2(0.5f / width_), dt_over_2(0.5f / height_),
	gl_type(0),
	gl_type_size(0),
	gl_internal_format(0),
	gl_format(0)
{
	assert(N == 3 || N == 4);
}


CompressedImage::~CompressedImage() {}


const Colour4f CompressedImage::pixelColour(size_t /*x*/, size_t /*y*/) const
{
	assert(0);
	return Colour4f(0.f);
}


// X and Y are normalised image coordinates.
// (X, Y) = (0, 0) is at the bottom left of the image.
// Although this returns an SSE 4-vector, only the first three RGB components will be set.
const Colour4f CompressedImage::vec3Sample(Coord /*u*/, Coord /*v*/, bool /*wrap*/) const
{
	assert(0);
	return Colour4f(0.f);
}


// Used by TextureDisplaceMatParameter<>::eval(), for displacement and blend factor evaluation (channel 0) and alpha evaluation (channel N-1)
Map2D::Value CompressedImage::sampleSingleChannelTiled(Coord /*u*/, Coord /*v*/, size_t /*channel*/) const
{
	assert(0);
	return 0.f;
}


Map2D::Value CompressedImage::sampleSingleChannelHighQual(Coord /*x*/, Coord /*y*/, size_t /*channel*/, bool /*wrap*/) const
{
	assert(0);
	return 0.f;
}


// s and t are normalised image coordinates.
// Returns texture value (v) at (s, t)
// Also returns dv/ds and dv/dt.
Map2D::Value CompressedImage::getDerivs(Coord /*s*/, Coord /*t*/, Value& dv_ds_out, Value& dv_dt_out) const
{
	dv_ds_out = dv_dt_out = 0;
	assert(0);
	return 0.f;
}


Reference<Map2D> CompressedImage::extractAlphaChannel() const
{
	assert(0);
	return Reference<Map2D>();
}


bool CompressedImage::isAlphaChannelAllWhite() const
{
	return true;
}


Reference<Map2D> CompressedImage::extractChannelZero() const
{
	assert(0);
	return Reference<Map2D>();
}


Reference<ImageMapFloat> CompressedImage::extractChannelZeroLinear() const
{
	assert(0);
	return Reference<ImageMapFloat>();
}


Reference<Image> CompressedImage::convertToImage() const
{
	assert(0);
	return Reference<Image>();
}


#if MAP2D_FILTERING_SUPPORT
Reference<Map2D> CompressedImage::getBlurredLinearGreyScaleImage(glare::TaskManager& /*task_manager*/) const
{
	assert(0);
	return Reference<Map2D>();
}


Reference<ImageMap<float, FloatComponentValueTraits> > CompressedImage::resizeToImageMapFloat(const int /*target_width*/, bool& is_linear_out) const
{
	assert(0);
	is_linear_out = false;
	return Reference<ImageMapFloat>();
}


Reference<Map2D> CompressedImage::resizeMidQuality(const int /*new_width*/, const int /*new_height*/, glare::TaskManager* /*task_manager*/) const
{ 
	assert(0); 
	return Reference<Map2D>();
}
#endif


size_t CompressedImage::getBytesPerPixel() const
{
	return N; // NOTE: this gives the uncompressed size.
}


size_t CompressedImage::getByteSize() const
{
	size_t sum = 0;
	for(size_t i=0; i<mipmap_level_data.size(); ++i)
		sum += mipmap_level_data[i].dataSizeBytes();
	return sum;
}


#if BUILD_TESTS


#include "../indigo/globals.h"
#include "../utils/TestUtils.h"


void CompressedImage::test()
{
	conPrint("CompressedImage::test()");

}


#endif // BUILD_TESTS
