/*=====================================================================
DXTImageMap.h
-------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


#include "Map2D.h"
#include "Colour4f.h"
#include "ImageMap.h"
#include "../utils/AllocatorVector.h"
namespace glare { class TaskManager; }
class Image;


/*=====================================================================
DXTImageMap
-----------
Image map with DXT compression.

There are more tests in DXTImageMapTests.cpp
=====================================================================*/
class DXTImageMap : public Map2D
{
public:
	DXTImageMap();
	DXTImageMap(size_t width, size_t height, size_t N); // throws glare::Exception
	virtual ~DXTImageMap();

	DXTImageMap& operator = (const DXTImageMap& other);

	bool operator == (const DXTImageMap& other) const;

	void resize(size_t width_, size_t height_, size_t N_); // throws glare::Exception
	void resizeNoCopy(size_t width_, size_t height_, size_t N_); // throws glare::Exception

	virtual float getGamma() const { return gamma; }
	void setGamma(float g) { gamma = g; }

	// Just for DXTImageMap:
	Vec4i decodePixelRGBColour(size_t x, size_t y) const;
	uint32 decodePixelRed(size_t x, size_t y) const;
	uint32 decodePixelAlpha(size_t x, size_t y) const;

	virtual const Colour4f pixelColour(size_t x, size_t y) const;

	// X and Y are normalised image coordinates.
	virtual const Colour4f vec3Sample(Coord x, Coord y, bool wrap) const override;

	// X and Y are normalised image coordinates.
	// Used by TextureDisplaceMatParameter<>::eval(), for displacement and blend factor evaluation (channel 0) and alpha evaluation (channel N-1)
	virtual Value sampleSingleChannelTiled(Coord x, Coord y, size_t channel) const;
	//inline Value scalarSampleTiled(Coord x, Coord y) const { return sampleSingleChannelTiled(x, y, 0); }

	virtual Value sampleSingleChannelHighQual(Coord x, Coord y, size_t channel, bool wrap) const override;

	virtual Value getDerivs(Coord s, Coord t, Value& dv_ds_out, Value& dv_dt_out) const;


	inline size_t getWidth() const { return width; }
	inline size_t getHeight() const { return height; }

	virtual size_t getMapWidth() const { return width; }
	virtual size_t getMapHeight() const { return height; }
	virtual size_t numChannels() const { return N; }

	virtual bool takesOnlyUnitIntervalValues() const { return true; }

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

	virtual bool isDXTImageMap() const { return true; }

	// Get num components per pixel.
	inline size_t getN() const { return N; }

	inline size_t numBlocksX() const { return num_blocks_x; }
	
	static Reference<DXTImageMap> compressImageMap(glare::TaskManager& task_manager, const ImageMapUInt8& map);

	const uint64* getData() const { return data.data(); }

	void setAllocator(const Reference<glare::Allocator>& al) { data.setAllocator(al); }
	Reference<glare::Allocator>& getAllocator() { return data.getAllocator(); }

	static void test();

private:
	size_t width, height, N;
	size_t num_blocks_x, num_blocks_y;
	size_t log2_uint64s_per_block; // 0 for N=3, 1 for N=4.
	glare::AllocatorVector<uint64, 16> data;
	float gamma, ds_over_2, dt_over_2;
};


typedef Reference<DXTImageMap> DXTImageMapRef;
