/*=====================================================================
bitmap.cpp
----------
File created by ClassTemplate on Wed Oct 27 03:44:34 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "bitmap.h"


#include <assert.h>
#include <math.h>
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

