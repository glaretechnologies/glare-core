/*=====================================================================
CompressedImage.h
-----------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "Map2D.h"
#include "image.h"
#include "ImageMap.h"
#include "TextureData.h"
#include "../utils/AllocatorVector.h"
namespace glare { class TaskManager; }


/*=====================================================================
CompressedImage
---------------
Image with block-based compression, possibly with mip-maps.

For example for BPTC Texture Compression: https://www.khronos.org/opengl/wiki/BPTC_Texture_Compression

This class pretty much just holds compressed data, for uploading to OpenGL.
=====================================================================*/
class CompressedImage final : public Map2D
{
public:
	CompressedImage(size_t width, size_t height, OpenGLTextureFormat format);
	virtual ~CompressedImage();

	
	//====================== Map2D interface ======================
	virtual const Colour4f pixelColour(size_t x, size_t y) const override;

	// X and Y are normalised image coordinates.
	virtual const Colour4f vec3Sample(Coord x, Coord y, bool wrap) const override;

	// X and Y are normalised image coordinates.
	// Used by TextureDisplaceMatParameter<>::eval(), for displacement and blend factor evaluation (channel 0) and alpha evaluation (channel N-1)
	virtual Value sampleSingleChannelTiled(Coord x, Coord y, size_t channel) const override;

	virtual Value sampleSingleChannelHighQual(Coord x, Coord y, size_t channel, bool wrap) const override;


	virtual Value getDerivs(Coord s, Coord t, Value& dv_ds_out, Value& dv_dt_out) const override;

	virtual size_t getMapWidth() const override { return texture_data->W; }
	virtual size_t getMapHeight() const override { return texture_data->H; }
	virtual size_t numChannels() const override { return texture_data->numChannels(); }
	virtual double uncompressedBitsPerChannel() const override { return texture_data->uncompressedBitsPerChannel(); }

	virtual bool takesOnlyUnitIntervalValues() const override { return false; }

	virtual bool hasAlphaChannel() const override { return numChannels() == 4; }
	virtual Reference<Map2D> extractAlphaChannel() const override;
	virtual bool isAlphaChannelAllWhite() const override;

#if IMAGE_CLASS_SUPPORT
	virtual Reference<Image> convertToImage() const override;
#endif

	virtual Reference<Map2D> extractChannelZero() const override;

	virtual Reference<ImageMap<float, FloatComponentValueTraits> > extractChannelZeroLinear() const override;
#if MAP2D_FILTERING_SUPPORT
	virtual Reference<Map2D> getBlurredLinearGreyScaleImage(glare::TaskManager& task_manager) const override;

	virtual Reference<ImageMap<float, FloatComponentValueTraits> > resizeToImageMapFloat(const int target_width, bool& is_linear) const override;

	virtual Reference<Map2D> resizeMidQuality(const int new_width, const int new_height, glare::TaskManager* task_manager) const override;
#endif
	virtual size_t getByteSize() const override;

	virtual float getGamma() const override { return 2.2f; } //gamma; }

	virtual bool isDXTImageMap() const override { return false; }
	//====================== End Map2D interface ======================
	
	void setAllocator(const Reference<glare::Allocator>& al) {texture_data->setAllocator(al); }
	// Reference<glare::Allocator>& getAllocator() { return texture_data->getAllocator(); }

	static void test();

	Reference<TextureData> texture_data;
};


typedef Reference<CompressedImage> CompressedImageRef;
