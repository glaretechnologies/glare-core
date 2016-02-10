/*=====================================================================
GridNoise.cpp
-------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2011-06-22 22:23:03 +0100
=====================================================================*/
#include "GridNoise.h"


#include "../maths/mathstypes.h"
#include "../utils/MTwister.h"
#include "../utils/StringUtils.h"
#include <fstream>


static float data[256];
static uint8 p_x[256];
static uint8 p_y[256];
static uint8 p_z[256];
static uint8 p_w[256];


void GridNoise::init()
{
	MTwister rng(1);

	const int N = 256;

	// Generate some random floating point values stratified over the unit interval.
	for(int i=0; i<N; ++i)
		data[i] = (i + rng.unitRandom()) * (1.0f / N);

	// Permute
	for(int t=N-1; t>=0; --t)
	{
		int k = (int)(rng.unitRandom() * t);
		mySwap(data[t], data[k]);
	}


	for(int i=0; i<N; ++i)
		p_x[i] = p_y[i] = p_z[i] = p_w[i] = (uint8)i;

	// Permute
	for(int t=N-1; t>=0; --t)
	{
		{
			int k = (int)(rng.unitRandom() * t);
			mySwap(p_x[t], p_x[k]);
		}
		{
			int k = (int)(rng.unitRandom() * t);
			mySwap(p_y[t], p_y[k]);
		}
		{
			int k = (int)(rng.unitRandom() * t);
			mySwap(p_z[t], p_z[k]);
		}
		{
			int k = (int)(rng.unitRandom() * t);
			mySwap(p_w[t], p_w[k]);
		}
	}


	// Some code to print out the tables used, so the OpenCL implementation can use the same data
	/*{
		std::ofstream file("gridnoise_data.txt");

		file << "\np_x = ";
		for(int i=0; i<N; ++i)
			file << toString(p_x[i]) + ", ";

		file << "\np_y = ";
		for(int i=0; i<N; ++i)
			file << toString(p_y[i]) + ", ";

		file << "\np_z = ";
		for(int i=0; i<N; ++i)
			file << toString(p_z[i]) + ", ";

		file << "\np_w = ";
		for(int i=0; i<N; ++i)
			file << toString(p_w[i]) + ", ";

		file << "\ndata = ";
		for(int i=0; i<N; ++i)
			file << floatLiteralString(data[i]) + ", ";
	}*/
}


float GridNoise::eval(int x, int y, int z)
{
	// See 'Texturing and Modelling, a Procedural Approach', page 69 - 'Lattice Noises'.
	//int i = perm(x + perm(y + perm(z)));
	//return data[i];

	//NOTE: the XOR approach both looks better and is slightly faster.

	//NOTE: this stuff can be vectorised when we know we have SSE4 (floorps, andps, etc..)

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
