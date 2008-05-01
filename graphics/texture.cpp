/*=====================================================================
texture.cpp
-----------
File created by ClassTemplate on Mon May 02 21:31:01 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "texture.h"




Texture::Texture()
{
	
}


Texture::~Texture()
{
	
}

// u and v are normalised image coordinates.  U goes across image, v goes up image.
void Texture::sampleTiled(double u, double v, Colour3d& colour_out) const
{
	if(getBytesPP() == 1)
		sampleTiled1BytePP(u, v, colour_out);
	else if(getBytesPP() == 3)
		sampleTiled3BytesPP(u, v, colour_out);
	else
	{
		assert(0);
		colour_out = Colour3d(1,0,0);
	}
}


// u and v are normalised image coordinates.  U goes across image, v goes up image.
void Texture::sampleTiled1BytePP(double u, double v, Colour3d& colour_out) const
{
	assert(getBytesPP() == 1);

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

	double colour_result;

	// Top left pixel
	{
	const unsigned char* pixel = getPixel(ut, vt);
	const double factor = oneufrac * onevfrac;
	colour_result = (double)pixel[0] * factor;
	}


	// Top right pixel
	{
	const unsigned char* pixel = getPixel(ut_1, vt);
	const double factor = ufrac * onevfrac;
	colour_result += (double)pixel[0] * factor;
	}

	
	// Bottom left pixel
	{
	const unsigned char* pixel = getPixel(ut, vt_1);
	const double factor = oneufrac * vfrac;
	colour_result += (double)pixel[0] * factor;
	}


	// Bottom right pixel
	{
	const unsigned char* pixel = getPixel(ut_1, vt_1);
	const double factor = ufrac * vfrac;
	colour_result += (double)pixel[0] * factor;
	}

	// Copy red to G and B
	colour_out.set(colour_result, colour_result, colour_result);

	colour_out *= (1.0 / 255.0);
}




// u and v are normalised image coordinates.  U goes across image, v goes up image.
void Texture::sampleTiled3BytesPP(double u, double v, Colour3d& colour_out) const
{
	assert(getBytesPP() == 3);

	// Convert from normalised image coords into pixel coordinates
	//double u_pixels = u * (double)getWidth();
	//double v_pixels = (1.0 - v) * (double)getHeight(); // 1.0 - v because we want v=0 to be at top of image, and v=1 to be at bottom.

	//const int ut_unwrapped = (int)floor(u_pixels);
	//const int vt_unwrapped = (int)floor(v_pixels);

	//int ut = ut_unwrapped % (int)getWidth();
	//int vt = vt_unwrapped % (int)getHeight();

	//assert(ut > -(int)getWidth() && ut < (int)getWidth());
	//assert(vt > -(int)getHeight() && vt < (int)getHeight());

	//if(ut < 0)
	//	ut = getWidth() + ut;
	//if(vt < 0)
	//	vt = getHeight() + vt;

	//assert(ut >= 0 && ut < (int)getWidth());
	//assert(vt >= 0 && vt < (int)getHeight());

	//const unsigned int ut_1 = (ut + 1) % getWidth();
	//const unsigned int vt_1 = (vt + 1) % getHeight();

	//assert(ut < (int)getWidth());
	//assert(vt < (int)getHeight());
	//assert(ut_1 < getWidth());
	//assert(vt_1 < getHeight());

	//const double ufrac = u_pixels - (double)ut_unwrapped;
	//const double vfrac = v_pixels - (double)vt_unwrapped;
	//const double oneufrac = (1.0 - ufrac);
	//const double onevfrac = (1.0 - vfrac);

	//assert(Maths::inRange(ufrac, 0.0, 1.0));
	//assert(Maths::inRange(vfrac, 0.0, 1.0));
	//assert(Maths::inRange(oneufrac, 0.0, 1.0));
	//assert(Maths::inRange(onevfrac, 0.0, 1.0));

	//// Top left pixel
	//{
	//const unsigned char* pixel = getPixel(ut, vt);
	//const double factor = oneufrac * onevfrac;
	//colour_out.r = (double)pixel[0] * factor;
	//colour_out.g = (double)pixel[1] * factor;
	//colour_out.b = (double)pixel[2] * factor;
	//}


	//// Top right pixel
	//{
	//const unsigned char* pixel = getPixel(ut_1, vt);
	//const double factor = ufrac * onevfrac;
	//colour_out.r += (double)pixel[0] * factor;
	//colour_out.g += (double)pixel[1] * factor;
	//colour_out.b += (double)pixel[2] * factor;
	//}

	//
	//// Bottom left pixel
	//{
	//const unsigned char* pixel = getPixel(ut, vt_1);
	//const double factor = oneufrac * vfrac;
	//colour_out.r += (double)pixel[0] * factor;
	//colour_out.g += (double)pixel[1] * factor;
	//colour_out.b += (double)pixel[2] * factor;
	//}

	//// Bottom right pixel
	//{
	//const unsigned char* pixel = getPixel(ut_1, vt_1);
	//const double factor = ufrac * vfrac;
	//colour_out.r += (double)pixel[0] * factor;
	//colour_out.g += (double)pixel[1] * factor;
	//colour_out.b += (double)pixel[2] * factor;
	//}

	//colour_out *= (1.0 / 255.0);
















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
	const unsigned char* pixel = getPixel(ut, vt);
	const double factor = oneufrac * onevfrac;
	colour_out.r = (double)pixel[0] * factor;
	colour_out.g = (double)pixel[1] * factor;
	colour_out.b = (double)pixel[2] * factor;
	}


	// Top right pixel
	{
	const unsigned char* pixel = getPixel(ut_1, vt);
	const double factor = ufrac * onevfrac;
	colour_out.r += (double)pixel[0] * factor;
	colour_out.g += (double)pixel[1] * factor;
	colour_out.b += (double)pixel[2] * factor;
	}

	
	// Bottom left pixel
	{
	const unsigned char* pixel = getPixel(ut, vt_1);
	const double factor = oneufrac * vfrac;
	colour_out.r += (double)pixel[0] * factor;
	colour_out.g += (double)pixel[1] * factor;
	colour_out.b += (double)pixel[2] * factor;
	}

	// Bottom right pixel
	{
	const unsigned char* pixel = getPixel(ut_1, vt_1);
	const double factor = ufrac * vfrac;
	colour_out.r += (double)pixel[0] * factor;
	colour_out.g += (double)pixel[1] * factor;
	colour_out.b += (double)pixel[2] * factor;
	}

	colour_out *= (1.0 / 255.0);





















	//v != y :P
/*	v = 1.0f - v;

	double intpart;
	if(u < 0)
	{
		u = -u;

		//get fractional part of u
		u = (double)modf(u / (double)getWidth(), &intpart);//u is normed after this

		u = 1.0f - u;
		u *= (double)getWidth();
	}
	else
	{
		u = (double)modf(u / (double)getWidth(), &intpart);//u is normed after this
		u *= (double)getWidth();
	}

	if(v < 0)
	{
		v = -v;

		v = (double)modf(v / (double)getHeight(), &intpart);//v is normed after this

		v = 1.0f - v;
		v *= (double)getHeight();
	}
	else
	{
		v = (double)modf(v / (double)getHeight(), &intpart);//v is normed after this
		v *= (double)getHeight();
	}

	int ut = (int)u;//NOTE: redundant
	int vt = (int)v;

	const double ufrac = u - (double)ut;//(float)floor(u);
	const double vfrac = v - (double)vt;//(float)floor(v);
	const double oneufrac = (1.0f - ufrac);
	const double onevfrac = (1.0f - vfrac);



	if(ut < 0) ut = 0;
	if(vt < 0) vt = 0;
	if(ut + 1 >= getWidth()) ut = getWidth() - 2;//need to allow for + 1 pixel in x and y dir
	if(vt + 1 >= getHeight()) vt = getHeight() - 2;

		//const float sum = (ufrac * vfrac) + (ufrac * onevfrac) + 
		//	(oneufrac * onevfrac) + (oneufrac * vfrac);
	const unsigned char* pixel = getPixel(ut, vt);
	const Colour3d u0v0(pixel[0], pixel[1], pixel[2]);

	pixel = getPixel(ut, vt+1);
	const Colour3d u0v1(pixel[0], pixel[1], pixel[2]);

	pixel = getPixel(ut+1, vt+1);
	const Colour3d u1v1(pixel[0], pixel[1], pixel[2]);

	pixel = getPixel(ut+1, vt);
	const Colour3d u1v0(pixel[0], pixel[1], pixel[2]);

	colour_out.set(0,0,0);
	colour_out.addMult(u0v0, oneufrac * onevfrac);
	colour_out.addMult(u0v1, oneufrac * vfrac);
	colour_out.addMult(u1v1, ufrac * vfrac);
	colour_out.addMult(u1v0, ufrac * onevfrac);

	colour_out *= (1.0f / 255.0f);

*/
	/*int x = (int)x_;
	int y = (int)y_;
	if(x < 0)
	{
		x = 0 - x;//x = -x		, so now x is positive
		x = x % getWidth();
		x = getWidth() - x - 1;
	}
	else
	{
		x = x % getWidth();
	}

	if(y < 0)
	{
		y = 0 - y;					
		y = y % getHeight();
		y = getHeight() - y - 1;

	}
	else
	{
		y = y % getHeight();
	}
	
	const unsigned char* pixel = getPixel(x, y);
	col_out.r = (float)pixel[0] / 255.0f;
	col_out.g = (float)pixel[1] / 255.0f;
	col_out.b = (float)pixel[2] / 255.0f;*/
}


