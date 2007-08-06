/*=====================================================================
texture.h
---------
File created by ClassTemplate on Mon May 02 21:31:01 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TEXTURE_H_666_
#define __TEXTURE_H_666_



#include "bitmap.h"
#include "colour3.h"


/*=====================================================================
Texture
-------

=====================================================================*/
class Texture : public Bitmap
{
public:
	/*=====================================================================
	Texture
	-------
	
	=====================================================================*/
	Texture();

	~Texture();


	inline void sampleTiled(double x, double y, Colour3d& col_out) const;


};

inline void Texture::sampleTiled(double u, double v, Colour3d& colour_out) const
{
	//v != y :P
	v = 1.0f - v;

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


#endif //__TEXTURE_H_666_




