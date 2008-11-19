/*=====================================================================
FPImageMap16.cpp
----------------
File created by ClassTemplate on Sun May 18 21:48:49 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "FPImageMap16.h"



FPImageMap16::FPImageMap16(unsigned int width_, unsigned int height_)
:	width(width_),
	height(height_)
{
	data.resize(width * height * 3);
}


FPImageMap16::~FPImageMap16()
{
	
}


const Colour3<FPImageMap16::Value> FPImageMap16::vec3SampleTiled(Coord u, Coord v) const
{
	Colour3<Value> colour_out;

	Coord intpart; // not used
	Coord u_frac_part = modf(u, &intpart);
	Coord v_frac_part = modf(1.0 - v, &intpart); // 1.0 - v because we want v=0 to be at top of image, and v=1 to be at bottom.

	if(u_frac_part < 0.0)
		u_frac_part = 1.0 + u_frac_part;
	if(v_frac_part < 0.0)
		v_frac_part = 1.0 + v_frac_part;

	assert(Maths::inHalfClosedInterval<Coord>(u_frac_part, 0.0, 1.0));
	assert(Maths::inHalfClosedInterval<Coord>(v_frac_part, 0.0, 1.0));

	// Convert from normalised image coords to pixel coordinates
	const Coord u_pixels = u_frac_part * (Coord)getWidth();
	const Coord v_pixels = v_frac_part * (Coord)getHeight();

	assert(Maths::inHalfClosedInterval<Coord>(u_pixels, 0.0, (Coord)getWidth()));
	assert(Maths::inHalfClosedInterval<Coord>(v_pixels, 0.0, (Coord)getHeight()));

	const unsigned int ut = (unsigned int)u_pixels;
	const unsigned int vt = (unsigned int)v_pixels;

	assert(ut >= 0 && ut < getWidth());
	assert(vt >= 0 && vt < getHeight());

	const unsigned int ut_1 = (ut + 1) % getWidth();
	const unsigned int vt_1 = (vt + 1) % getHeight();

	const Coord ufrac = u_pixels - (Coord)ut;
	const Coord vfrac = v_pixels - (Coord)vt;
	const Coord oneufrac = 1.0 - ufrac;
	const Coord onevfrac = 1.0 - vfrac;

	// Top left pixel
	{
		const half* pixel = getPixel(ut, vt);
		const Value factor = oneufrac * onevfrac;
		colour_out.r = pixel[0] * factor;
		colour_out.g = pixel[1] * factor;
		colour_out.b = pixel[2] * factor;
	}


	// Top right pixel
	{
		const half* pixel = getPixel(ut_1, vt);
		const Value factor = ufrac * onevfrac;
		colour_out.r += pixel[0] * factor;
		colour_out.g += pixel[1] * factor;
		colour_out.b += pixel[2] * factor;
	}


	// Bottom left pixel
	{
		const half* pixel = getPixel(ut, vt_1);
		const Value factor = oneufrac * vfrac;
		colour_out.r += pixel[0] * factor;
		colour_out.g += pixel[1] * factor;
		colour_out.b += pixel[2] * factor;
	}

	// Bottom right pixel
	{
		const half* pixel = getPixel(ut_1, vt_1);
		const Value factor = ufrac * vfrac;
		colour_out.r += pixel[0] * factor;
		colour_out.g += pixel[1] * factor;
		colour_out.b += pixel[2] * factor;
	}

	return colour_out;
}


FPImageMap16::Value FPImageMap16::scalarSampleTiled(Coord x, Coord y) const
{
	const Colour3<Value> col = vec3SampleTiled(x, y);
	return (col.r + col.g + col.b) * (1.0 / 3.0);
}
