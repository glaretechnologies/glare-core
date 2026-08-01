/*=====================================================================
WebPDecoder.cpp
---------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#include "WebPDecoder.h"


#include "imformatdecoder.h"
#include "ImageMap.h"
#include "ImageMapSequence.h"
#include "../utils/StringUtils.h"
#include "../utils/MemMappedFile.h"
#include "../utils/Exception.h"
#include <webp/decode.h>
#include <webp/demux.h>
#include <cstring> // For std::memcpy


// Sanity-check limits on the sizes of images we will allocate memory for.
static const int max_image_width  = 100000;
static const int max_image_height = 100000;
static const size_t max_image_num_pixels = 1 << 27;
static const size_t max_num_frames = 100000;
static const size_t max_total_image_size = 1 << 28; // 268,435,456 B


static const std::string errorString(VP8StatusCode status)
{
	switch(status)
	{
	case VP8_STATUS_OK:                   return "OK";
	case VP8_STATUS_OUT_OF_MEMORY:        return "out of memory";
	case VP8_STATUS_INVALID_PARAM:        return "invalid parameter";
	case VP8_STATUS_BITSTREAM_ERROR:      return "bitstream error";
	case VP8_STATUS_UNSUPPORTED_FEATURE:  return "unsupported feature";
	case VP8_STATUS_SUSPENDED:            return "suspended";
	case VP8_STATUS_USER_ABORT:           return "user abort";
	case VP8_STATUS_NOT_ENOUGH_DATA:      return "not enough data";
	default:                              return "[Unknown]";
	}
}


// Throws ImFormatExcep if the image dimensions are invalid or excessively large.
static void checkImageDims(int width, int height)
{
	if(width <= 0 || width > max_image_width)
		throw ImFormatExcep("Invalid image width: " + toString(width));
	if(height <= 0 || height > max_image_height)
		throw ImFormatExcep("Invalid image height: " + toString(height));
	if((size_t)width * (size_t)height > max_image_num_pixels)
		throw ImFormatExcep("Invalid width and height (too many pixels): " + toString(width) + ", " + toString(height));
}


Reference<Map2D> WebPDecoder::decode(const std::string& path, glare::Allocator* mem_allocator)
{
	try
	{
		MemMappedFile file(path);
		return decodeFromBuffer(file.fileData(), file.fileSize(), /*return_animated_webp_as_sequence=*/false, mem_allocator);
	}
	catch(glare::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


Reference<Map2D> WebPDecoder::decodeFromBuffer(const void* data, size_t size, bool return_animated_webp_as_sequence, glare::Allocator* mem_allocator)
{
	try
	{
		WebPBitstreamFeatures features;
		const VP8StatusCode status = WebPGetFeatures((const uint8_t*)data, size, &features);
		if(status != VP8_STATUS_OK)
			throw ImFormatExcep("Failed to read WebP features: " + errorString(status));

		// The still-image decoding functions can't handle animations, so decode the first frame of the animation instead.
		if(features.has_animation)
		{
			Reference<Map2D> sequence = decodeImageSequenceFromBuffer(data, size, mem_allocator);
			return return_animated_webp_as_sequence ? sequence : sequence.downcastToPtr<ImageMapSequenceUInt8>()->images[0];
		}

		checkImageDims(features.width, features.height);

		const size_t w = (size_t)features.width;
		const size_t h = (size_t)features.height;
		const size_t N = features.has_alpha ? 4 : 3;

		ImageMapUInt8Ref image_map = new ImageMapUInt8(w, h, N, mem_allocator);
		image_map->setGamma(2.2f);

		const size_t buffer_size = w * h * N;
		const int stride = (int)(w * N); // Distance between scanlines in bytes.  We know this fits in an int since w * h <= max_image_num_pixels.

		const uint8_t* res = features.has_alpha ?
			WebPDecodeRGBAInto((const uint8_t*)data, size, image_map->getData(), buffer_size, stride) :
			WebPDecodeRGBInto ((const uint8_t*)data, size, image_map->getData(), buffer_size, stride);

		if(res == NULL)
			throw ImFormatExcep("Failed to decode WebP image.");

		return image_map;
	}
	catch(std::bad_alloc&)
	{
		throw ImFormatExcep("Failed to decode WebP image (memory allocation failure).");
	}
	catch(glare::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


Reference<Map2D> WebPDecoder::decodeImageOrSequence(const std::string& path, glare::Allocator* mem_allocator)
{
	try
	{
		MemMappedFile file(path);
		return decodeImageOrSequenceFromBuffer(file.fileData(), file.fileSize(), mem_allocator);
	}
	catch(glare::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


Reference<Map2D> WebPDecoder::decodeImageOrSequenceFromBuffer(const void* data, size_t size, glare::Allocator* mem_allocator)
{
	return decodeFromBuffer(data, size, /*return_animated_webp_as_sequence=*/true, mem_allocator);
}


Reference<Map2D> WebPDecoder::decodeImageSequence(const std::string& path, glare::Allocator* mem_allocator)
{
	try
	{
		MemMappedFile file(path);
		return decodeImageSequenceFromBuffer(file.fileData(), file.fileSize(), mem_allocator);
	}
	catch(glare::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


Reference<Map2D> WebPDecoder::decodeImageSequenceFromBuffer(const void* data, size_t size, glare::Allocator* mem_allocator)
{
	WebPData webp_data;
	WebPDataInit(&webp_data);
	webp_data.bytes = (const uint8_t*)data;
	webp_data.size = size;

	WebPAnimDecoderOptions dec_options;
	if(!WebPAnimDecoderOptionsInit(&dec_options))
		throw ImFormatExcep("WebPAnimDecoderOptionsInit failed (libwebp ABI version mismatch).");
	dec_options.color_mode = MODE_RGBA;
	dec_options.use_threads = 0;

	// Note that WebPAnimDecoderNew() doesn't copy webp_data, so the source buffer must remain valid until WebPAnimDecoderDelete() is called.
	WebPAnimDecoder* dec = WebPAnimDecoderNew(&webp_data, &dec_options);
	if(dec == NULL)
		throw ImFormatExcep("Failed to parse WebP file.");

	try
	{
		WebPAnimInfo anim_info;
		if(!WebPAnimDecoderGetInfo(dec, &anim_info))
			throw ImFormatExcep("WebPAnimDecoderGetInfo failed.");

		if(anim_info.canvas_width > (uint32_t)max_image_width || anim_info.canvas_height > (uint32_t)max_image_height)
			throw ImFormatExcep("Invalid canvas dimensions: " + toString(anim_info.canvas_width) + ", " + toString(anim_info.canvas_height));

		checkImageDims((int)anim_info.canvas_width, (int)anim_info.canvas_height);

		if(anim_info.frame_count < 1)
			throw ImFormatExcep("Invalid frame count (< 1).");
		if(anim_info.frame_count > max_num_frames)
			throw ImFormatExcep("Invalid frame count (> " + toString(max_num_frames) + ").");

		const size_t w = (size_t)anim_info.canvas_width;
		const size_t h = (size_t)anim_info.canvas_height;
		const size_t frame_size_B = w * h * 4;

		if(frame_size_B * (size_t)anim_info.frame_count > max_total_image_size)
			throw ImFormatExcep("Animation is too large (" + toString(anim_info.frame_count) + " frames of " + toString(w) + " x " + toString(h) + ").");

		Reference<ImageMapSequenceUInt8> sequence = new ImageMapSequenceUInt8();
		sequence->images           .reserve(anim_info.frame_count);
		sequence->frame_durations  .reserve(anim_info.frame_count);
		sequence->frame_start_times.reserve(anim_info.frame_count);

		// WebPAnimDecoderGetNext() returns the timestamp of the *end* of each frame, in milliseconds.
		int prev_end_time_ms = 0;
		while(WebPAnimDecoderHasMoreFrames(dec))
		{
			uint8_t* frame_data;
			int end_time_ms;
			if(!WebPAnimDecoderGetNext(dec, &frame_data, &end_time_ms))
				throw ImFormatExcep("Failed to decode WebP frame " + toString(sequence->images.size()) + ".");

			// Guard against a decoder returning more frames than it said it would, which would result in unbounded memory use.
			if(sequence->images.size() >= (size_t)anim_info.frame_count)
				throw ImFormatExcep("WebP file has more frames than the frame count in the header.");

			ImageMapUInt8Ref image_map = new ImageMapUInt8(w, h, 4, mem_allocator);
			image_map->setGamma(2.2f);
			std::memcpy(image_map->getData(), frame_data, frame_size_B); // WebPAnimDecoderGetNext() returns a full canvas-sized RGBA frame.

			int duration_ms = end_time_ms - prev_end_time_ms;
			if(duration_ms <= 0)
				duration_ms = 100; // Some files have a frame duration of 0, which seems invalid.  Use 100ms, matching what browsers do for such files.

			sequence->images           .push_back(image_map);
			sequence->frame_durations  .push_back(duration_ms * 1.0e-3);
			sequence->frame_start_times.push_back(prev_end_time_ms * 1.0e-3);

			prev_end_time_ms += duration_ms;
		}

		if(sequence->images.empty())
			throw ImFormatExcep("WebP file had no frames.");

		WebPAnimDecoderDelete(dec);

		return sequence;
	}
	catch(std::bad_alloc&)
	{
		WebPAnimDecoderDelete(dec);
		throw ImFormatExcep("Failed to decode WebP file (memory allocation failure).");
	}
	catch(glare::Exception& e)
	{
		WebPAnimDecoderDelete(dec);
		throw ImFormatExcep(e.what());
	}
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Platform.h"


#if 0
// Command line:
// C:\fuzz_corpus\webp N:\indigo\trunk\testfiles\webps

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		WebPDecoder::decodeFromBuffer(data, size);
	}
	catch(glare::Exception&)
	{
	}

	try
	{
		WebPDecoder::decodeImageSequenceFromBuffer(data, size);
	}
	catch(glare::Exception&)
	{
	}

	return 0;  // Non-zero return values are reserved for future use.
}
#endif


void WebPDecoder::test()
{
	conPrint("WebPDecoder::test()");

	// Test a lossy WebP file without an alpha channel.
	try
	{
		Reference<Map2D> im = WebPDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/webps/test_lossy.webp");
		testAssert(im->getMapWidth() == 128);
		testAssert(im->getMapHeight() == 128);
		testAssert(im->numChannels() == 3);
		testAssert(!im->hasAlphaChannel());
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Test a lossy WebP file with an alpha channel.  (This file has a separate ALPH chunk alongside the VP8 chunk)
	try
	{
		Reference<Map2D> im = WebPDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/webps/sample-alpha-400x300.webp");
		testAssert(im->getMapWidth() == 400);
		testAssert(im->getMapHeight() == 300);
		testAssert(im->numChannels() == 4);
		testAssert(im->hasAlphaChannel());

		// The image should actually have some transparency in it, e.g. the alpha channel should have been decoded.
		const ImageMapUInt8* image_map = im.downcastToPtr<ImageMapUInt8>();
		bool has_transparent_pixel = false;
		for(size_t i=0; i<400*300; ++i)
			if(image_map->getPixel(i)[3] != 255)
				has_transparent_pixel = true;
		testAssert(has_transparent_pixel);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Test a lossless (VP8L) WebP file without an alpha channel.
	try
	{
		Reference<Map2D> im = WebPDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/webps/sample-lossless-400x300.webp");
		testAssert(im->getMapWidth() == 400);
		testAssert(im->getMapHeight() == 300);
		testAssert(im->numChannels() == 3);
		testAssert(!im->hasAlphaChannel());
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Test an animated WebP file.  This file has 8 frames, each with a duration of 120ms.
	// The individual frames are sub-rectangles of the canvas, so this also tests that libwebp is compositing them onto the full canvas for us.
	try
	{
		Reference<Map2D> map = WebPDecoder::decodeImageSequence(TestUtils::getTestReposDir() + "/testfiles/webps/sample-animated-200x200.webp");
		ImageMapSequenceUInt8* sequence = map.downcastToPtr<ImageMapSequenceUInt8>();

		testAssert(sequence->images.size() == 8);
		testAssert(sequence->frame_durations.size() == 8);
		testAssert(sequence->frame_start_times.size() == 8);

		for(size_t i=0; i<sequence->images.size(); ++i)
		{
			testAssert(sequence->images[i]->getWidth()  == 200);
			testAssert(sequence->images[i]->getHeight() == 200);
			testAssert(sequence->images[i]->numChannels() == 4);

			testEpsEqual(sequence->frame_durations  [i], 0.12);
			testEpsEqual(sequence->frame_start_times[i], 0.12 * i);
		}

		// Not all frames should be identical, e.g. we should be getting the individual frames and not the same frame repeatedly.
		bool all_frames_identical = true;
		for(size_t i=1; i<sequence->images.size(); ++i)
			if(std::memcmp(sequence->images[i]->getData(), sequence->images[0]->getData(), 200*200*4) != 0)
				all_frames_identical = false;
		testAssert(!all_frames_identical);

		// decode() on an animated file should return the first frame of the animation.
		Reference<Map2D> im = WebPDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/webps/sample-animated-200x200.webp");
		testAssert(im->getMapWidth() == 200);
		testAssert(im->getMapHeight() == 200);
		testAssert(im->numChannels() == 4);

		const ImageMapUInt8* first_frame = im.downcastToPtr<ImageMapUInt8>();
		testAssert(std::memcmp(first_frame->getData(), sequence->images[0]->getData(), 200*200*4) == 0);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// decodeImageSequence() on a non-animated file should return a sequence with a single frame.
	// Note that the anim decoder always returns RGBA frames, even for a file without an alpha channel.
	try
	{
		Reference<Map2D> map = WebPDecoder::decodeImageSequence(TestUtils::getTestReposDir() + "/testfiles/webps/test_lossy.webp");
		ImageMapSequenceUInt8* sequence = map.downcastToPtr<ImageMapSequenceUInt8>();

		testAssert(sequence->images.size() == 1);
		testAssert(sequence->images.size() == sequence->frame_durations.size());
		testAssert(sequence->images.size() == sequence->frame_start_times.size());

		testAssert(sequence->images[0]->getWidth() == 128);
		testAssert(sequence->images[0]->getHeight() == 128);
		testAssert(sequence->images[0]->numChannels() == 4);

		testAssert(sequence->frame_durations[0] > 0);
		testEpsEqual(sequence->frame_start_times[0], 0.0);

		// The RGB values should match those from the still-image decode path.
		Reference<Map2D> im = WebPDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/webps/test_lossy.webp");
		const ImageMapUInt8* still_im = im.downcastToPtr<ImageMapUInt8>();
		for(size_t i=0; i<128*128; ++i)
			for(size_t c=0; c<3; ++c)
				testAssert(sequence->images[0]->getPixel(i)[c] == still_im->getPixel(i)[c]);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	//=================== Test that failure to load an image is handled gracefully ===================

	// Try with an invalid path
	try
	{
		WebPDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/webps/NO_SUCH_FILE.webp");
		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try with a JPG file
	try
	{
		WebPDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/checker.jpg");
		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	try
	{
		WebPDecoder::decodeImageSequence(TestUtils::getTestReposDir() + "/testfiles/checker.jpg");
		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try with an empty file
	try
	{
		WebPDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/empty_file");
		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try with a zero-size buffer
	try
	{
		WebPDecoder::decodeFromBuffer(NULL, 0, /*return_animated_webp_as_sequence=*/false, /*mem allocator=*/nullptr);
		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try with truncated files: decoding should fail, but not crash or hang.
	try
	{
		const std::string paths[] = {
			TestUtils::getTestReposDir() + "/testfiles/webps/test_lossy.webp",
			TestUtils::getTestReposDir() + "/testfiles/webps/sample-alpha-400x300.webp",
			TestUtils::getTestReposDir() + "/testfiles/webps/sample-lossless-400x300.webp",
			TestUtils::getTestReposDir() + "/testfiles/webps/sample-animated-200x200.webp"
		};

		for(size_t path_i=0; path_i<staticArrayNumElems(paths); ++path_i)
		{
			MemMappedFile file(paths[path_i]);
			for(size_t new_size = 0; new_size < file.fileSize(); ++new_size)
			{
				try
				{
					WebPDecoder::decodeFromBuffer(file.fileData(), new_size, /*return_animated_webp_as_sequence=*/false, /*mem allocator=*/nullptr);
				}
				catch(ImFormatExcep&)
				{}

				try
				{
					WebPDecoder::decodeImageSequenceFromBuffer(file.fileData(), new_size);
				}
				catch(ImFormatExcep&)
				{}
			}
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	conPrint("WebPDecoder::test() done.");
}


#endif // BUILD_TESTS
