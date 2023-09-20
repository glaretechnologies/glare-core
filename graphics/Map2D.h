/*=====================================================================
Map2D.h
-------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


#include "Colour4f.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
class Image;
class FloatComponentValueTraits;
template<class T, class TTraits> class ImageMap;
namespace glare { class TaskManager; }


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
	Map2D();
	virtual ~Map2D();

	typedef float Value;
	typedef float Coord;

	// Although this returns an SSE 4-vector, only the first three RGB components will be set.
	virtual const Colour4f pixelColour(size_t x, size_t y) const = 0;

	// X and Y are normalised image coordinates.
	// (X, Y) = (0, 0) is at the bottom left of the image.
	// Although this returns an SSE 4-vector, only the first three RGB components will be set.
	virtual const Colour4f vec3SampleTiled(Coord x, Coord y) const = 0;

	// X and Y are normalised image coordinates.
	// Used by TextureDisplaceMatParameter<>::eval(), for displacement and blend factor evaluation (channel 0) and alpha evaluation (channel N-1)
	virtual Value sampleSingleChannelTiled(Coord x, Coord y, size_t channel) const = 0;

	virtual Value sampleSingleChannelTiledHighQual(Coord x, Coord y, size_t channel) const = 0;


	// s and t are normalised image coordinates.
	// Returns texture value (v) at (s, t)
	// Also returns dv/ds and dv/dt.
	virtual Value getDerivs(Coord s, Coord t, Value& dv_ds_out, Value& dv_dt_out) const = 0;


	virtual size_t getMapWidth() const = 0;
	virtual size_t getMapHeight() const = 0;
	virtual size_t numChannels() const = 0;

	virtual bool takesOnlyUnitIntervalValues() const = 0;

	virtual bool hasAlphaChannel() const { return false; }
	virtual Reference<Map2D> extractAlphaChannel() const { return Reference<Map2D>(); }
	virtual bool isAlphaChannelAllWhite() const { return false; }

	virtual Reference<Map2D> extractChannelZero() const = 0;

	virtual Reference<ImageMap<float, FloatComponentValueTraits> > extractChannelZeroLinear() const = 0;

	virtual Reference<Image> convertToImage() const = 0;

	// Put various Map2D functions behind the MAP2D_FILTERING_SUPPORT flag.  
	// This is so programs can use Map2D, ImageMap etc..without having to compile in TaskManager support, GaussianImageFilter support etc..
#if MAP2D_FILTERING_SUPPORT
	virtual Reference<Map2D> getBlurredLinearGreyScaleImage(glare::TaskManager& task_manager) const = 0;

	// Return a new, resized version of this image.
	// Should have maximum dimensions of 'width', while maintaining aspect ratio.
	// Doesn't convert from non-linear to linear space.  (e.g. no gamma is applied)
	// Resizing is medium quality, as it needs to be fast for generating thumbnails for large images.
	// May change number of channels (reduce 4 to 3, or 2 to 1, etc..)
	virtual Reference<ImageMap<float, FloatComponentValueTraits> > resizeToImageMapFloat(const int width, bool& is_linear) const = 0;

	// Return a new, resized version of this image.
	// Scaling is assumed to be mostly the same in each dimension.
	// Resizing is medium quality, as it needs to be fast for large images (env maps)
	virtual Reference<Map2D> resizeMidQuality(const int new_width, const int new_height, glare::TaskManager* task_manager) const = 0;
#endif

	virtual size_t getBytesPerPixel() const = 0; // Get the uncompressed number of bytes per pixel.

	virtual size_t getByteSize() const = 0; // Get total size of image in bytes.  Returns the compressed size if the image is compressed.

	virtual float getGamma() const = 0;

	virtual bool isDXTImageMap() const { return false; }

private:
	GLARE_DISABLE_COPY(Map2D)
};


typedef Reference<Map2D> Map2DRef;
