/*=====================================================================
bitmap.cpp
----------
File created by ClassTemplate on Wed Oct 27 03:44:34 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "bitmap.h"


#include "../maths/mathstypes.h"
#include "../utils/Checksum.h"


Bitmap::Bitmap()
{
	width = 0;
	height = 0;
	bytespp = 3;
}

Bitmap::Bitmap(size_t width_, size_t height_, size_t bytespp_, const uint8* srcdata)
:	width(width_),
	height(height_),
	bytespp(bytespp_)
{
	assert(sizeof(unsigned char) == 1);

	const size_t datasize = width * height * bytespp;
	data.resize(datasize);

	if(srcdata)
		for(size_t i = 0; i<datasize; ++i)
			data[i] = srcdata[i];
}


Bitmap::~Bitmap()
{
}


void Bitmap::resize(size_t newwidth, size_t newheight, size_t new_bytes_pp)
{
	if(width != newwidth || height != newheight || bytespp != new_bytes_pp)
	{
		width = newwidth;
		height = newheight;
		bytespp = new_bytes_pp;

		data.resize(newwidth * newheight * new_bytes_pp);
	}
}


unsigned int Bitmap::checksum() const
{
	return Checksum::checksum((void*)&data[0], width * height * bytespp);
}


void Bitmap::blendImageWithWhite(const Bitmap& img, const int destx, const int desty, const float alpha/* = 1 */)
{
	assert((alpha >= 0) && (alpha <= 1));
	const int dst_xres = (int)getWidth();
	const int dst_yres = (int)getHeight();
	const int img_xres = (int)img.getWidth();
	const int img_yres = (int)img.getHeight();

	// Perform trivial rejects.
	if(destx >= dst_xres) return;
	if(desty >= dst_yres) return;
	if((destx + img_xres) < 0) return;
	if((desty + img_yres) < 0) return;

	for(int y = 0; y < img_yres; ++y)
	{
		const int dy = y + desty;

		if(dy < 0) continue; else	// Skip to next line if this row is before first.
		if(dy >= dst_yres) break;	// Stop looping if we've hit the last row.

		for(int x = 0; x < img_xres; ++x)
		{
			const int dx = x + destx;

			if(dx >= 0 && dx < dst_xres)
			{
				// Final_colour[c] = lerp(old_colour[c], solid_colour, t)
				// where t = alpha * img_colour[0]

				const float solid_colour = 255.0f;

				for(unsigned int c = 0; c < bytespp; ++c)
					setPixelComp(dx, dy, c, (unsigned char)Maths::lerp((float)getPixelComp(dx, dy, c), solid_colour, (alpha / 255.0f) * img.getPixelComp(x, y, 0)));
			}
		}
	}
}


void Bitmap::blitToImage(int src_start_x, int src_start_y, int src_end_x, int src_end_y, Bitmap& dest, int destx, int desty) const
{
	assert(dest.getBytesPP() >= 3);

	src_start_x = myMax(0, src_start_x);
	src_start_y = myMax(0, src_start_y);

	src_end_x = myMin(src_end_x, (int)getWidth());
	src_end_y = myMin(src_end_y, (int)getHeight());

	for(int y = src_start_y; y < src_end_y; ++y)
		for(int x = src_start_x; x < src_end_x; ++x)
		{
			const int dx = (x - src_start_x) + destx;
			const int dy = (y - src_start_y) + desty;

			if(dx >= 0 && dx < (int)dest.getWidth() && dy >= 0 && dy < (int)dest.getHeight())
			{
				// NOTE: bit of a hack assuming dest.getBytesPP() >= 3.
				for(uint32 c = 0; c < 3; ++c)
					dest.setPixelComp(dx, dy, c, getPixelComp(x, y, c));

				if(dest.getBytesPP() == 4)
					dest.setPixelComp(dx, dy, 3, 255); // Set alpha to one for now.
			}
		}
}


void Bitmap::zero()
{
	const size_t datasize = data.size();
	for(size_t i=0; i<datasize; ++i)
		data[i] = 0;
}
