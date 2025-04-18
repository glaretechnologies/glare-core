/*=====================================================================
bitmap.h
--------
File created by ClassTemplate on Wed Oct 27 03:44:34 2004
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "ImageMap.h"
#include "../utils/Platform.h"
#include "../utils/Vector.h"
#include <assert.h>


/*=====================================================================
Bitmap
------
8bit/component bitmap image.
=====================================================================*/
class Bitmap
{
public:
	
	Bitmap();

	/*=====================================================================
	Bitmap
	------
	srcdata may be NULL.
	Throw glare::Exception on mem alloc failure.
	=====================================================================*/
	Bitmap(size_t width, size_t height, size_t bytespp, const uint8* srcdata);

	~Bitmap();

	void resize(size_t newwidth, size_t newheight, size_t new_bytes_pp); // Throws glare::Exception
	void resizeNoCopy(size_t newwidth, size_t newheight, size_t new_bytes_pp); // Throws glare::Exception

	size_t getWidth()   const { return width;   }
	size_t getHeight()  const { return height;  }
	size_t getBytesPP() const { return bytespp; }

	inline uint8* rowPointer(size_t y);

	inline uint8* getPixelNonConst(size_t x, size_t y);
	inline const uint8* getPixel(size_t x, size_t y) const;

	inline unsigned char getPixelComp(size_t x, size_t y, uint32 c) const;
	inline void setPixelComp(size_t x, size_t y, uint32 c, uint8 newval);

	// Blend the current bitmap with white, based on the zeroth channel of img.  Used by Watermarker.
	// TODO: Remove this and use proper alpha compositing.
	void blendImageWithWhite(const Bitmap& img, int destx, int desty, float alpha = 1);

	// Used by TextDrawer.  Assumes getBytesPP() >= 3 and dest.getBytesPP() >= 3.
	void blitToImage(int src_start_x, int src_start_y, int src_end_x, int src_end_y, Bitmap& dest, int destx, int desty) const;

	void setFromImageMap(const ImageMap<uint8, UInt8ComponentValueTraits>& image_map);
	Reference<ImageMap<uint8, UInt8ComponentValueTraits> > toImageMap() const;

	void zero();

	void setAllocator(const Reference<glare::Allocator>& al) { data.setAllocator(al); }
	Reference<glare::Allocator>& getAllocator() { return data.getAllocator(); }

private:
	glare::AllocatorVector<uint8, 16> data;

	size_t width, height;
	size_t bytespp;
};


uint8* Bitmap::rowPointer(size_t y)
{
	assert(y < height);

	return &data[y * width * bytespp];
}


uint8* Bitmap::getPixelNonConst(size_t x, size_t y)
{
	assert(x < width);
	assert(y < height);
	assert((y * width + x) * bytespp < data.size());

	return &data[(y * width + x) * bytespp];
}


const uint8* Bitmap::getPixel(size_t x, size_t y) const
{
	assert(x < width);
	assert(y < height);
	assert((y * width + x) * bytespp < data.size());

	return &data[(y * width + x) * bytespp];
}


uint8 Bitmap::getPixelComp(size_t x, size_t y, uint32 c) const
{
	assert(x < width);
	assert(y < height);
	assert(c < bytespp);

	return data[(y*width + x) * bytespp + c];
}


void Bitmap::setPixelComp(size_t x, size_t y, uint32 c, uint8 newval)
{
	assert(x < width);
	assert(y < height);
	assert(c < bytespp);

	data[(y*width + x) * bytespp + c] = newval;
}
