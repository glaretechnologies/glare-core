/*=====================================================================
bmpdecoder.cpp
--------------
Copyright Glare Technologies Limited 2018 -
File created by ClassTemplate on Mon May 02 22:00:30 2005
=====================================================================*/
#include "bmpdecoder.h"


#include "imformatdecoder.h"
#include "../graphics/ImageMap.h"
#include "../utils/StringUtils.h"
#include "../utils/MemMappedFile.h"
#include <assert.h>
#include <string.h>	
#include <cstdlib> // For std::abs()


#define BITMAP_ID        0x4D42       // this is the universal id for a bitmap


#pragma pack (push, 1)

typedef struct {
   unsigned short int type;                 /* Magic identifier            */
   unsigned int size;                       /* File size in bytes          */
   unsigned short int reserved1, reserved2;
   unsigned int offset;                     /* Offset to image data, bytes */
} BITMAP_HEADER;



typedef struct {
   unsigned int size;               /* Header size in bytes      */
   int width,height;                /* Width and height of image */
   unsigned short int planes;       /* Number of colour planes   */
   unsigned short int bits;         /* Bits per pixel            */
   unsigned int compression;        /* Compression type          */
   unsigned int imagesize;          /* Image size in bytes       */
   int xresolution,yresolution;     /* Pixels per meter          */
   unsigned int ncolours;           /* Number of colours         */
   unsigned int importantcolours;   /* Important colours         */
} BITMAP_INFOHEADER;

#pragma pack (pop)


