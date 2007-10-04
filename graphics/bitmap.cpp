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
	data = 0;
}

Bitmap::Bitmap(int width_, int height_, int bytespp_, const unsigned char* srcdata)
:	width(width_),
	height(height_),
	bytespp(bytespp_)
{
	assert(width >= 0 && height >= 0);
	assert(bytespp >= 0);

	assert(sizeof(unsigned char) == 1);

	const int datasize = width * height * bytespp;
	data = new unsigned char[datasize];

	if(srcdata)
	{
		for(int i=0; i<datasize; ++i)
			data[i] = srcdata[i];
	}
}


Bitmap::~Bitmap()
{
	delete[] data;
}


void Bitmap::takePointer(int newwidth, int newheight, int newbytespp, unsigned char* srcdata)
{
	assert(newwidth >= 0 && newheight >= 0);
	assert(newbytespp >= 0);

	width = newwidth;
	height = newheight;
	bytespp = newbytespp;

	delete[] data;

	data = srcdata;
}


void Bitmap::setBytesPP(const int new_bytes_pp)
{
	assert(new_bytes_pp >= 0);

	bytespp = new_bytes_pp;
}


void Bitmap::resize(int newwidth, int newheight)
{
	assert(newwidth >= 0 && newheight >= 0);

	delete[] data;

	width = newwidth;
	height = newheight;

	const int datasize = width * height * bytespp;
	data = new unsigned char[datasize];
}

void Bitmap::raiseToPower(float exponent)
{
	const int datasize = width * height * bytespp;
	for(int i=0; i<datasize; ++i)
	{
		const unsigned char c = data[i];

		data[i] = (unsigned char)(pow((float)c / 255.0f, exponent) * 255.0f);
	}
}


unsigned int Bitmap::checksum() const
{
	const unsigned int initial_crc = crc32(0, 0, 0);
	return crc32(initial_crc, data, width * height * bytespp);
}



