/*=====================================================================
Map2D.h
-------
File created by ClassTemplate on Sun May 18 20:59:34 2008
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "colour3.h"
#include "Colour4f.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
class Image;
class FloatComponentValueTraits;
template<class T, class TTraits> class ImageMap;
namespace Indigo { class TaskManager; }


/*=====================================================================
Map2D
-----
Base class for two-dimensional textures.
Since both core threads and UI threads might be accessing a map concurrently, 
make ThreadSafeRefCounted.
=====================================================================*/
class Map2D : public ThreadSafeRefCounted
{
public:
	/*=====================================================================
	Map2d
	-----
	
	=====================================================================*/
	Map2D();
	virtual ~Map2D();


	typedef float Value;
	typedef float Coord;

	virtual const Colour3<Value> pixelColour(size_t x, size_t y) const = 0;
	virtual const Value pixelComponent(size_t x, size_t y, size_t c) const = 0;

	// X and Y are normalised image coordinates.
	// (X, Y) = (0, 0) is at the bottom left of the image.
	virtual const Colour4f vec3SampleTiled(Coord x, Coord y) const = 0;

	// X and Y are normalised image coordinates.
	virtual Value sampleSingleChannelTiled(Coord x, Coord y, unsigned int channel) const = 0;

	// s and t are normalised image coordinates.
	// Returns texture value (v) at (s, t)
	// Also returns dv/ds and dv/dt.
	virtual Value getDerivs(Coord s, Coord t, Value& dv_ds_out, Value& dv_dt_out) const = 0;


	virtual unsigned int getMapWidth() const = 0;
	virtual unsigned int getMapHeight() const = 0;
	virtual unsigned int numChannels() const = 0;

	virtual bool takesOnlyUnitIntervalValues() const = 0;

	virtual bool hasAlphaChannel() const { return false; }
	virtual Reference<Map2D> extractAlphaChannel() const { return Reference<Map2D>(); }
	virtual bool isAlphaChannelAllWhite() const { return false; }

	virtual Reference<Map2D> extractChannelZero() const = 0;

	virtual Reference<ImageMap<float, FloatComponentValueTraits> > extractChannelZeroLinear() const = 0;

	virtual Reference<Image> convertToImage() const = 0;

	virtual Reference<Map2D> getBlurredLinearGreyScaleImage(Indigo::TaskManager& task_manager) const = 0;


	// Return a new, resized version of this image.
	// Should have maximum dimensions of 'width', while maintaining aspect ratio.
	// Doesn't convert from non-linear to linear space.  (e.g. no gamma is applied)
	// Resizing is medium quality, as it needs to be fast for generating thumbnails for large images.
	// May change number of channels (reduce 4 to 3, or 2 to 1, etc..)
	virtual Reference<ImageMap<float, FloatComponentValueTraits> > resizeToImageMapFloat(const int width, bool& is_linear) const = 0;

	// Return a new, resized version of this image.
	// Scaling is assumed to be mostly the same in each dimension.
	// Resizing is medium quality, as it needs to be fast for large images (env maps)
	virtual Reference<Map2D> resizeMidQuality(const int new_width, const int new_height, Indigo::TaskManager& task_manager) const = 0;

	virtual unsigned int getBytesPerPixel() const = 0;

	virtual float getGamma() const = 0;

private:
	INDIGO_DISABLE_COPY(Map2D)
};


typedef Reference<Map2D> Map2DRef;




