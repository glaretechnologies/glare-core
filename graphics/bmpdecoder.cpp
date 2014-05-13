/*=====================================================================
bmpdecoder.cpp
--------------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Mon May 02 22:00:30 2005
=====================================================================*/
#include "bmpdecoder.h"


#include "bitmap.h"
#include "imformatdecoder.h"
#include "../graphics/ImageMap.h"
#include "../utils/FileUtils.h"
#include "../utils/StringUtils.h"
#include <assert.h>
#include <string.h>


BMPDecoder::BMPDecoder()
{
}


BMPDecoder::~BMPDecoder()
{
}


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


//these throw ImFormatExcep
Reference<Map2D> BMPDecoder::decode(const std::string& path)
{
	// Read file into memory
	std::vector<unsigned char> encoded_img;
	try
	{
		FileUtils::readEntireFile(path, encoded_img);
	}
	catch(FileUtils::FileUtilsExcep& e)
	{
		throw ImFormatExcep(e.what());
	}

	unsigned int srcindex = 0;
	//-----------------------------------------------------------------
	//read bitmap header
	//-----------------------------------------------------------------
	if(encoded_img.size() < sizeof(BITMAP_HEADER))
		throw ImFormatExcep("numimagebytes < sizeof(BITMAP_HEADER)");


	BITMAP_HEADER header;
	assert(sizeof(BITMAP_HEADER) == 14);

	memcpy(&header, &encoded_img[srcindex], sizeof(BITMAP_HEADER));
	srcindex += sizeof(BITMAP_HEADER);

	
	//-----------------------------------------------------------------
	//read bitmap info-header
	//-----------------------------------------------------------------
	assert(sizeof(BITMAP_INFOHEADER) == 40);
	BITMAP_INFOHEADER infoheader;

	if(encoded_img.size() < sizeof(BITMAP_HEADER) + sizeof(BITMAP_INFOHEADER))
		throw ImFormatExcep("EOF before BITMAP_INFOHEADER");

	memcpy(&infoheader, &encoded_img[srcindex], sizeof(BITMAP_INFOHEADER));
	srcindex += sizeof(BITMAP_INFOHEADER);

	// Move to image data offset
	srcindex = header.offset;
	if(srcindex < (sizeof(BITMAP_HEADER) + sizeof(BITMAP_INFOHEADER)) || srcindex >= encoded_img.size())
		throw ImFormatExcep("Invalid offset");


	if(!(infoheader.bits == 8 || infoheader.bits == 24))
		throw ImFormatExcep("unsupported bitdepth of " + ::toString(infoheader.bits));

	if(infoheader.compression != 0)
		throw ImFormatExcep("Only uncompressed BMPs supported.");

	const int width = infoheader.width;
	const int signed_height = infoheader.height; // Height can be negative, this means data is stored top-to-bottom. (see http://en.wikipedia.org/wiki/BMP_file_format)
	const int bytespp = infoheader.bits / 8;

	const int abs_height = std::abs(signed_height);

	const int MAX_DIMS = 10000;
	if(width < 0 || width > MAX_DIMS) 
		throw ImFormatExcep("Invalid image width (" + toString(width) + ")");
	if(abs_height < 0 || abs_height > MAX_DIMS) 
		throw ImFormatExcep("Invalid image height (" + toString(signed_height) + ")");

	Reference<ImageMap<uint8_t, UInt8ComponentValueTraits> > texture = new ImageMap<uint8_t, UInt8ComponentValueTraits>(width, abs_height, bytespp);

	int rowpaddingbytes = 4 - ((width * bytespp) % 4);
	if(rowpaddingbytes == 4)
		rowpaddingbytes = 0;

	//-----------------------------------------------------------------
	//convert into colour array
	//-----------------------------------------------------------------
	if(signed_height < 0)
	{
		// This is a top-to-bottom bitmap:
		for(int y=0; y<abs_height; ++y)
		{
			// Make sure we're not going to read past end of encoded_img
			if(srcindex + infoheader.width * bytespp > (int)encoded_img.size())
				throw ImFormatExcep("Error while parsing BMP");

			for(int x=0; x<infoheader.width; ++x)
			{
				if(bytespp == 1)
				{
					assert(srcindex < encoded_img.size());
					texture->getPixel(x, y)[0] = encoded_img[srcindex++];
				}
				else
				{
					assert(srcindex + 2 < encoded_img.size());

					unsigned char r, g, b;
					b = encoded_img[srcindex++];
					g = encoded_img[srcindex++];
					r = encoded_img[srcindex++];

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
			if(srcindex + infoheader.width * bytespp > (int)encoded_img.size())
				throw ImFormatExcep("Error while parsing BMP");

			for(int x=0; x<infoheader.width; ++x)
			{
				if(bytespp == 1)
				{
					assert(srcindex < encoded_img.size());
					texture->getPixel(x, y)[0] = encoded_img[srcindex++];
				}
				else
				{
					assert(srcindex + 2 < encoded_img.size());

					unsigned char r, g, b;
					b = encoded_img[srcindex++];
					g = encoded_img[srcindex++];
					r = encoded_img[srcindex++];

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
