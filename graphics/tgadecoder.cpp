/*=====================================================================
tgadecoder.cpp
--------------
Copyright Glare Technologies Limited 2018 -
File created by ClassTemplate on Mon Oct 04 23:30:26 2004
=====================================================================*/
#include "tgadecoder.h"


#include "imformatdecoder.h"
#include "../graphics/ImageMap.h"
#include "../graphics/bitmap.h"
#include "../utils/FileUtils.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../utils/MemMappedFile.h"
#include "../utils/Exception.h"
#include <cstring> // for std::memcpy()
#include <assert.h>
#include <memory.h>
#include <stdlib.h>


TGADecoder::TGADecoder()
{
}


TGADecoder::~TGADecoder()
{
}


// We need 1 byte alignment for this to work properly
#pragma pack(push, 1)

typedef struct
{
	uint8  identsize;          // size of ID field that follows 18 byte header (0 usually)
	uint8  colourmaptype;      // type of colour map 0=none, 1=has palette
	uint8  imagetype;          // type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed

	short colourmapstart;     // first colour map entry in palette
	short colourmaplength;    // number of colours in palette
	uint8  colourmapbits;      // number of bits per palette entry 15,16,24,32

	short xstart;             // image x origin
	short ystart;             // image y origin
	short width;              // image width in pixels
	short height;             // image height in pixels
	uint8  bits;               // image bits per pixel 8,16,24,32
	uint8  descriptor;         // image descriptor bits (vh flip bits)
} TGA_HEADER;

#pragma pack(pop)


