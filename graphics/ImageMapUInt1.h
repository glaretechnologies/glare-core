/*=====================================================================
ImageMapUInt1.h
---------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "Map2D.h"
#include "../utils/BitVector.h"
namespace glare { class TaskManager; }
namespace glare { class Allocator; }


/*=====================================================================
ImageMapUInt1
-------------
1 channel of 1 bit image map.
Most methods aren't implemented, sampleSingleChannelTiled is though.
=====================================================================*/
class ImageMapUInt1 final : public Map2D
{
public:
	ImageMapUInt1();
	ImageMapUInt1(size_t width, size_t height, glare::Allocator* mem_allocator = NULL);
	virtual ~ImageMapUInt1();

	ImageMapUInt1& operator = (const ImageMapUInt1& other);

	bool operator == (const ImageMapUInt1& other) const;

	void zero();

	void resize(size_t width_, size_t height_);
	void resizeNoCopy(size_t width_, size_t height_);

	virtual float getGamma() const override { return 1.0f; }

	virtual const Colour4f pixelColour(size_t x, size_t y) const override;

	// X and Y are normalised image coordinates.
	inline virtual const Colour4f vec3Sample(Coord x, Coord y, bool wrap) const override;

	// X and Y are normalised image coordinates.
	virtual Value sampleSingleChannelTiled(Coord x, Coord y, size_t channel) const override;
	Value scalarSampleTiled(Coord x, Coord y) const { return sampleSingleChannelTiled(x, y, 0); }

	virtual Value sampleSingleChannelHighQual(Coord x, Coord y, size_t channel, bool wrap) const override;

	void sampleAllChannels(Coord x, Coord y, Value* res_out) const;

	virtual Value getDerivs(Coord s, Coord t, Value& dv_ds_out, Value& dv_dt_out) const override;


	size_t getWidth() const { return width; }
	size_t getHeight() const { return height; }

	virtual size_t getMapWidth() const override { return width; }
	virtual size_t getMapHeight() const override { return height; }
	virtual size_t numChannels() const override { return 1; }
	virtual double uncompressedBitsPerChannel() const override { return 1; }

	virtual bool takesOnlyUnitIntervalValues() const override { return true; }

	virtual bool hasAlphaChannel() const override { return false; }
	virtual Reference<Map2D> extractAlphaChannel() const override;
	virtual bool isAlphaChannelAllWhite() const override { return false; }

#if IMAGE_CLASS_SUPPORT
	virtual Reference<Image> convertToImage() const override;
#endif

	virtual Reference<Map2D> extractChannelZero() const override;
	

	virtual Reference<ImageMap<float, FloatComponentValueTraits> > extractChannelZeroLinear() const override;
#if MAP2D_FILTERING_SUPPORT
	virtual Reference<Map2D> getBlurredLinearGreyScaleImage(glare::TaskManager& task_manager) const override;

	virtual Reference<ImageMap<float, FloatComponentValueTraits> > resizeToImageMapFloat(const int target_width, bool& is_linear) const override;

	// task_manager may be NULL
	virtual Reference<Map2D> resizeMidQuality(const int new_width, const int new_height, glare::TaskManager* task_manager) const override;
#endif
	virtual size_t getByteSize() const override { return data.byteSize(); }


	inline void setPixelValue(size_t x, size_t y, uint32 val)
	{
		assert(val == 0 || val == 1);
		data.setBit(x + y*height, val);
	}

	inline void setPixelValue(size_t pixel_i, uint32 val)
	{
		assert(val == 0 || val == 1);
		data.setBit(pixel_i, val);
	}

	size_t width, height;
	BitVector data;
};
