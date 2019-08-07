/*=====================================================================
TIFFDecoder.cpp
---------------
Copyright Glare Technologies Limited 2013 -
File created by ClassTemplate on Fri May 02 16:51:32 2008
=====================================================================*/
#include "TIFFDecoder.h"


#include "ImageMap.h"
#include "imformatdecoder.h"
#include "../utils/StringUtils.h"
#include <tiffio.h>


// Explicit template instantiation
template void TIFFDecoder::write(const ImageMap<uint8,  UInt8ComponentValueTraits> & bitmap, const std::string& path);
template void TIFFDecoder::write(const ImageMap<uint16, UInt16ComponentValueTraits>& bitmap, const std::string& path);


Reference<Map2D> TIFFDecoder::decode(const std::string& path)
{
	// Stop LibTiff writing to stderr by setting NULL handlers.
	// NOTE: restore old handlers at end of method?
	// NOTE: threadsafe?
	TIFFSetWarningHandler(NULL);
	TIFFSetErrorHandler(NULL);

#if defined(_WIN32)
	TIFF* tif = TIFFOpenW(StringUtils::UTF8ToWString(path).c_str(), "r");
#else
	TIFF* tif = TIFFOpen(path.c_str(), "r");
#endif
    if(tif)
	{
		uint32 w, h;

		uint16 bits_per_sample, samples_per_pixel;

		TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &w);
		TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &h);
		TIFFGetField(tif, TIFFTAG_BITSPERSAMPLE, &bits_per_sample);
		TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &samples_per_pixel);
	
		if(samples_per_pixel < 1 || samples_per_pixel > 4)
			throw ImFormatExcep("Tiff file has unsupported number of channels: " + toString(samples_per_pixel));
		if(bits_per_sample != 8 && bits_per_sample != 16)
			throw ImFormatExcep("Tiff file has unsupported number of bits per sample: " + toString(bits_per_sample));

		uint32 rows_per_strip = 0;
		TIFFGetField(tif, TIFFTAG_ROWSPERSTRIP, &rows_per_strip);
	
		const tstrip_t num_strips = TIFFNumberOfStrips(tif);

		Reference<Map2D> result_tex;

		if(bits_per_sample == 8)
		{
			std::vector<unsigned char> buf(TIFFStripSize(tif));

			Reference<ImageMap<uint8_t, UInt8ComponentValueTraits> > texture( new ImageMap<uint8_t, UInt8ComponentValueTraits>(
				w, h, samples_per_pixel
			));

			for(tstrip_t strip = 0; strip < num_strips; strip++)
			{
				const tsize_t num_bytes = TIFFReadEncodedStrip(tif, strip, &buf[0], (tsize_t)buf.size());
				if(num_bytes == -1)
					throw ImFormatExcep("TIFFReadEncodedStrip failed.");

				int i=0;
				for(uint32 row=0; row<rows_per_strip; ++row)
				{
					const uint32 y = strip * rows_per_strip + row;
					if(y < h)
					{
						for(uint32 x=0; x<w; ++x)
							for(uint32 c=0; c<samples_per_pixel; ++c)
								texture->getPixel(x, y)[c] = buf[i++]; // NOTE: Is there row padding?
					}
				}
			}

			result_tex = texture;
		}
		else if(bits_per_sample == 16)
		{
			std::vector<uint16_t> buf(TIFFStripSize(tif) / sizeof(uint16_t));

			Reference<ImageMap<uint16_t, UInt16ComponentValueTraits> > texture( new ImageMap<uint16_t, UInt16ComponentValueTraits>(
				w, h, samples_per_pixel
			));

			for(tstrip_t strip = 0; strip < num_strips; strip++)
			{
				const tsize_t num_bytes = TIFFReadEncodedStrip(tif, strip, &buf[0], (tsize_t)buf.size() * sizeof(uint16_t));
				if(num_bytes == -1)
					throw ImFormatExcep("TIFFReadEncodedStrip failed.");

				int i=0;
				for(uint32 row=0; row<rows_per_strip; ++row)
				{
					const uint32 y = strip * rows_per_strip + row;
					if(y < h)
					{
						for(uint32 x=0; x<w; ++x)
							for(uint32 c=0; c<samples_per_pixel; ++c)
								texture->getPixel(x, y)[c] = buf[i++]; // NOTE: Is there row padding?
					}
				}
			}

			result_tex = texture;
		}
		else
		{
			assert(!"Invalid bits_per_sample");
		}

		TIFFClose(tif);
		return result_tex;
	}
	else // else if !tif
	{
		throw ImFormatExcep("Failed to open file '" + path + "' for reading.");
	}
}


template <class T, class Traits>
void TIFFDecoder::write(const ImageMap<T, Traits>& bitmap, const std::string& path)
{
	// Stop LibTiff writing to stderr by setting NULL handlers.
	// NOTE: restore old handlers at end of method?
	// NOTE: threadsafe?
	TIFFSetWarningHandler(NULL);
	TIFFSetErrorHandler(NULL);

#if defined(_WIN32)
	TIFF* tif = TIFFOpenW(StringUtils::UTF8ToWString(path).c_str(), "w");
#else
	TIFF* tif = TIFFOpen(path.c_str(), "w");
#endif
    if(tif)
	{
		const uint32 w = (uint32)bitmap.getWidth();
		const uint32 h = (uint32)bitmap.getHeight();
		const uint16 bits_per_sample = (uint16)(sizeof(T) * 8);
		const uint16 samples_per_pixel = (uint16)bitmap.getN();
		TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, w);
		TIFFSetField(tif, TIFFTAG_IMAGELENGTH, h);
		TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
		TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, samples_per_pixel);

		TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT); // Set the origin of the image.
		TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
		TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);

		TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_LZW); // NOTE: is this a suitable compression method to use?  Accept compression method as option?
	
		const tsize_t linebytes = sizeof(T) * samples_per_pixel * w;
		std::vector<unsigned char> scanline(linebytes);

		// We set the strip size of the file to be size of one row of pixels
		TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, TIFFDefaultStripSize(tif, linebytes));
	
		for(uint32 y=0; y<h; ++y)
		{
			std::memcpy(&scanline[0], bitmap.getPixel(0, y), linebytes);
			const tsize_t num_bytes = TIFFWriteScanline(tif, &scanline[0], y);
			if(num_bytes == -1)
				throw ImFormatExcep("TIFFReadEncodedStrip failed.");
		}

		TIFFClose(tif);
	}
	else // else if !tif
	{
		throw ImFormatExcep("Failed to open file '" + path + "' for writing.");
	}
}


