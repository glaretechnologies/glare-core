/*=====================================================================
GifDecoder.cpp
-------------------
Copyright Glare Technologies Limited 2019 -
Generated at 2013-02-21 14:06:25 +0000
=====================================================================*/
#include "GifDecoder.h"


#include "imformatdecoder.h"
#include "ImageMap.h"
#include "../utils/StringUtils.h"
#include "../utils/FileUtils.h"
#include "../indigo/globals.h"
#include <stdio.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <gif_lib.h>


GIFDecoder::GIFDecoder()
{
}


GIFDecoder::~GIFDecoder()
{
}


// throws ImFormatExcep on failure
Reference<Map2D> GIFDecoder::decode(const std::string& path)
{
	// Do this file descriptor malarkey so that we can handle Unicode paths.
#ifdef _WIN32
	const int file_descriptor = _wopen(StringUtils::UTF8ToPlatformUnicodeEncoding(path).c_str(), O_RDONLY);
#else
	const int file_descriptor = open(path.c_str(), O_RDONLY);
#endif
	if(file_descriptor == -1)
		throw ImFormatExcep("failed to open gif file '" + path + "'");


	// NOTE: if DGifOpenFileHandle fails to open the gif file, it closes the handle itself.
	int error_code = 0;
	GifFileType* gif_file = DGifOpenFileHandle(file_descriptor, &error_code);
	if(gif_file == NULL)
		throw ImFormatExcep("failed to open gif file '" + path + "': " + GifErrorString(error_code));

	try
	{
		int res = DGifSlurp(gif_file);
		if(res != GIF_OK)
			throw ImFormatExcep("failed to process gif file.");

		// Get image dimensions
		if(gif_file->SWidth <= 0 || gif_file->SWidth > 1000000)
			throw ImFormatExcep("Invalid image width.");
		if(gif_file->SHeight <= 0 || gif_file->SHeight > 1000000)
			throw ImFormatExcep("Invalid image height.");
		if(gif_file->ImageCount < 1)
			throw ImFormatExcep("Invalid ImageCount.");

		const size_t w = (size_t)gif_file->SWidth;
		const size_t h = (size_t)gif_file->SHeight;

		Reference<ImageMap<uint8, UInt8ComponentValueTraits> > image_map = new ImageMap<uint8, UInt8ComponentValueTraits>(w, h, 3);
		image_map->setGamma((float)2.2f);

		// Get the current palette:
		ColorMapObject* colour_map = NULL;
		if(gif_file->SColorMap) // Use global colormap, if it exists.
			colour_map = gif_file->SColorMap;
		else
			colour_map = gif_file->SavedImages[0].ImageDesc.ColorMap;

		if(!colour_map)
			throw ImFormatExcep("Failed to get ColorMapObject (palette).");

		const int BitsPerPixel = colour_map->BitsPerPixel;

		// Decode colours from Palette
		const SavedImage* const image_0 = &gif_file->SavedImages[0];
		const GifColorType* const colours = colour_map->Colors;
		uint8* const dest = image_map->getData();
		
		const size_t num_pixels = w * h;
		for(size_t i=0; i<num_pixels; ++i)
		{
			const uint8 c = image_0->RasterBits[i];
			if(c >= colour_map->ColorCount)
				throw ImFormatExcep("Colour index out of bounds.");
			dest[i*3 + 0] = colours[c].Red;
			dest[i*3 + 1] = colours[c].Green;
			dest[i*3 + 2] = colours[c].Blue;
		}

		error_code = 0;
		DGifCloseFile(gif_file, &error_code);

		return image_map;
	}
	catch(ImFormatExcep& e)
	{
		close(file_descriptor);
		throw e;
	}
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"


void GIFDecoder::test()
{
	try
	{
		Reference<Map2D> im;

		// This files uses a local palette.
		im = GIFDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/gifs/https_58_47_47media.giphy.com_47media_47X93e1eC2J2hjy_47giphy.gif");
		testAssert(im->getMapWidth() == 620);
		testAssert(im->getMapHeight() == 409);
		testAssert(im->getBytesPerPixel() == 3);

		im = GIFDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/gifs/fire.gif");
		testAssert(im->getMapWidth() == 30);
		testAssert(im->getMapHeight() == 60);
		testAssert(im->getBytesPerPixel() == 3);

		// Test Unicode path
		const std::string euro = "\xE2\x82\xAC";
		Reference<Map2D> im2 = GIFDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/gifs/" + euro + ".gif");
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Test that failure to load an image is handled gracefully.


	// This file has palette index out of bounds
	try
	{
		GIFDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/gifs/https_58_47_47media.giphy.com_47media_47ppTMXv7gqwCDm_47giphy.gif");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{
	}

	// Try with an invalid path
	try
	{
		GIFDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/gifs/NO_SUCH_FILE.gif");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try with a JPG file
	try
	{
		GIFDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/checker.jpg");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}
}


#endif // BUILD_TESTS
