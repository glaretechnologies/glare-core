/*=====================================================================
Image4f.h
---------
Copyright Glare Technologies Limited 2019 - 
=====================================================================*/
#pragma once


#include <stddef.h>
#include "Colour4f.h"
#include "../utils/Array2D.h"
#include "../utils/TaskManager.h"
class Bitmap;
class FilterFunction;


/*=====================================================================
Image4f
-------
A floating point quad-component image class.
Each component is stored as a 32-bit float.
=====================================================================*/
class Image4f
{
public:
	Image4f();
	Image4f(size_t width, size_t height); // throws glare::Exception
	~Image4f();

	Image4f& operator = (const Image4f& other);

	typedef Colour4f ColourType;

	void setFromBitmap(const Bitmap& bmp, float image_gamma); // will throw glare::Exception if bytespp != 1, 3 or 4

	void copyRegionToBitmap(Bitmap& bmp_out, int x1, int y1, int x2, int y2) const; // will throw ImageExcep if bytespp != 3 && bytespp != 4

	void copyToBitmap(Bitmap& bmp_out) const;
	void copyToBitmapSetAlphaTo255(Bitmap& bmp_out) const;

	inline size_t getHeight() const { return pixels.getHeight(); }
	inline size_t getWidth()  const { return pixels.getWidth(); }
	inline size_t numPixels() const { return pixels.getWidth() * pixels.getHeight(); }

	inline const ColourType* getPixelData() const { return pixels.getData(); }
	inline ColourType* getPixelData() { return pixels.getData(); }

	GLARE_STRONG_INLINE const ColourType& getPixel(size_t x, size_t y) const;
	GLARE_STRONG_INLINE ColourType& getPixel(size_t x, size_t y);

	GLARE_STRONG_INLINE const ColourType& getPixel(size_t i) const;
	GLARE_STRONG_INLINE ColourType& getPixel(size_t i);

	inline void setPixel(size_t x, size_t y, const ColourType& colour);

	void zero();
	void set(float s);

	void resize(size_t newwidth, size_t newheight); // throws glare::Exception
	void resizeNoCopy(size_t newwidth, size_t newheight); // Resize without copying existing data.

	void blitToImage(Image4f& dest, int destx, int desty) const;
	void blitToImage(int src_start_x, int src_start_y, int src_end_x, int src_end_y, Image4f& dest, int destx, int desty) const;
	void addImage(const Image4f& other, int destx, int desty, float alpha = 1);
	void subImage(const Image4f& dest, int destx, int desty);
	void blendImage(const Image4f& dest, int destx, int desty, const Colour4f& colour);
	

	void collapseSizeBoxFilter(int factor); // trims off border before collapsing

	static void collapseImage(int factor, int border_width, const FilterFunction& filter_function, float max_component_value, const Image4f& in, Image4f& out);

	static void downsampleImage(const ptrdiff_t factor, const ptrdiff_t border_width,
								const ptrdiff_t filter_span, const float * const resize_filter,
								const Image4f& img_in, Image4f& img_out, glare::TaskManager& task_manager);

	double averageLuminance() const;

	float minPixelComponent() const;
	float maxPixelComponent() const;

	static void test();

private:
	Array2D<ColourType> pixels;
};


const Image4f::ColourType& Image4f::getPixel(size_t i) const
{
	assert(i < numPixels());
	return pixels.getData()[i];
}


Image4f::ColourType& Image4f::getPixel(size_t i)
{
	assert(i < numPixels());
	return pixels.getData()[i];
}


const Image4f::ColourType& Image4f::getPixel(size_t x, size_t y) const
{
	assert(x < pixels.getWidth() && y < pixels.getHeight());

	return pixels.elem(x, y);
}


Image4f::ColourType& Image4f::getPixel(size_t x, size_t y)
{
	assert(x < pixels.getWidth() && y < pixels.getHeight());

	return pixels.elem(x, y);
}


void Image4f::setPixel(size_t x, size_t y, const ColourType& colour)
{
	pixels.elem(x, y) = colour;
}
