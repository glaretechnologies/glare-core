/*=====================================================================
SOGDecoder.h
------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "GaussianSplatData.h"
#include "../physics/jscol_aabbox.h"
#include <string>
namespace glare { class Allocator; }


/*=====================================================================
SOGDecoder
----------
Decodes SOG (PlayCanvas "Spatially Ordered Gaussians") Gaussian splat files.
See https://developer.playcanvas.com/user-manual/gaussian-splatting/formats/sog/

A .sog file is a ZIP archive containing a meta.json describing the splat
attributes, plus a WebP image per attribute, with one pixel per splat.  The
attribute values are quantised into those images: positions as 16-bit values
split over a low and a high image, scales and base colours as 8-bit indices
into codebooks in meta.json, and rotations with the "smallest three"
quaternion encoding.

Only the bundled (single-file) layout is handled, not the unbundled layout of
a loose meta.json alongside loose .webp files.

Only the DC term (sh0) of the spherical harmonics is decoded - the shN data,
if present, is ignored, so the resulting colours are view-independent.
=====================================================================*/
class SOGDecoder
{
public:
	// All methods throw glare::Exception on failure.

	static GaussianSplatDataRef decode(const std::string& path, glare::Allocator* mem_allocator = NULL);

	// data/size is the contents of a .sog file.
	static GaussianSplatDataRef decodeFromBuffer(const void* data, size_t size, glare::Allocator* mem_allocator = NULL);


	struct MetaSummary
	{
		js::AABBox aabb_os;
		size_t num_splats; // Zero if the file didn't record a splat count.
	};

	// Reads just meta.json out of the file and derives the object-space bounds and splat count from it, without decoding
	// any of the (much larger) WebP images.  Useful for sizing or placing a splat cloud before committing to decoding it.
	//
	// Note that the bounds returned here are derived from the quantisation range recorded in meta.json, so they can be
	// very slightly looser than the bounds decode() computes from the actual splat positions, but never tighter.
	static MetaSummary readMetaSummaryFromBuffer(const void* data, size_t size);

	static void test();
};
