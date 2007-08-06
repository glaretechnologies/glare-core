/*=====================================================================
tgadecoder.cpp
--------------
File created by ClassTemplate on Mon Oct 04 23:30:26 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "tgadecoder.h"


#include "bitmap.h"
#include "imformatdecoder.h"
#include <assert.h>
#include <memory.h>
#include <stdlib.h>

//#include "../cyberspace/graphicsdll/mmgrwrapper.h"


TGADecoder::TGADecoder()
{
	
}


TGADecoder::~TGADecoder()
{
	
}

typedef unsigned char byte;


//we need 1 byte alignment for this to work properly
#pragma pack(push, 1)


typedef struct
{
    byte  identsize;          // size of ID field that follows 18 byte header (0 usually)
    byte  colourmaptype;      // type of colour map 0=none, 1=has palette
    byte  imagetype;          // type of image 0=none,1=indexed,2=rgb,3=grey,+8=rle packed

    short colourmapstart;     // first colour map entry in palette
    short colourmaplength;    // number of colours in palette
    byte  colourmapbits;      // number of bits per palette entry 15,16,24,32

    short xstart;             // image x origin
    short ystart;             // image y origin
    short width;              // image width in pixels
    short height;             // image height in pixels
    byte  bits;               // image bits per pixel 8,16,24,32
    byte  descriptor;         // image descriptor bits (vh flip bits)
    
    // pixel data follows header
    
} TGA_HEADER;


#pragma pack(pop)


void TGADecoder::decode(const std::vector<unsigned char>& encoded_img,//const void* encoded_image, int numimagebytes,
						Bitmap& bitmap_out)
{
	assert(sizeof(TGA_HEADER) == 18);
	//assert(numimagebytes >= sizeof(TGA_HEADER));

	if(encoded_img.size() < sizeof(TGA_HEADER))
		throw ImFormatExcep("numimagebytes < sizeof(TGA_HEADER)");


	TGA_HEADER header;
	memcpy(&header, &(*encoded_img.begin()), sizeof(TGA_HEADER));

	const int width = (int)header.width;

	if(width < 0)
		throw ImFormatExcep("width < 0");

	const int height = (int)header.height;

	if(height < 0)
		throw ImFormatExcep("height < 0");

	const int bpp = header.bits;

	const int bytes_pp = bpp / 8;

	const int imagesize = width * height * bytes_pp;

	//NOTE: this chokes on RLE compressed data.
	if((int)encoded_img.size() < (int)sizeof(TGA_HEADER) + imagesize)
		throw ImFormatExcep("not enough data supplied");

	bitmap_out.setBytesPP(bytes_pp);
	bitmap_out.resize(width, height);

	const byte* srcpointer = &(*encoded_img.begin()) + sizeof(TGA_HEADER);

	if(bytes_pp == 1)
	{
		/*for(int i=0; i<imagesize; i += bytes_pp)
		{
			imagedata[i] = srcpointer[i];
		}*/
		for(int x=0; x<width; ++x)
		{
			for(int y=0; y<height; ++y)
			{
				const int srcoffset = (x + width*(height - 1 - y))*bytes_pp;
				const int dstoffset = (x + width*y)*bytes_pp;

				bitmap_out.getData()[dstoffset] = srcpointer[srcoffset];
			}
		}
	}	
	else if(bytes_pp == 3)
	{
		/*for(int i=0; i<imagesize; i += bytes_pp)
		{
			imagedata[i] = srcpointer[i+2];
			imagedata[i+1] = srcpointer[i+1];
			imagedata[i+2] = srcpointer[i];
		}*/
		for(int x=0; x<width; ++x)
		{
			for(int y=0; y<height; ++y)
			{
				const int srcoffset = (x + width*(height - 1 - y))*bytes_pp;
				const int dstoffset = (x + width*y)*bytes_pp;

				bitmap_out.getData()[dstoffset] = srcpointer[srcoffset+2];
				bitmap_out.getData()[dstoffset+1] = srcpointer[srcoffset+1];
				bitmap_out.getData()[dstoffset+2] = srcpointer[srcoffset];
			}
		}
	}
	else if(bytes_pp == 4)
	{
		for(int x=0; x<width; ++x)
		{
			for(int y=0; y<height; ++y)
			{
				const int srcoffset = (x + width*(height - 1 - y))*bytes_pp;
				const int dstoffset = (x + width*y)*bytes_pp;

				bitmap_out.getData()[dstoffset] = srcpointer[srcoffset+2];
				bitmap_out.getData()[dstoffset+1] = srcpointer[srcoffset+1];
				bitmap_out.getData()[dstoffset+2] = srcpointer[srcoffset];
				bitmap_out.getData()[dstoffset+3] = srcpointer[srcoffset+3];
			}
		}
	}
				/*for(int i=0; i<imagesize; i += bytes_pp)
				{
					imagedata[i] = srcpointer[i+2];
					imagedata[i+1] = srcpointer[i+1];
					imagedata[i+2] = srcpointer[i];
					imagedata[i+3] = srcpointer[i+3];
				}*/
	else
	{
		throw ImFormatExcep("invalid bytes per pixel.");
	}
}

void TGADecoder::encode(const Bitmap& bitmap, std::vector<unsigned char>& encoded_img_out)
{
	TGA_HEADER header;
	memset(&header, 0, sizeof(TGA_HEADER));

	header.identsize = 0;
	header.colourmaptype;//no palette
	header.imagetype = 2;//rgb

	header.xstart = 0;
	header.ystart = 0;
	header.width = bitmap.getWidth();
	header.height = bitmap.getHeight();

	header.bits = bitmap.getBytesPP() * 8;
	header.descriptor = 0;

	encoded_img_out.resize(sizeof(TGA_HEADER));
	memcpy(&(*encoded_img_out.begin()), &header, sizeof(TGA_HEADER));

	if(bitmap.getBytesPP() == 3)
	{
		for(int x=0; x<bitmap.getWidth(); ++x)
		{
			for(int y=0; y<bitmap.getHeight(); ++y)
			{
				encoded_img_out.push_back(bitmap.getPixel(x, y)[2]);
				encoded_img_out.push_back(bitmap.getPixel(x, y)[1]);
				encoded_img_out.push_back(bitmap.getPixel(x, y)[0]);
			}
		}
	}
	else
	{
		assert(0);
	}

}


