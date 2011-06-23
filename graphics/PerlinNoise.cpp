/*=====================================================================
PerlinNoise.cpp
---------------
File created by ClassTemplate on Fri Jun 08 01:50:46 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "PerlinNoise.h"


#include "../maths/mathstypes.h"


// Explicit template instantiation
template float PerlinNoise::noise(float x, float y, float z);
template float PerlinNoise::FBM(float x, float y, float z, unsigned int num_octaves);


PerlinNoise::PerlinNoise()
{
	
}


PerlinNoise::~PerlinNoise()
{
	
}


//http://mrl.nyu.edu/~perlin/noise/
template <class Real>
inline static Real fade(Real t) 
{ 
	return t * t * t * (t * (t * (Real)6.0 - (Real)15.0) + (Real)10.0); 
}


template <class Real>
inline static Real lerp(Real t, Real a, Real b)
{
	return a + t * (b - a);
}
// return a * t + (1-t)*b


template <class Real>
inline static Real grad(int hash, Real x, Real y, Real z)
{
	const int h = hash & 15;                      // CONVERT LO 4 BITS OF HASH CODE
	Real u = h<8 ? x : y,                 // INTO 12 GRADIENT DIRECTIONS.
		v = h<4 ? y : h==12||h==14 ? x : z;
	return ((h&1) == 0 ? u : -u) + ((h&2) == 0 ? v : -v);
}


const static int permutation[] = { 151,160,137,91,90,15,
   131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
   190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
   88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
   77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
   102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
   135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
   5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
   223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
   129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
   251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
   49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
   138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};


void PerlinNoise::init()
{
	for(int i=0; i<256 ;i++) 
		p[256+i] = p[i] = permutation[i];
}


int PerlinNoise::p[512];


template <class Real>
Real PerlinNoise::noise(Real x, Real y, Real z)
{
	/*const int	X = Maths::floorToInt(x) & 255,                  // FIND UNIT CUBE THAT
			Y = Maths::floorToInt(y) & 255,                  // CONTAINS POINT.
			Z = Maths::floorToInt(z) & 255;
      x -= Maths::floorToInt(x);                                // FIND RELATIVE X,Y,Z
      y -= Maths::floorToInt(y);                                // OF POINT IN CUBE.
      z -= Maths::floorToInt(z);*/

	const int fx = (int)floor(x);
	const int fy = (int)floor(y);
	const int fz = (int)floor(z);
	x = x - fx;
	y = y - fy;
	z = z - fz;
	const int X = fx & 0xFF;
	const int Y = fy & 0xFF;
	const int Z = fz & 0xFF;

	assert(Maths::inHalfClosedInterval(x, (Real)0.0, (Real)1.0));
	assert(Maths::inHalfClosedInterval(y, (Real)0.0, (Real)1.0));
	assert(Maths::inHalfClosedInterval(z, (Real)0.0, (Real)1.0));
	assert(Maths::inHalfClosedInterval(X, 0, 256));
	assert(Maths::inHalfClosedInterval(Y, 0, 256));
	assert(Maths::inHalfClosedInterval(Z, 0, 256));


      const Real u = fade(x),                                // COMPUTE FADE CURVES
             v = fade(y),                                // FOR EACH OF X,Y,Z.
             w = fade(z);
      const int A = p[X  ]+Y, AA = p[A]+Z, AB = p[A+1]+Z,      // HASH COORDINATES OF
          B = p[X+1]+Y, BA = p[B]+Z, BB = p[B+1]+Z;      // THE 8 CUBE CORNERS,

      return lerp(w, lerp(v, lerp(u, grad(p[AA  ], x  , y  , z   ),  // AND ADD
                                     grad(p[BA  ], x-1, y  , z   )), // BLENDED
                             lerp(u, grad(p[AB  ], x  , y-1, z   ),  // RESULTS
                                     grad(p[BB  ], x-1, y-1, z   ))),// FROM  8
                     lerp(v, lerp(u, grad(p[AA+1], x  , y  , z-1 ),  // CORNERS
                                     grad(p[BA+1], x-1, y  , z-1 )), // OF CUBE
                             lerp(u, grad(p[AB+1], x  , y-1, z-1 ),
                                     grad(p[BB+1], x-1, y-1, z-1 ))));
}


template <class Real>
Real PerlinNoise::FBM(Real x, Real y, Real z, unsigned int num_octaves)
{
	Real sum = 0.0;
	Real scale = 1.0;
	Real weight = 1.0;
	for(unsigned int i=0; i<num_octaves; ++i)
	{
		sum += weight * PerlinNoise::noise(x * scale, y * scale, z * scale);
		scale *= (Real)1.99;
		weight *= (Real)0.5;
	}

	return sum;
}
