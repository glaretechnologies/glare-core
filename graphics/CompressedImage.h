/*=====================================================================
CompressedImage.h
-----------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "Map2D.h"
#include "image.h"
#include "ImageMap.h"
#include "../utils/AllocatorVector.h"
namespace glare { class TaskManager; }


/*=====================================================================
CompressedImage
---------------
Image with block-based compression, possibly with mip-maps.

For example for BPTC Texture Compression: https://www.khronos.org/opengl/wiki/BPTC_Texture_Compression

This class pretty much just holds compressed data, for uploading to OpenGL.
=====================================================================*/
class CompressedImage : public Map2D
{
public:
	CompressedImage(size_t width, size_t height, size_t N);
	virtual ~CompressedImage();

	
	//====================== Map2D interface ======================
	virtual const Colour4f pixelColour(size_t x, size_t y) const;

	// X and Y are normalised image coordinates.
	virtual const Colour4f vec3SampleTiled(Coord x, Coord y) const;

	// X and Y are normalised image coordinates.
	// Used by TextureDisplaceMatParameter<>::eval(), for displacement and blend factor evaluation (channel 0) and alpha evaluation (channel N-1)
	virtual Value sampleSingleChannelTiled(Coord x, Coord y, size_t channel) const;

	virtual Value sampleSingleChannelTiledHighQual(Coord x, Coord y, size_t channel) const;


	virtual Value getDerivs(Coord s, Coord t, Value& dv_ds_out, Value& dv_dt_out) const;

	virtual size_t getMapWidth() const { return width; }
	virtual size_t getMapHeight() const { return height; }
	virtual size_t numChannels() const { return N; }

	virtual bool takesOnlyUnitIntervalValues() const { return false; }

	virtual bool hasAlphaChannel() const { return N == 4; }
	virtual Reference<Map2D> extractAlphaChannel() const;
	virtual bool isAlphaChannelAllWhite() const;

	virtual Reference<Image> convertToImage() const;

	virtual Reference<Map2D> extractChannelZero() const;

	virtual Reference<ImageMap<float, FloatComponentValueTraits> > extractChannelZeroLinear() const;

	virtual Reference<Map2D> getBlurredLinearGreyScaleImage(glare::TaskManager& task_manager) const;

	virtual Reference<ImageMap<float, FloatComponentValueTraits> > resizeToImageMapFloat(const int target_width, bool& is_linear) const;

	virtual Reference<Map2D> resizeMidQuality(const int new_width, const int new_height, glare::TaskManager* task_manager) const;

	virtual size_t getBytesPerPixel() const;

	virtual size_t getByteSize() const;

	virtual float getGamma() const { return gamma; }

	virtual bool isDXTImageMap() const { return false; }
	//====================== End Map2D interface ======================
	
	// Get num components per pixel.
	inline size_t getN() const { return N; }

	void setAllocator(const Reference<glare::Allocator>& al) { mipmap_level_data.setAllocator(al); }
	Reference<glare::Allocator>& getAllocator() { return mipmap_level_data.getAllocator(); }

	static void test();

//private:
	size_t width, height, N;
	glare::AllocatorVector<glare::AllocatorVector<uint8, 16>, 16> mipmap_level_data;
	float gamma, ds_over_2, dt_over_2;

	uint32 gl_type;
	uint32 gl_type_size;
	uint32 gl_internal_format;
	uint32 gl_format;
};


typedef Reference<CompressedImage> CompressedImageRef;