Reference<Map2D> TGADecoder::decode(const std::string& path)
{
	//Timer timer;

	static_assert(sizeof(TGA_HEADER) == 18, "sizeof(TGA_HEADER) == 18");

	try
	{
		MemMappedFile file(path);

		// Read header
		if(file.fileSize() < sizeof(TGA_HEADER))
			throw ImFormatExcep("numimagebytes < sizeof(TGA_HEADER)");

		TGA_HEADER header;
		std::memcpy(&header, file.fileData(), sizeof(TGA_HEADER));

		const int width = (int)header.width;

		if(width < 0)
			throw ImFormatExcep("width < 0");

		const int height = (int)header.height;

		if(height < 0)
			throw ImFormatExcep("height < 0");

		if(!(header.bits == 8 || header.bits == 24 || header.bits == 32))
			throw ImFormatExcep("Invalid TGA bit-depth: " + toString(header.bits) + ". Only 8, 24 or 32 supported.");


		const int bytes_pp = header.bits / 8;

		bool uses_RLE = false;

		if(header.imagetype == 2 || header.imagetype == 3)
		{
			// RGB / greyscale

			const size_t imagesize = (size_t)width * (size_t)height * (size_t)bytes_pp;
			if(file.fileSize() < sizeof(TGA_HEADER) + imagesize)
				throw ImFormatExcep("not enough data supplied");
		}
		else if(header.imagetype == 10 || header.imagetype == 11)
		{
			// RGB / greyscale with RLE
			uses_RLE = true;
		}
		else
		{
			throw ImFormatExcep("Invalid TGA type " + toString(header.imagetype) + ": Only greyscale or RGB supported.");
		}

		Reference<ImageMap<uint8_t, UInt8ComponentValueTraits> > texture( new ImageMap<uint8_t, UInt8ComponentValueTraits>(
			width, height, bytes_pp
			));

		if(uses_RLE)
		{
			size_t cur = sizeof(TGA_HEADER);
			const size_t end = file.fileSize();
			const uint8* const data = (const uint8*)file.fileData();
			if(bytes_pp == 1)
			{
				for(int y=0; y<height; ++y)
				{
					const int dest_y = height - 1 - y;

					for(int x=0; x<width; )
					{
						if(cur >= end)
							throw ImFormatExcep("Decoding error.");

						if(data[cur] & 0x00000080u) // If high bit of byte is set
						{
							// Run-length packet:
							const int repeat_count = (int)data[cur] - 127;
							cur++;

							// Check the read of the next byte is in-bounds.
							if(cur >= end)
								throw ImFormatExcep("Decoding error.");

							// Read colour value
							uint8 col = data[cur];
							cur++;

							// Write it out repeat_count times

							// Check we are writing in-bounds.
							// NOTE: According to http://www.gamers.org/dEngine/quake3/TGA.txt the run should be able to continue to the next scan line.
							// However files seen in the wild, and saved from GIMP, so far don't see to do this.
							if(x + repeat_count > width)
								throw ImFormatExcep("Decoding error");

							for(int z=0; z<repeat_count; ++z)
							{
								texture->getPixel(x, dest_y)[0] = col;
								x++;
							}
						}
						else
						{
							// Raw packet:
							const int direct_count = (int)data[cur] + 1;
							cur++;

							// Check we are writing in-bounds
							if(x + direct_count > width)
								throw ImFormatExcep("Decoding error");

							// Check we are reading in-bounds
							if(cur + direct_count > end)
								throw ImFormatExcep("Decoding error");

							for(int z=0; z<direct_count; ++z)
							{
								texture->getPixel(x, dest_y)[0] = data[cur];
								x++;
								cur++;
							}
						}
					}
				}
			}	
			else if(bytes_pp == 3)
			{
				for(int y=0; y<height; ++y)
				{
					const int dest_y = height - 1 - y;

					for(int x=0; x<width; )
					{
						if(cur >= end)
							throw ImFormatExcep("Decoding error.");

						if(data[cur] & 0x00000080u) // If high bit is set
						{
							// Run-length packet:
							const int repeat_count = (int)data[cur] - 127;
							cur++;

							// Check the read of the next 3 bytes is in-bounds.
							if(cur + 2 >= end)
								throw ImFormatExcep("Decoding error.");

							// Read colour value - order in TGA file is BGR
							uint8 col[3];
							col[0] = data[cur + 2];
							col[1] = data[cur + 1];
							col[2] = data[cur + 0];
							cur += 3;

							// Write it out repeat_count times

							// Check we are writing in-bounds
							if(x + repeat_count > width)
								throw ImFormatExcep("Decoding error");

							for(int z=0; z<repeat_count; ++z)
							{
								texture->getPixel(x, dest_y)[0] = col[0];
								texture->getPixel(x, dest_y)[1] = col[1];
								texture->getPixel(x, dest_y)[2] = col[2];
								x++;
							}
						}
						else
						{
							// Raw packet:
							const int direct_count = (int)data[cur] + 1;
							cur++;

							// Check we are writing in-bounds
							if(x + direct_count > width)
								throw ImFormatExcep("Decoding error");

							// Check we are reading in-bounds
							if(cur + (size_t)direct_count * 3 > end)
								throw ImFormatExcep("Decoding error");

							for(int z=0; z<direct_count; ++z)
							{
								texture->getPixel(x, dest_y)[0] = data[cur + 2];
								texture->getPixel(x, dest_y)[1] = data[cur + 1];
								texture->getPixel(x, dest_y)[2] = data[cur + 0];
								x++;
								cur += 3;
							}
						}
					}
				}
			}
			else if(bytes_pp == 4)
			{
				for(int y=0; y<height; ++y)
				{
					const int dest_y = height - 1 - y;

					for(int x=0; x<width; )
					{
						if(cur >= end)
							throw ImFormatExcep("Decoding error.");

						if(data[cur] & 0x00000080u) // If high bit is set
						{
							// Run-length packet:
							const int repeat_count = (int)data[cur] - 127;
							cur++;

							// Check the read of the next 4 bytes is in-bounds.
							if(cur + 4 >= end)
								throw ImFormatExcep("Decoding error.");

							// Read colour value - order in TGA file is BGRA
							uint8 col[4]; // Colour in RGBA order
							col[0] = data[cur + 2];
							col[1] = data[cur + 1];
							col[2] = data[cur + 0];
							col[3] = data[cur + 3];
							cur += 4;

							// Write it out repeat_count times

							// Check we are writing in-bounds
							if(x + repeat_count > width)
								throw ImFormatExcep("Decoding error");

							for(int z=0; z<repeat_count; ++z)
							{
								texture->getPixel(x, dest_y)[0] = col[0];
								texture->getPixel(x, dest_y)[1] = col[1];
								texture->getPixel(x, dest_y)[2] = col[2];
								texture->getPixel(x, dest_y)[3] = col[3];
								x++;
							}
						}
						else
						{
							// Raw packet:
							const int direct_count = (int)data[cur] + 1;
							cur++;

							// Check we are writing in-bounds
							if(x + direct_count > width)
								throw ImFormatExcep("Decoding error");

							// Check we are reading in-bounds
							if(cur + (size_t)direct_count * 4 > end)
								throw ImFormatExcep("Decoding error");

							for(int z=0; z<direct_count; ++z)
							{
								texture->getPixel(x, dest_y)[0] = data[cur + 2];
								texture->getPixel(x, dest_y)[1] = data[cur + 1];
								texture->getPixel(x, dest_y)[2] = data[cur + 0];
								texture->getPixel(x, dest_y)[3] = data[cur + 3];
								x++;
								cur += 4;
							}
						}
					}
				}
			}
		}
		else
		{
			if(bytes_pp == 1)
			{
				const uint8* const srcpointer = (uint8*)file.fileData() + sizeof(TGA_HEADER);
				for(int y=0; y<height; ++y)
					for(int x=0; x<width; ++x)
					{
						const int srcoffset = (x + width*(height - 1 - y))*bytes_pp;

						texture->getPixel(x, y)[0] = srcpointer[srcoffset];
					}
			}	
			else if(bytes_pp == 3)
			{
				const uint8* const srcpointer = (uint8*)file.fileData() + sizeof(TGA_HEADER);
				for(int y=0; y<height; ++y)
				{
					for(int x=0; x<width; ++x)
					{
						const int srcoffset = (x + width*(height - 1 - y))*bytes_pp;

						texture->getPixel(x, y)[0] = srcpointer[srcoffset+2];
						texture->getPixel(x, y)[1] = srcpointer[srcoffset+1];
						texture->getPixel(x, y)[2] = srcpointer[srcoffset];
					}
				}
			}
			else if(bytes_pp == 4)
			{
				const uint8* const srcpointer = (uint8*)file.fileData() + sizeof(TGA_HEADER);
				for(int y=0; y<height; ++y)
				{
					for(int x=0; x<width; ++x)
					{
						const int srcoffset = (x + width*(height - 1 - y))*bytes_pp;

						texture->getPixel(x, y)[0] = srcpointer[srcoffset+2];
						texture->getPixel(x, y)[1] = srcpointer[srcoffset+1];
						texture->getPixel(x, y)[2] = srcpointer[srcoffset+0];
						texture->getPixel(x, y)[3] = srcpointer[srcoffset+3];
					}
				}
			}
			else
			{
				throw ImFormatExcep("invalid bytes per pixel.");
			}
		}

		//conPrint("Copied data.  Elapsed: " + timer.elapsedString());

		return texture;
	}
	catch(glare::Exception& e)
	{
		throw ImFormatExcep(e.what());
	}
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"


void TGADecoder::test()
{
	// Try loading a TGA with an alpha channel
	try
	{
		Reference<Map2D> im = TGADecoder::decode(TestUtils::getTestReposDir() + "/testfiles/TGAs/T_AcaciaNeedles_A.tga");
		testAssert(im->getMapWidth() == 128);
		testAssert(im->getMapHeight() == 256);
		testAssert(im->getBytesPerPixel() == 4);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Try a file with alpha but no RLE.
	try
	{
		Reference<Map2D> im = TGADecoder::decode(TestUtils::getTestReposDir() + "/testfiles/TGAs/T_AcaciaNeedles_A_no_RLE.tga");
		testAssert(im->getMapWidth() == 128);
		testAssert(im->getMapHeight() == 256);
		testAssert(im->getBytesPerPixel() == 4);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}


	// Try loading a RGB TGA without RLE
	try
	{
		Reference<Map2D> im = TGADecoder::decode(TestUtils::getTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref.tga");
		testAssert(im->getMapWidth() == 1080);
		testAssert(im->getMapHeight() == 768);
		testAssert(im->getBytesPerPixel() == 3);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Try loading a greyscale TGA without RLE
	try
	{
		Reference<Map2D> im = TGADecoder::decode(TestUtils::getTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref_greyscale.tga");
		testAssert(im->getMapWidth() == 1080);
		testAssert(im->getMapHeight() == 768);
		testAssert(im->getBytesPerPixel() == 1);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Try loading a RGB TGA with RLE
	try
	{
		Reference<Map2D> im = TGADecoder::decode(TestUtils::getTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref_with_RLE.tga");
		testAssert(im->getMapWidth() == 1080);
		testAssert(im->getMapHeight() == 768);
		testAssert(im->getBytesPerPixel() == 3);
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Try loading a greyscale TGA with RLE
	try
	{
		Reference<Map2D> im = TGADecoder::decode(TestUtils::getTestReposDir() + "/testscenes/ColorChecker_sRGB_from_Ref_greyscale_with_RLE.tga");
		testAssert(im->getMapWidth() == 1080);
		testAssert(im->getMapHeight() == 768);
		testAssert(im->getBytesPerPixel() == 1);
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
		Reference<Map2D> im2 = TGADecoder::decode(TestUtils::getTestReposDir() + "/testscenes/" + euro + ".tga");
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}

	// Test that failure to load an image is handled gracefully.

	// Try with an invalid path
	try
	{
		TGADecoder::decode(TestUtils::getTestReposDir() + "/testfiles/hdrs/NO_SUCH_FILE.gif");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}

	// Try with a JPG file
	try
	{
		TGADecoder::decode(TestUtils::getTestReposDir() + "/testfiles/checker.jpg");

		failTest("Shouldn't get here.");
	}
	catch(ImFormatExcep&)
	{}
}


#endif // BUILD_TESTS