Reference<Map2D> BMPDecoder::decode(const std::string& path)
{
	try
	{
		MemMappedFile file(path);
		const uint8* const filedata = (const uint8*)file.fileData();
		const size_t filesize = file.fileSize();
		
		size_t srcindex = 0;

		//-----------------------------------------------------------------
		//read bitmap header
		//-----------------------------------------------------------------
		if(filesize < sizeof(BITMAP_HEADER))
			throw ImFormatExcep("numimagebytes < sizeof(BITMAP_HEADER)");

		BITMAP_HEADER header;
		static_assert(sizeof(BITMAP_HEADER) == 14, "sizeof(BITMAP_HEADER) == 14");

		memcpy(&header, filedata + srcindex, sizeof(BITMAP_HEADER));
		srcindex += sizeof(BITMAP_HEADER);

	
		//-----------------------------------------------------------------
		//read bitmap info-header
		//-----------------------------------------------------------------
		static_assert(sizeof(BITMAP_INFOHEADER) == 40, "sizeof(BITMAP_INFOHEADER) == 40");
		BITMAP_INFOHEADER infoheader;

		if(filesize < sizeof(BITMAP_HEADER) + sizeof(BITMAP_INFOHEADER))
			throw ImFormatExcep("EOF before BITMAP_INFOHEADER");

		memcpy(&infoheader, filedata + srcindex, sizeof(BITMAP_INFOHEADER));
		srcindex += sizeof(BITMAP_INFOHEADER);

		// Move to image data offset
		srcindex = header.offset;
		if(srcindex < (sizeof(BITMAP_HEADER) + sizeof(BITMAP_INFOHEADER)) || srcindex >= filesize)
			throw ImFormatExcep("Invalid offset");


		if(!(infoheader.bits == 8 || infoheader.bits == 24))
			throw ImFormatExcep("unsupported bitdepth of " + ::toString(infoheader.bits));

		if(infoheader.compression != 0)
			throw ImFormatExcep("Only uncompressed BMPs supported.");
		
		const int signed_height = infoheader.height; // Height can be negative, this means data is stored top-to-bottom. (see http://en.wikipedia.org/wiki/BMP_file_format)
		const int abs_height = std::abs(signed_height);

		const int MAX_DIMS = 10000;
		if(infoheader.width < 0 || infoheader.width > MAX_DIMS)
			throw ImFormatExcep("Invalid image width (" + toString(infoheader.width) + ")");
		if(abs_height < 0 || abs_height > MAX_DIMS) 
			throw ImFormatExcep("Invalid image height (" + toString(signed_height) + ")");

		const size_t bytespp = infoheader.bits / 8;
		const size_t width = infoheader.width;

		Reference<ImageMap<uint8_t, UInt8ComponentValueTraits> > texture = new ImageMap<uint8_t, UInt8ComponentValueTraits>((unsigned int)infoheader.width, abs_height, (unsigned int)bytespp);

		const size_t rowpaddingbytes = (4 - ((width * bytespp) % 4)) % 4;

		if(signed_height < 0)
		{
			// This is a top-to-bottom bitmap:
			for(int y=0; y<abs_height; ++y)
			{
				// Make sure we're not going to read past end of encoded_img
				if(srcindex + width * bytespp > filesize)
					throw ImFormatExcep("Error while parsing BMP");

				for(int x=0; x<infoheader.width; ++x)
				{
					if(bytespp == 1)
					{
						assert(srcindex < filesize);
						texture->getPixel(x, y)[0] = filedata[srcindex++];
					}
					else
					{
						assert(srcindex + 2 < filesize);

						uint8 r, g, b;
						b = filedata[srcindex++];
						g = filedata[srcindex++];
						r = filedata[srcindex++];

						texture->getPixel(x, y)[0] = r;
						texture->getPixel(x, y)[1] = g;
						texture->getPixel(x, y)[2] = b;
					}
				}
				srcindex += rowpaddingbytes;
			}
		}
		else
		{
			for(int y=abs_height-1; y>=0; --y)
			{
				// Make sure we're not going to read past end of encoded_img
				if(srcindex + width * bytespp > filesize)
					throw ImFormatExcep("Error while parsing BMP");

				for(int x=0; x<infoheader.width; ++x)
				{
					if(bytespp == 1)
					{
						assert(srcindex < filesize);
						texture->getPixel(x, y)[0] = filedata[srcindex++];
					}
					else
					{
						assert(srcindex + 2 < filesize);

						uint8 r, g, b;
						b = filedata[srcindex++];
						g = filedata[srcindex++];
						r = filedata[srcindex++];

						texture->getPixel(x, y)[0] = r;
						texture->getPixel(x, y)[1] = g;
						texture->getPixel(x, y)[2] = b;
					}
				}
				srcindex += rowpaddingbytes;
			}
		}

		return texture;
	}
	catch(Indigo::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"


void BMPDecoder::test()
{
	conPrint("BMPDecoder::test()");

	// Load a RGB BMP
	try
	{
		Reference<Map2D> im = BMPDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref.bmp");
		
		testAssert(im->getMapWidth() == 1080);
		testAssert(im->getMapHeight() == 768);
		testAssert(im->getBytesPerPixel() == 3);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Load a greyscale BMP
	try
	{
		Reference<Map2D> im = BMPDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref_greyscale.bmp");

		testAssert(im->getMapWidth() == 1080);
		testAssert(im->getMapHeight() == 768);
		testAssert(im->getBytesPerPixel() == 1);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Test top-to-bottom BMP
	try
	{
		Reference<Map2D> im = BMPDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/BMPs/top_to_bottom.BMP");
		testAssert(im->getMapWidth() == 372);
		testAssert(im->getMapHeight() == 379);
		testAssert(im->getBytesPerPixel() == 3);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}


	// Test Unicode path
	try
	{
		// Test Unicode path
		const std::string euro = "\xE2\x82\xAC";
		Reference<Map2D> im2 = BMPDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testscenes/" + euro + ".bmp");
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Test that failure to load an image is handled gracefully.

	// Try with an invalid path
	try
	{
		BMPDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/BMPs/NO_SUCH_FILE.bmp");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{
	}

	// Try with a JPG file
	try
	{
		BMPDecoder::decode(TestUtils::getIndigoTestReposDir() + "/testfiles/checker.jpg");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{
	}

	conPrint("BMPDecoder::test() done.");
}


#endif // BUILD_TESTS
