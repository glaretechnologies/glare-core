/*=====================================================================
ImageMapSequence.h
------------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


#include "ImageMap.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
class Image;
class FloatComponentValueTraits;
template<class T, class TTraits> class ImageMap;
namespace glare { class TaskManager; }



template <class V, class ComponentValueTraits>
class ImageMapSequence final : public Map2D
{
public:
	virtual ~ImageMapSequence() {}


	// Although this returns an SSE 4-vector, only the first three RGB components will be set.
	virtual const Colour4f pixelColour(size_t /*x*/, size_t /*y*/) const override { assert(0); return Colour4f(0.f); }

	// X and Y are normalised image coordinates.
	// (X, Y) = (0, 0) is at the bottom left of the image.
	// Although this returns an SSE 4-vector, only the first three RGB components will be set.
	virtual const Colour4f vec3Sample(Coord /*x*/, Coord /*y*/, bool /*wrap*/) const override { assert(0); return Colour4f(0.f); }

	// X and Y are normalised image coordinates.
	// Used by TextureDisplaceMatParameter<>::eval(), for displacement and blend factor evaluation (channel 0) and alpha evaluation (channel N-1)
	virtual Value sampleSingleChannelTiled(Coord /*x*/, Coord /*y*/, size_t /*channel*/) const override { assert(0); return 0.f; }

	virtual Value sampleSingleChannelHighQual(Coord /*x*/, Coord /*y*/, size_t /*channel*/, bool /*wrap*/) const override { assert(0); return 0.f; }

	// s and t are normalised image coordinates.
	// Returns texture value (v) at (s, t)
	// Also returns dv/ds and dv/dt.
	virtual Value getDerivs(Coord /*s*/, Coord /*t*/, Value& /*dv_ds_out*/, Value& /*dv_dt_out*/) const override { assert(0); return 0.f; }


	virtual size_t getMapWidth() const override { assert(0); return 1; }
	virtual size_t getMapHeight() const override { assert(0); return 1; }
	virtual size_t numChannels() const override { assert(0); return 1; }

	virtual bool takesOnlyUnitIntervalValues() const override { assert(0); return true; }

	virtual Reference<Map2D> extractChannelZero() const override { assert(0); return NULL; }

	virtual Reference<ImageMap<float, FloatComponentValueTraits> > extractChannelZeroLinear() const override { assert(0); return NULL; }

	virtual Reference<Image> convertToImage() const override { assert(0); return NULL; }

	// Put various Map2D functions behind the MAP2D_FILTERING_SUPPORT flag.  
	// This is so programs can use Map2D, ImageMap etc..without having to compile in TaskManager support, GaussianImageFilter support etc..
#if MAP2D_FILTERING_SUPPORT
	virtual Reference<Map2D> getBlurredLinearGreyScaleImage(glare::TaskManager& task_manager) const override { assert(0); return NULL; }

	// Return a new, resized version of this image.
	// Should have maximum dimensions of 'width', while maintaining aspect ratio.
	// Doesn't convert from non-linear to linear space.  (e.g. no gamma is applied)
	// Resizing is medium quality, as it needs to be fast for generating thumbnails for large images.
	// May change number of channels (reduce 4 to 3, or 2 to 1, etc..)
	virtual Reference<ImageMap<float, FloatComponentValueTraits> > resizeToImageMapFloat(const int width, bool& is_linear) const override { assert(0); return NULL; }

	// Return a new, resized version of this image.
	// Scaling is assumed to be mostly the same in each dimension.
	// Resizing is medium quality, as it needs to be fast for large images (env maps)
	virtual Reference<Map2D> resizeMidQuality(const int new_width, const int new_height, glare::TaskManager* task_manager) const override { assert(0); return NULL; }
#endif

	virtual size_t getByteSize() const override { assert(0); return 1; }; // Get total size of image in bytes.  Returns the compressed size if the image is compressed.

	virtual float getGamma() const override { assert(0); return 1; };


	std::vector<Reference<ImageMap<uint8, UInt8ComponentValueTraits> > > images;

	std::vector<double> frame_durations;
	std::vector<double> frame_start_times;

private:
	//GLARE_DISABLE_COPY(Map2D)
};


typedef ImageMapSequence<uint8, UInt8ComponentValueTraits> ImageMapSequenceUInt8;
typedef Reference<ImageMapSequenceUInt8> ImageMapSequenceUInt8Ref;
