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


const Colour3d FPImageMap16::vec3SampleTiled(double u, double v) const
{
	Colour3d colour_out;

	double intpart; // not used
	double u_frac_part = modf(u, &intpart);
	double v_frac_part = modf(1.0 - v, &intpart); // 1.0 - v because we want v=0 to be at top of image, and v=1 to be at bottom.

	if(u_frac_part < 0.0)
		u_frac_part = 1.0 + u_frac_part;
	if(v_frac_part < 0.0)
		v_frac_part = 1.0 + v_frac_part;

	assert(Maths::inHalfClosedInterval(u_frac_part, 0.0, 1.0));
	assert(Maths::inHalfClosedInterval(v_frac_part, 0.0, 1.0));

	// Convert from normalised image coords to pixel coordinates
	const double u_pixels = u_frac_part * (double)getWidth();
	const double v_pixels = v_frac_part * (double)getHeight();

	assert(Maths::inHalfClosedInterval(u_pixels, 0.0, (double)getWidth()));
	assert(Maths::inHalfClosedInterval(v_pixels, 0.0, (double)getHeight()));

	const unsigned int ut = (unsigned int)u_pixels;
	const unsigned int vt = (unsigned int)v_pixels;

	assert(ut >= 0 && ut < getWidth());
	assert(vt >= 0 && vt < getHeight());

	const unsigned int ut_1 = (ut + 1) % getWidth();
	const unsigned int vt_1 = (vt + 1) % getHeight();

	const double ufrac = u_pixels - (double)ut;
	const double vfrac = v_pixels - (double)vt;
	const double oneufrac = 1.0 - ufrac;
	const double onevfrac = 1.0 - vfrac;

	// Top left pixel
	{
		const half* pixel = getPixel(ut, vt);
		const double factor = oneufrac * onevfrac;
		colour_out.r = (double)pixel[0] * factor;
		colour_out.g = (double)pixel[1] * factor;
		colour_out.b = (double)pixel[2] * factor;
	}


	// Top right pixel
	{
		const half* pixel = getPixel(ut_1, vt);
		const double factor = ufrac * onevfrac;
		colour_out.r += (double)pixel[0] * factor;
		colour_out.g += (double)pixel[1] * factor;
		colour_out.b += (double)pixel[2] * factor;
	}


	// Bottom left pixel
	{
		const half* pixel = getPixel(ut, vt_1);
		const double factor = oneufrac * vfrac;
		colour_out.r += (double)pixel[0] * factor;
		colour_out.g += (double)pixel[1] * factor;
		colour_out.b += (double)pixel[2] * factor;
	}

	// Bottom right pixel
	{
		const half* pixel = getPixel(ut_1, vt_1);
		const double factor = ufrac * vfrac;
		colour_out.r += (double)pixel[0] * factor;
		colour_out.g += (double)pixel[1] * factor;
		colour_out.b += (double)pixel[2] * factor;
	}

	return colour_out;
}


double FPImageMap16::scalarSampleTiled(double x, double y) const
{
	const Colour3d col = vec3SampleTiled(x, y);
	return (col.r + col.g + col.b) * (1.0 / 3.0);
}
