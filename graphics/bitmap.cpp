/*=====================================================================
bitmap.cpp
----------
File created by ClassTemplate on Wed Oct 27 03:44:34 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "bitmap.h"


#include <assert.h>
#include "../maths/mathstypes.h"
#include <zlib.h>


Bitmap::Bitmap()
{
	width = 0;
	height = 0;
	bytespp = 3;
}

Bitmap::Bitmap(unsigned int width_, unsigned int height_, unsigned int bytespp_, const unsigned char* srcdata)
:	width(width_),
	height(height_),
	bytespp(bytespp_)
{
	assert(width >= 0 && height >= 0);
	assert(bytespp >= 0);

	assert(sizeof(unsigned char) == 1);

	const unsigned int datasize = width * height * bytespp;
	data.resize(datasize);

	if(srcdata)
		for(unsigned int i=0; i<datasize; ++i)
			data[i] = srcdata[i];

}


Bitmap::~Bitmap()
{

}


void Bitmap::resize(unsigned int newwidth, unsigned int newheight, unsigned int new_bytes_pp)
{
	if(width != newwidth || height != newheight || bytespp != new_bytes_pp)
	{
		width = newwidth;
		height = newheight;
		bytespp = new_bytes_pp;
		data.resize(newwidth * newheight * new_bytes_pp);
	}
}


void Bitmap::raiseToPower(float exponent)
{
	const int datasize = data.size();
	for(int i=0; i<datasize; ++i)
	{
		const unsigned char c = data[i];

		data[i] = (unsigned char)(pow((float)c / 255.0f, exponent) * 255.0f);
	}
}


unsigned int Bitmap::checksum() const
{
	const unsigned int initial_crc = crc32(0, 0, 0);
	return crc32(initial_crc, &data[0], width * height * bytespp);
}


void Bitmap::addImage(const Bitmap& img, int destx, int desty)
{
	for(int y=0; y<(int)img.getHeight(); ++y)
		for(int x=0; x<(int)img.getWidth(); ++x)
		{
			const int dx = x + destx;
			const int dy = y + desty;

			if(dx >= 0 && dx < (int)getWidth() && dy >= 0 && dy < (int)getHeight())
				for(unsigned int c=0; c<3; ++c)
					setPixelComp(dx, dy, c, 
						(unsigned char)myMin((unsigned int)255, (unsigned int)img.getPixelComp(x, y, c) + (unsigned int)getPixelComp(dx, dy, c))
					);
		}
}


void Bitmap::blendImage(const Bitmap& img, int destx, int desty)
{
	for(int y=0; y<(int)img.getHeight(); ++y)
		for(int x=0; x<(int)img.getWidth(); ++x)
		{
			const int dx = x + destx;
			const int dy = y + desty;

			if(dx >= 0 && dx < (int)getWidth() && dy >= 0 && dy < (int)getHeight())
				for(unsigned int c=0; c<3; ++c)
					setPixelComp(dx, dy, c, 
						(unsigned char)Maths::lerp(
							(float)getPixelComp(dx, dy, c), // a
							255.0f, // b (white)
							(1.0f / 255.0f) * (float)img.getPixelComp(x, y, 0) // t
						)
					);
		}
}
