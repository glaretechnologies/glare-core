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


void Bitmap::raiseToPower(float exponent)
{
	const size_t datasize = data.size();
	for(size_t i = 0; i < datasize; ++i)
		data[i] = (uint8)(std::pow(data[i] * (1.0f / 255.0f), exponent) * 255.0f);
}


unsigned int Bitmap::checksum() const
{
	const uint32 initial_crc = crc32(0, 0, 0);
	return crc32(initial_crc, &data[0], width * height * bytespp);
}


void Bitmap::addImage(const Bitmap& img, const int destx, const int desty, const float alpha/* = 1 */)
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

		if(dy < 0) continue;		// Skip to next line if this row is before first.
		if(dy >= dst_yres) break;	// Stop looping if we've hit the last row.

		for(int x = 0; x < img_xres; ++x)
		{
			const int dx = x + destx;

			if(dx >= 0 && dx < dst_xres)
			{
				setPixelComp(dx, dy, 0, (unsigned char)myMin(255, (int)(img.getPixelComp(x, y, 0) * alpha) + (int)getPixelComp(dx, dy, 0)));
				setPixelComp(dx, dy, 1, (unsigned char)myMin(255, (int)(img.getPixelComp(x, y, 1) * alpha) + (int)getPixelComp(dx, dy, 1)));
				setPixelComp(dx, dy, 2, (unsigned char)myMin(255, (int)(img.getPixelComp(x, y, 2) * alpha) + (int)getPixelComp(dx, dy, 2)));
			}
		}
	}
}


void Bitmap::blendImage(const Bitmap& img, const int destx, const int desty, unsigned char solid_colour[3], const float alpha/* = 1 */)
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
				for(unsigned int c = 0; c < 3; ++c)
					setPixelComp(dx, dy, c, (unsigned char)Maths::lerp((float)getPixelComp(dx, dy, c), (float)solid_colour[c], (alpha / 255.0f) * img.getPixelComp(x, y, 0)));
			}
		}
	}
}


void Bitmap::mulImage(const Bitmap& img, const int destx, const int desty, const float alpha/* = 1*/, bool invert/* = false*/)
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
				const float alpha255 = ((invert) ? 255 - img.getPixelComp(x, y, 0) : img.getPixelComp(x, y, 0)) * (alpha / 255.0f);

				for(uint32 c = 0; c < 3; ++c)
					setPixelComp(dx, dy, c, myMin(255, (int)(getPixelComp(dx, dy, c) * (1 - alpha) + getPixelComp(dx, dy, c) * alpha255)));
			}
		}
	}
}


void Bitmap::blitToImage(int src_start_x, int src_start_y, int src_end_x, int src_end_y, Bitmap& dest, int destx, int desty) const
{
	src_start_x = myMax(0, src_start_x);
	src_start_y = myMax(0, src_start_y);

	src_end_x = myMin(src_end_x, (int)getWidth());
	src_end_y = myMin(src_end_y, (int)getHeight());

	for(int y = src_start_y; y < src_end_y; ++y)
		for(int x = src_start_x; x < src_end_x; ++x)
		{
			const int dx = (x - src_start_x) + destx;
			const int dy = (y - src_start_y) + desty;

			if(dx >= 0 && dx < dest.getWidth() && dy >= 0 && dy < dest.getHeight())
				for(uint32 c = 0; c < 3; ++c)
					dest.setPixelComp(dx, dy, c, getPixelComp(x, y, c));
		}
}
