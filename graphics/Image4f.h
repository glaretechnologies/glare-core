/*=====================================================================
Image4f.h
---------
Copyright Glare Technologies Limited 2013 - 
=====================================================================*/
#pragma once


#include "Map2D.h"
#include "Colour4f.h"
#include "../utils/array2d.h"
class Bitmap;
class FilterFunction;
//#include "ImageMap.h"
//typedef ImageMap<float, FloatComponentValueTraits> Image4f;


/*=====================================================================
Image4f
-------
A floating point quad-component image class.
Each component is stored as a 32-bit float.
=====================================================================*/
class Image4f //  : public Map2D
{
public:
	Image4f();
	Image4f(size_t width, size_t height);
	~Image4f();

	Image4f& operator = (const Image4f& other);

	typedef Colour4f ColourType;

	void setFromBitmap(const Bitmap& bmp, float image_gamma); // will throw Indigo::Exception if bytespp != 3 or 4

	void copyRegionToBitmap(Bitmap& bmp_out, int x1, int y1, int x2, int y2) const; // will throw ImageExcep if bytespp != 3 && bytespp != 4

	void copyToBitmap(Bitmap& bmp_out) const;

	inline size_t getHeight() const { return pixels.getHeight(); }
	inline size_t getWidth()  const { return pixels.getWidth(); }
	inline size_t numPixels() const { return pixels.getWidth() * pixels.getHeight(); }

	INDIGO_STRONG_INLINE const ColourType& getPixel(size_t x, size_t y) const;
	INDIGO_STRONG_INLINE ColourType& getPixel(size_t x, size_t y);

	INDIGO_STRONG_INLINE const ColourType& getPixel(size_t i) const;
	INDIGO_STRONG_INLINE ColourType& getPixel(size_t i);

	inline const ColourType& getPixelTiled(int x, int y) const;

	inline void setPixel(size_t x, size_t y, const ColourType& colour);
	inline void incrPixel(size_t x, size_t y, const ColourType& colour);

	void loadFromHDR(const std::string& pathname, int width, int height);

	void zero();
	void set(float s);

	void resize(size_t newwidth, size_t newheight);

	void posClamp();
	void clampInPlace(float min, float max);

	void gammaCorrect(float exponent);

	void blitToImage(Image4f& dest, int destx, int desty) const;
	void blitToImage(int src_start_x, int src_start_y, int src_end_x, int src_end_y, Image4f& dest, int destx, int desty) const;
	void addImage(const Image4f& other, int destx, int desty, float alpha = 1);
	void subImage(const Image4f& dest, int destx, int desty);
	void blendImage(const Image4f& dest, int destx, int desty, const Colour4f& colour);
	
	void overwriteImage(const Image& src, int destx, int desty);

	void scale(float factor);

	void normalise();//make the average pixel luminance == 1

	enum DOWNSIZE_FILTER
	{
		DOWNSIZE_FILTER_MN_CUBIC,
		DOWNSIZE_FILTER_BOX
	};

	void collapseSizeBoxFilter(int factor); // trims off border before collapsing
	//void collapseImage(int factor, int border_width, DOWNSIZE_FILTER filter_type, double mn_B, double mn_C);

	static void collapseImage(int factor, int border_width, const FilterFunction& filter_function, float max_component_value, const Image4f& in, Image4f& out);

	static void downsampleImage(const ptrdiff_t factor, const ptrdiff_t border_width,
								const ptrdiff_t filter_span, const float * const resize_filter, const float pre_clamp,
								const Image4f& img_in, Image4f& img_out, Indigo::TaskManager& task_manager);

	size_t getByteSize() const;

	float minLuminance() const;
	float maxLuminance() const;
	double averageLuminance() const;

	//static void buildRGBFilter(const Image& original_filter, const Vec3d& filter_scales, Image& result_out);
	//void convolve(const Image& filter, Image& result_out) const;

	float minPixelComponent() const;
	float maxPixelComponent() const;

	////// Map2D interface //////////
	/*virtual unsigned int getMapWidth() const { return (unsigned int)getWidth(); }
	virtual unsigned int getMapHeight() const { return (unsigned int)getHeight(); }

	virtual const Colour3<Value> pixelColour(size_t x, size_t y) const { return Colour3<Value>(pixels.elem(x, y).x[0], pixels.elem(x, y).x[1], pixels.elem(x, y).x[2]); }

	virtual const Colour3<Value> vec3SampleTiled(Coord x, Coord y) const;

	virtual Value scalarSampleTiled(Coord x, Coord y) const;

	virtual bool takesOnlyUnitIntervalValues() const { return false; }

	virtual Reference<Image> convertToImage() const;

	virtual Reference<Map2D> getBlurredLinearGreyScaleImage(Indigo::TaskManager& task_manager) const;

	virtual Reference<Map2D> resizeToImage(const int width, bool& is_linear) const;

	virtual unsigned int getBytesPerPixel() const;*/
	/////////////////////////////////

	static void test();

private:
	Array2d<ColourType> pixels;
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


void Image4f::incrPixel(size_t x, size_t y, const ColourType& colour)
{
	pixels.elem(x, y) += colour;
}
