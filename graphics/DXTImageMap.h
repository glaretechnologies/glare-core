/*=====================================================================
DXTImageMap.h
-------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


#include "Map2D.h"
#include "Colour4f.h"
#include "image.h"
#include "ImageMap.h"
#include "GaussianImageFilter.h"
#include "../utils/AllocatorVector.h"
#include "../utils/Exception.h"
namespace Indigo { class TaskManager; }
class OutStream;
class InStream;


/*=====================================================================
DXTImageMap
-----------
Image map with DXT compression.

There are more tests in DXTImageMapTests.cpp
=====================================================================*/
class DXTImageMap : public Map2D
{
public:
	inline DXTImageMap();

	inline DXTImageMap(size_t width, size_t height, size_t N); // throws Indigo::Exception
	inline virtual ~DXTImageMap();

	inline DXTImageMap& operator = (const DXTImageMap& other);

	inline bool operator == (const DXTImageMap& other) const;

	void resize(size_t width_, size_t height_, size_t N_); // throws Indigo::Exception
	void resizeNoCopy(size_t width_, size_t height_, size_t N_); // throws Indigo::Exception

	virtual float getGamma() const { return gamma; }
	void setGamma(float g) { gamma = g; }

	// Just for DXTImageMap:
	Vec4i pixelRGBColourBytes(size_t x, size_t y) const;
	uint32 pixelAlphaByte(size_t x, size_t y) const;

	inline virtual const Colour3<Value> pixelColour(size_t x, size_t y) const;
	inline virtual const Value pixelComponent(size_t x, size_t y, size_t c) const;

	// X and Y are normalised image coordinates.
	inline virtual const Colour4f vec3SampleTiled(Coord x, Coord y) const;

	// X and Y are normalised image coordinates.
	inline virtual Value sampleSingleChannelTiled(Coord x, Coord y, size_t channel) const;
	//inline Value scalarSampleTiled(Coord x, Coord y) const { return sampleSingleChannelTiled(x, y, 0); }

	virtual Value getDerivs(Coord s, Coord t, Value& dv_ds_out, Value& dv_dt_out) const;


	inline size_t getWidth() const { return width; }
	inline size_t getHeight() const { return height; }

	inline virtual size_t getMapWidth() const { return width; }
	inline virtual size_t getMapHeight() const { return height; }
	inline virtual size_t numChannels() const { return N; }

	inline virtual bool takesOnlyUnitIntervalValues() const { return true; }

	inline virtual bool hasAlphaChannel() const { return N == 4; }
	inline virtual Reference<Map2D> extractAlphaChannel() const;
	inline virtual bool isAlphaChannelAllWhite() const;

	inline virtual Reference<Image> convertToImage() const;

	inline virtual Reference<Map2D> extractChannelZero() const;

	virtual Reference<ImageMap<float, FloatComponentValueTraits> > extractChannelZeroLinear() const;

	inline virtual Reference<Map2D> getBlurredLinearGreyScaleImage(Indigo::TaskManager& task_manager) const;

	inline virtual Reference<ImageMap<float, FloatComponentValueTraits> > resizeToImageMapFloat(const int target_width, bool& is_linear) const;

	virtual Reference<Map2D> resizeMidQuality(const int new_width, const int new_height, Indigo::TaskManager& task_manager) const { assert(0); return Reference<Map2D>(); }

	virtual size_t getBytesPerPixel() const;

	virtual size_t getByteSize() const;

	// Get num components per pixel.
	inline size_t getN() const { return N; }

	inline size_t numBlocksX() const { return num_blocks_x; }
	
	static Reference<DXTImageMap> compressImageMap(Indigo::TaskManager& task_manager, const ImageMapUInt8& map);

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