bool TIFFDecoder::isTiffCompressed(const std::string& path)
{
#if defined(_WIN32)
	TIFF* tif = TIFFOpenW(StringUtils::UTF8ToWString(path).c_str(), "r");
#else
	TIFF* tif = TIFFOpen(path.c_str(), "r");
#endif

	if(tif)
	{
		uint16 compression;
		TIFFGetField(tif, TIFFTAG_COMPRESSION, &compression);
		
		TIFFClose(tif);

		return compression != COMPRESSION_NONE;
	}
	else // else if !tif
	{
		throw ImFormatExcep("Failed to open file '" + path + "' for reading.");
	}
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/ConPrint.h"


void TIFFDecoder::test()
{
	conPrint("TIFFDecoder::test()");

	const std::string tempdir = PlatformUtils::getTempDirPath();

	// Try writing a 8-bit ImageMap object to a TIFF file
	try
	{
		const int W = 50; const int H = 40;
		ImageMapUInt8 imagemap(W, H, 3);
		for(int y=0; y<H; ++y)
		for(int x=0; x<W; ++x)
		{
			imagemap.getPixel(x, y)[0] = (uint8)(255 * (float)x/W);
			imagemap.getPixel(x, y)[1] = (uint8)(255 * (float)x/W);
			imagemap.getPixel(x, y)[2] = (uint8)(255 * (float)x/W);
		}

		const std::string path = tempdir + "/indigo_tiff_write_test.tiff";
		write(imagemap, path);

		// Now read it to make sure it's a valid TIFF file.
		Reference<Map2D> im = decode(path);
		testAssert(im->getMapWidth() == W);
		testAssert(im->getMapHeight() == H);
		testAssert(im->getBytesPerPixel() == 3);
	}
	catch(PlatformUtils::PlatformUtilsExcep& e)
	{
		failTest(e.what());
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Try writing a 16-bit ImageMap object to a TIFF file
	try
	{
		const int W = 50; const int H = 40;
		ImageMap<uint16, UInt16ComponentValueTraits> imagemap(W, H, 3);
		for(int y=0; y<H; ++y)
		for(int x=0; x<W; ++x)
		{
			imagemap.getPixel(x, y)[0] = (uint16)(65535 * (float)x/W);
			imagemap.getPixel(x, y)[1] = (uint16)(65535 * (float)x/W);
			imagemap.getPixel(x, y)[2] = (uint16)(65535 * (float)x/W);
		}

		const std::string path = tempdir + "/indigo_tiff_write_test_16bit.tiff";
		write(imagemap, path);

		// Now read it to make sure it's a valid TIFF file.
		Reference<Map2D> im = decode(path);
		testAssert(im->getMapWidth() == W);
		testAssert(im->getMapHeight() == H);
		testAssert(im->getBytesPerPixel() == 3 * 2);
	}
	catch(PlatformUtils::PlatformUtilsExcep& e)
	{
		failTest(e.what());
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}




	try
	{
		Reference<Map2D> im = TIFFDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref_lzw.tiff");
		testAssert(im->getMapWidth() == 1080);
		testAssert(im->getMapHeight() == 768);
		testAssert(im->getBytesPerPixel() == 3);

		testAssert(dynamic_cast<ImageMapUInt8*>(im.getPointer()) != NULL);
		const std::string path = tempdir + "/temp.tiff";
		write(*dynamic_cast<ImageMapUInt8*>(im.getPointer()), path);

		// Now read it to make sure it's a valid TIFF file.
		Reference<Map2D> im2 = decode(path);
		testAssert(im2->getMapWidth() == im->getMapWidth());
		testAssert(im2->getMapHeight() == im2->getMapHeight());
		testAssert(im2->getBytesPerPixel() == im2->getBytesPerPixel());
	}
	catch(PlatformUtils::PlatformUtilsExcep& e)
	{
		failTest(e.what());
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}


	try
	{
		Reference<Map2D> im = TIFFDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref_lzw.tiff");
		testAssert(im->getMapWidth() == 1080);
		testAssert(im->getMapHeight() == 768);
		testAssert(im->getBytesPerPixel() == 3);

		// This file Uses LZW compression according to Windows
		testAssert(TIFFDecoder::isTiffCompressed(TestUtils::getIndigoTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref_lzw.tiff"));

		testAssert(!TIFFDecoder::isTiffCompressed(TestUtils::getIndigoTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref_small_uncompressed.tiff"));

		// Test Unicode path
		const std::string euro = "\xE2\x82\xAC";
		Reference<Map2D> im2 = TIFFDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testscenes/" + euro + ".tiff");
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Test that failure to load an image is handled gracefully.

	// Try with an invalid path
	try
	{
		TIFFDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/NO_SUCH_FILE.tiff");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try with a JPG file
	try
	{
		TIFFDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/checker.jpg");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}
}


#endif // BUILD_TESTS
