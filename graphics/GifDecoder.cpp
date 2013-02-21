/*=====================================================================
GifDecoder.cpp
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-02-21 14:06:25 +0000
=====================================================================*/
#include "GifDecoder.h"


#include "imformatdecoder.h"
#include "ImageMap.h"
#include "../utils/stringutils.h"
#include "../utils/fileutils.h"
#include "../indigo/globals.h"
#include <stdio.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
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
		const int w = gif_file->SWidth;
		const int h = gif_file->SHeight;

		if(w <= 0 || w > 1000000)
			throw ImFormatExcep("Invalid image width.");
		if(h <= 0 || h > 1000000)
			throw ImFormatExcep("Invalid image height.");


		Reference<ImageMap<uint8_t, UInt8ComponentValueTraits> > image_map = new ImageMap<uint8_t, UInt8ComponentValueTraits>(w, h, 3);
		image_map->setGamma((float)2.2f);

		// Decode colours from Palette
		for(int y=0; y<h; ++y)
		for(int x=0; x<w; ++x)
		{
			const int i = y*w + x;
			const uint8 c = gif_file->SavedImages[0].RasterBits[i];
			image_map->getPixel(x, y)[0] = gif_file->SColorMap->Colors[c].Red;
			image_map->getPixel(x, y)[1] = gif_file->SColorMap->Colors[c].Green;
			image_map->getPixel(x, y)[2] = gif_file->SColorMap->Colors[c].Blue;
		}

		DGifCloseFile(gif_file);

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
		Reference<Map2D> im = GIFDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/gifs/fire.gif");
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
