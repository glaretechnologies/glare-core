/*=====================================================================
WebPDecoder.h
-------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include "../utils/Reference.h"
#include <string>
class Map2D;
namespace glare { class Allocator; }


/*=====================================================================
WebPDecoder
-----------
Loading of WebP files using libwebp.
See https://developers.google.com/speed/webp/docs/api

Decoded images have 3 components (RGB) if the file has no alpha channel,
or 4 components (RGBA) if it does.

Animated WebP files are supported: decode() returns the first frame of the
animation, decodeImageSequence() returns all frames.
=====================================================================*/
class WebPDecoder
{
public:
	// All methods throw ImFormatExcep on failure.

	// Information read from a file's header, without decoding any image data.
	struct ImageInfo
	{
		int width, height; // For an animation, the canvas dimensions.
		bool has_alpha; // If true, decode() will return a 4-component image, otherwise a 3-component one.
		bool has_animation;
	};

	// Reads just the header of a WebP file.  Much cheaper than decoding it, so useful for checking that a file is the
	// expected size and format before committing to decoding it.
	static ImageInfo getInfoFromBuffer(const void* data, size_t size);

	static Reference<Map2D> decode(const std::string& path, glare::Allocator* mem_allocator = NULL);
	static Reference<Map2D> decodeFromBuffer(const void* data, size_t size, bool return_animated_webp_as_sequence, glare::Allocator* mem_allocator);

	// Returns an ImageMapSequenceUInt8.  Works on non-animated files as well, in which case the sequence has a single frame.
	// The returned frames always have 4 components (RGBA), and are always the full canvas size, with inter-frame disposal and blending already applied by libwebp.
	static Reference<Map2D> decodeImageSequence(const std::string& path, glare::Allocator* mem_allocator = NULL);
	static Reference<Map2D> decodeImageSequenceFromBuffer(const void* data, size_t size, glare::Allocator* mem_allocator = NULL);

	static Reference<Map2D> decodeImageOrSequence(const std::string& path, glare::Allocator* mem_allocator = NULL);
	static Reference<Map2D> decodeImageOrSequenceFromBuffer(const void* data, size_t size, glare::Allocator* mem_allocator = NULL);

	static void test();
};
