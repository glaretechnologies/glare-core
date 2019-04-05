/*=====================================================================
GridNoise.h
-------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2011-06-22 22:23:03 +0100
=====================================================================*/
#pragma once


#include "../maths/mathstypes.h"


/*=====================================================================
GridNoise
-------------------

=====================================================================*/
namespace GridNoise
{
	void generateData();

	inline float eval(float x, float y, float z);
	inline float eval(float x, float y, float z, float w);
	inline float eval(int x, int y, int z);
	inline float eval(int x, int y, int z, int w);
}


extern const uint8 p_x[256];
extern const uint8 p_y[256];
extern const uint8 p_z[256];
extern const uint8 p_w[256];
extern const float data[256];


/*
See 'Texturing and Modelling, a Procedural Approach', page 69 - 'Lattice Noises'.
which gives the code

int i = perm(x + perm(y + perm(z)));
return data[i];

However the XOR approach used below both looks better and is slightly faster.

NOTE: this stuff can be vectorised when we know we have SSE4 (floorps, andps, etc..)
*/
float GridNoise::eval(int x, int y, int z)
{
	const int hash_x  = p_x[x & 0xFF];
	const int hash_y  = p_y[y & 0xFF];
	const int hash_z  = p_z[z & 0xFF];

	return data[hash_x ^ hash_y ^ hash_z];
}


float GridNoise::eval(int x, int y, int z, int w)
{
	const int hash_x  = p_x[x & 0xFF];
	const int hash_y  = p_y[y & 0xFF];
	const int hash_z  = p_z[z & 0xFF];
	const int hash_w  = p_w[w & 0xFF];

	return data[hash_x ^ hash_y ^ hash_z ^ hash_w];
}


float GridNoise::eval(float xf, float yf, float zf)
{
	int x = (int)floor(xf);
	int y = (int)floor(yf);
	int z = (int)floor(zf);

	const int hash_x  = p_x[x & 0xFF];
	const int hash_y  = p_y[y & 0xFF];
	const int hash_z  = p_z[z & 0xFF];

	return data[hash_x ^ hash_y ^ hash_z];
}


float GridNoise::eval(float xf, float yf, float zf, float wf)
{
	int x = (int)floor(xf);
	int y = (int)floor(yf);
	int z = (int)floor(zf);
	int w = (int)floor(wf);

	const int hash_x  = p_x[x & 0xFF];
	const int hash_y  = p_y[y & 0xFF];
	const int hash_z  = p_z[z & 0xFF];
	const int hash_w  = p_w[w & 0xFF];

	return data[hash_x ^ hash_y ^ hash_z ^ hash_w];
}
