/*=====================================================================
PerlinNoise.cpp
---------------
File created by ClassTemplate on Fri Jun 08 01:50:46 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "PerlinNoise.h"


#include "GridNoise.h"
#include "Voronoi.h"
#include "../maths/mathstypes.h"
#include "../maths/vec2.h"
#include "../maths/Vec4i.h"
#include "../indigo/globals.h"
//#include "../indigo/HaltonSampler.h"
#include "../utils/StringUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/MTwister.h"
#include "../maths/GeometrySampling.h"
#include <fstream>


/*

See http://mrl.nyu.edu/~perlin/noise/
also http://www.cs.utah.edu/~aek/research/noise.pdf

*/


// Explicit template instantiation
template float PerlinNoise::noiseRef(float x, float y, float z);
template float PerlinNoise::FBM2(const Vec4f& p, float H, float lacunarity, float octaves);
template float PerlinNoise::ridgedFBM(const Vec4f& p, float H, float lacunarity, float octaves);
template float PerlinNoise::voronoiFBM(const Vec4f& p, float H, float lacunarity, float octaves);
template float PerlinNoise::multifractal(const Vec4f& p, float H, float lacunarity, float octaves, float offset);
template float PerlinNoise::ridgedMultifractal(const Vec4f& p, float H, float lacunarity, float octaves, float offset);
template float PerlinNoise::voronoiMultifractal(const Vec4f& p, float H, float lacunarity, float octaves, float offset);


const static uint8 permutation[] = { 151,160,137,91,90,15,
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


static Vec4f new_gradients[256];
static int masks[4]; // For perlin 4-valued noise

uint8 PerlinNoise::p[512];
uint8 PerlinNoise::p_masked[512];
bool PerlinNoise::have_sse4;

// Same as data in GridNoise.cpp.  This is the same data as used in OpenCL (winter_opencl_support_code.cl).
static const uint8 p_x[256] = { 231, 101, 162, 155, 221, 105, 34, 28, 116, 181, 138, 154, 144, 45, 255, 17, 251, 226, 204, 21, 81, 49, 129, 118, 214, 174, 62, 8, 102, 35, 93, 179, 89, 246, 98, 128, 170, 171, 0, 57, 61, 111, 59, 97, 217, 203, 32, 86, 132, 65, 108, 75, 228, 235, 91, 74, 156, 215, 223, 158, 193, 30, 152, 202, 25, 77, 222, 243, 211, 183, 42, 122, 206, 233, 197, 120, 9, 54, 220, 173, 56, 16, 6, 117, 145, 12, 161, 96, 27, 178, 121, 124, 68, 131, 248, 26, 125, 199, 191, 141, 44, 67, 82, 2, 109, 112, 13, 139, 36, 107, 234, 15, 52, 187, 1, 201, 249, 113, 115, 207, 166, 254, 219, 205, 185, 64, 244, 7, 73, 90, 106, 157, 41, 11, 151, 164, 50, 227, 71, 236, 40, 150, 165, 46, 172, 103, 137, 63, 176, 209, 210, 238, 22, 18, 38, 140, 186, 177, 29, 23, 127, 247, 87, 100, 240, 149, 133, 230, 76, 66, 4, 78, 3, 24, 143, 148, 194, 192, 19, 14, 188, 84, 196, 212, 94, 200, 110, 224, 39, 10, 142, 47, 252, 85, 237, 168, 5, 218, 70, 104, 198, 195, 182, 241, 232, 225, 130, 53, 242, 250, 147, 20, 175, 184, 180, 69, 169, 245, 31, 119, 33, 114, 51, 92, 55, 163, 43, 58, 208, 135, 79, 37, 159, 253, 229, 88, 80, 134, 123, 136, 126, 60, 160, 213, 83, 48, 239, 153, 99, 95, 167, 216, 190, 189, 72, 146 }; 
static const uint8 p_y[256] = { 50, 163, 25, 146, 86, 180, 182, 69, 208, 58, 249, 0, 118, 7, 183, 155, 176, 210, 42, 218, 204, 142, 156, 119, 66, 101, 97, 29, 232, 149, 80, 195, 47, 63, 141, 31, 83, 38, 26, 217, 76, 143, 185, 152, 11, 230, 100, 244, 186, 169, 95, 193, 82, 89, 226, 99, 62, 90, 52, 33, 191, 221, 220, 4, 181, 96, 92, 5, 205, 130, 56, 207, 3, 24, 39, 30, 139, 225, 160, 122, 246, 117, 248, 213, 237, 41, 140, 255, 134, 121, 110, 197, 242, 251, 51, 40, 78, 60, 32, 79, 57, 178, 45, 151, 36, 241, 14, 27, 254, 224, 173, 223, 243, 231, 202, 209, 158, 190, 71, 104, 23, 214, 37, 55, 240, 137, 170, 219, 203, 144, 21, 13, 239, 54, 85, 77, 8, 22, 115, 28, 46, 199, 73, 126, 20, 222, 228, 235, 253, 74, 148, 88, 128, 188, 171, 216, 172, 87, 212, 12, 179, 124, 17, 167, 6, 184, 168, 112, 164, 65, 162, 98, 250, 127, 114, 48, 129, 68, 145, 81, 187, 245, 102, 135, 189, 229, 16, 91, 136, 166, 200, 43, 93, 107, 94, 157, 61, 113, 175, 161, 252, 194, 111, 196, 211, 238, 247, 64, 233, 147, 159, 9, 132, 133, 75, 84, 154, 234, 215, 34, 44, 174, 70, 35, 236, 2, 108, 49, 123, 192, 109, 120, 153, 201, 10, 165, 150, 15, 105, 19, 206, 72, 125, 227, 53, 67, 103, 138, 131, 59, 116, 1, 198, 18, 177, 106 }; 
static const uint8 p_z[256] = { 139, 47, 196, 125, 189, 195, 215, 57, 221, 77, 178, 59, 159, 232, 248, 94, 251, 176, 186, 224, 249, 234, 116, 164, 83, 180, 172, 236, 101, 141, 167, 245, 214, 34, 63, 133, 120, 191, 179, 27, 243, 90, 97, 222, 37, 117, 45, 237, 92, 184, 61, 100, 67, 188, 10, 2, 142, 58, 165, 144, 255, 8, 85, 108, 87, 12, 121, 28, 231, 41, 52, 14, 254, 95, 201, 15, 81, 244, 72, 242, 119, 74, 93, 246, 9, 32, 17, 118, 64, 68, 161, 38, 154, 235, 247, 103, 55, 220, 84, 162, 205, 69, 111, 228, 173, 70, 211, 49, 56, 130, 35, 140, 230, 82, 208, 131, 153, 89, 212, 170, 209, 3, 174, 73, 177, 21, 157, 233, 44, 31, 24, 148, 23, 134, 30, 113, 185, 33, 169, 43, 218, 88, 158, 122, 238, 182, 216, 168, 54, 151, 240, 225, 40, 124, 199, 62, 114, 155, 98, 227, 197, 145, 0, 1, 20, 6, 66, 79, 239, 126, 210, 19, 11, 13, 200, 75, 60, 123, 193, 76, 48, 171, 129, 147, 99, 138, 156, 198, 150, 96, 86, 50, 39, 253, 36, 135, 241, 146, 5, 26, 204, 127, 109, 29, 213, 91, 229, 53, 152, 7, 128, 223, 181, 202, 115, 187, 4, 226, 163, 143, 136, 105, 22, 137, 190, 46, 250, 25, 207, 71, 51, 107, 132, 203, 42, 18, 16, 110, 183, 252, 192, 206, 65, 219, 194, 104, 166, 106, 78, 102, 112, 80, 175, 217, 149, 160 }; 



void PerlinNoise::init()
{
	for(int i=0; i<256 ;i++) 
		p[256+i] = p[i] = permutation[i];

	for(int i=0; i<512 ;i++)
		p_masked[i] = p[i] & 0xF;

	have_sse4 = false;

	try
	{
		PlatformUtils::CPUInfo info;
		PlatformUtils::getCPUInfo(info);

		have_sse4 = info.sse4_1;
	}
	catch(PlatformUtils::PlatformUtilsExcep&)
	{
		assert(0);
	}

	MTwister rng(1);

	const int N = 256;
	/*for(int i=0; i<N; ++i)
		p_x[i] = p_y[i] = p_z[i] = (uint8)i;

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
	}*/
	
	// Make new_gradients
	//HaltonSampler halton;
	
	for(int i=0; i<N; ++i)
	{
		//float samples[2];
		//halton.getSamples(samples, 2);

		//new_gradients[i] = GeometrySampling::unitSquareToSphere(Vec2f(samples[0], samples[1])).toVec4fVector() * std::sqrt(2.f);
		new_gradients[i] = GeometrySampling::unitSquareToSphere(Vec2f(rng.unitRandom(), rng.unitRandom())).toVec4fVector() * std::sqrt(2.f);
	}

	// Permute directions
	/*for(int t=N-1; t>=0; --t)
	{
		{
			int k = (int)(rng.unitRandom() * N);
			mySwap(new_gradients[t], new_gradients[k]);
		}
	}*/

	// Compute a random bit mask for each result dimension
	for(int i=0; i<4; ++i)
		masks[i] = (int)rng.genrand_int32() & 0xFF;


	// Some code to print out the tables used, so the OpenCL implementation can use the same data
	if(false)
	{
		std::ofstream file("perlin_data.txt");

		/*file << "\np_x = ";
		for(int i=0; i<N; ++i)
			file << toString(p_x[i]) + ", ";

		file << "\np_y = ";
		for(int i=0; i<N; ++i)
			file << toString(p_y[i]) + ", ";

		file << "\np_z = ";
		for(int i=0; i<N; ++i)
			file << toString(p_z[i]) + ", ";*/

		file << "\nfloat new_gradients[256] = {";
		for(int i=0; i<N; ++i)
			file << ::floatLiteralString(new_gradients[i].x[0]) << ", " << ::floatLiteralString(new_gradients[i].x[1]) << ", " << ::floatLiteralString(new_gradients[i].x[2]) << ", 0.0f, ";
		file << "};\n";
		
		file << "masks = {";
		for(int i=0; i<4; ++i)
			file << toString(masks[i]) + ", ";
		file << "};\n";
	}
}


//http://mrl.nyu.edu/~perlin/noise/
template <class Real>
inline static Real fade(Real t) 
{ 
	return t * t * t * (t * (t * 6 - 15) + 10); 
}


static inline Vec4f operator * (const Vec4f& a, const Vec4f& b)
{
	return _mm_mul_ps(a.v, b.v);
}


inline const Vec4f fade(const Vec4f& t) 
{ 
	return t * t * t * (t * (t * Vec4f(6) - Vec4f(15)) + Vec4f(10)); 
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



// Reference Perlin noise implementation, based off http://mrl.nyu.edu/~perlin/noise/
template <class Real>
Real PerlinNoise::noiseRef(Real x, Real y, Real z)
{
	// (fx, fy, fz) are the integer lattice coordinates.
	const int fx = (int)floor(x);
	const int fy = (int)floor(y);
	const int fz = (int)floor(z);
	// (x, y, z) are the fractional coordinates inside the lattice cell, taking values [0, 1)
	x = x - fx;
	y = y - fy;
	z = z - fz;
	// Get lowest 8 bits of integer lattice coordinates
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


float PerlinNoise::noise(const Vec4f& point)
{
	// Switch implementations based on the results of the SSE4 query
	if(true) // PerlinNoise::have_sse4)
		return noiseImpl<true>(point); // Use SSE 4
	else
		return noiseImpl<false>(point); // Don't use SSE 4
}


// Fast Perlin noise implementation using SSE, for 2-vector input
float PerlinNoise::noise(float x, float y)
{
	// Switch implementations based on the results of the SSE4 query
	if(true) // if(PerlinNoise::have_sse4)
		return noiseImpl<true>(x, y); // Use SSE 4
	else
		return noiseImpl<false>(x, y); // Don't use SSE 4
}


const Vec4f PerlinNoise::noise4Valued(const Vec4f& point)
{
	if(true) // if(PerlinNoise::have_sse4)
		return noise4ValuedImpl<true>(point); // Use SSE 4
	else
		return noise4ValuedImpl<false>(point); // Don't use SSE 4
}


const Vec4f PerlinNoise::noise4Valued(float x, float y)
{
	if(true) // if(PerlinNoise::have_sse4)
		return noise4ValuedImpl<true>(x, y); // Use SSE 4
	else
		return noise4ValuedImpl<false>(x, y); // Don't use SSE 4
}


/*
	Fast Perlin noise implementation using SSE
	==========================================

	for each 8 corner positions p_c with gradient vector g_c

	d_c = dot(p - p_c, g_c)

	where p_c = floored(p) + offset[c]

	so 
	d_c = dot(p - (floored(p) + offset[c]), g_c)
	    = dot(p - floored(p) - offset[c], g_c)
		= dot(fractional - offset[c], g_c)

	res = w_0*d_0 + w_1*d_1 + w_2*d_2 + ... + w_7*d_7

	w_0 = (1 - u) * (1 - v) * (1 - w)
	w_1 = (    u) * (1 - v) * (1 - w)
	w_2 = (1 - u) * (    v) * (1 - w)

	(w_0*d_0)_x = w_0 * d_c_x
				= w_0 * (fractional_x - offset[0]_x) * g_0_x
	(w_1*d_1)_x = w_1 * (fractional_x - offset[1]_x) * g_1_x
	(w_2*d_2)_x = w_2 * (fractional_x - offset[2]_x) * g_2_x
	(w_3*d_3)_x = w_3 * (fractional_x - offset[3]_x) * g_3_x
	...
*/


// Chosen to match Perlin's grad() function
/*static const Vec4f gradients[16] = {
	Vec4f(1,1,0,0),
	Vec4f(-1,1,0,0),
	Vec4f(1,-1,0,0),
	Vec4f(-1,-1,0,0),
	Vec4f(1,0,1,0),
	Vec4f(-1,0,1,0),
	Vec4f(1,0,-1,0),
	Vec4f(-1,0,-1,0),
	Vec4f(0,1,1,0),
	Vec4f(0,-1,1,0),
	Vec4f(0,1,-1,0),
	Vec4f(0,-1,-1,0),
	Vec4f(1,1,0,0),
	Vec4f(0,-1,1,0),
	Vec4f(-1,1,0,0),
	Vec4f(0,-1,-1,0)
};*/


template <bool use_sse4>
float PerlinNoise::noiseImpl(const Vec4f& point)
{
	Vec4f floored;
	if(use_sse4)
		floored = floor(point); // SSE 4 floor.
	else
		floored = Vec4f(std::floor(point.x[0]), std::floor(point.x[1]), std::floor(point.x[2]), 0);

	// Get lowest 8 bits of integer lattice coordinates
	const Vec4i floored_i = toVec4i(floored) & Vec4i(0xFF, 0xFF, 0xFF, 0xFF);

	const int X = floored_i.x[0];
	const int Y = floored_i.x[1];
	const int Z = floored_i.x[2];
	assert(X >= 0 && X < 256);
	assert(Y >= 0 && Y < 256);
	assert(Z >= 0 && Z < 256);

	const int hash_x  = p_x[X];
	const int hash_x1 = p_x[(X + 1) & 0xFF];
	const int hash_y  = p_y[Y];
	const int hash_y1 = p_y[(Y + 1) & 0xFF];
	const int hash_z  = p_z[Z];
	const int hash_z1 = p_z[(Z + 1) & 0xFF];

	
	const Vec4f fractional = point - floored;
	const Vec4f uvw = fade(fractional);
	const Vec4f one_uvw = Vec4f(1) - uvw;

	//const Vec4f fractional_x = copyToAll<0>(fractional);
	//const Vec4f fractional_y = copyToAll<1>(fractional);
	//const Vec4f fractional_z = copyToAll<2>(fractional);
	const Vec4f fractional_x(fractional.x[0]);
	const Vec4f fractional_y(fractional.x[1]);
	const Vec4f fractional_z(fractional.x[2]);

	const Vec4f u_scale(-1, 1, -1, 1), u_offset(1,0,1,0);
	const Vec4f v_scale(-1, -1, 1, 1), v_offset(1,1,0,0);

	const Vec4f u = copyToAll<0>(uvw); // (u, u, u, u)
	const Vec4f uvec = u * u_scale + u_offset; // (1 - u, u, 1 - u, u)

	const Vec4f v = copyToAll<1>(uvw); // (v, v, v, v)
	const Vec4f vvec = v * v_scale + v_offset; // (1 - v, 1 - v, v, v)

	const Vec4f uv = uvec * vvec; // ((1 - u)(1 - v), u(1 - v), (1 - u)v, uv)

	const Vec4f gradient_i_0 = new_gradients[hash_x  ^ hash_y  ^ hash_z ];
	const Vec4f gradient_i_1 = new_gradients[hash_x1 ^ hash_y  ^ hash_z ];
	const Vec4f gradient_i_2 = new_gradients[hash_x  ^ hash_y1 ^ hash_z ];
	const Vec4f gradient_i_3 = new_gradients[hash_x1 ^ hash_y1 ^ hash_z ];
	const Vec4f gradient_i_4 = new_gradients[hash_x  ^ hash_y  ^ hash_z1];
	const Vec4f gradient_i_5 = new_gradients[hash_x1 ^ hash_y  ^ hash_z1];
	const Vec4f gradient_i_6 = new_gradients[hash_x  ^ hash_y1 ^ hash_z1];
	const Vec4f gradient_i_7 = new_gradients[hash_x1 ^ hash_y1 ^ hash_z1];
	
	/*
	Offsets (cube corners) are
	Vec4f(0,0,0,0),
	Vec4f(1,0,0,0),
	Vec4f(0,1,0,0),
	Vec4f(1,1,0,0),
	Vec4f(0,0,1,0),
	Vec4f(1,0,1,0),
	Vec4f(0,1,1,0),
	Vec4f(1,1,1,0)
	*/

	const Vec4f frac_offset_x = fractional_x - Vec4f(0, 1, 0, 1);
	const Vec4f frac_offset_y = fractional_y - Vec4f(0, 0, 1, 1);
	
	//==================== Corners 0 - 3 ======================
	Vec4f gradients_x, gradients_y, gradients_z, gradients_w;

	transpose(gradient_i_0, gradient_i_1, gradient_i_2, gradient_i_3,
		gradients_x, gradients_y, gradients_z, gradients_w);

	const Vec4f weighted_dot_x = frac_offset_x * gradients_x;
	const Vec4f weighted_dot_y = frac_offset_y * gradients_y;
	const Vec4f weighted_dot_z = fractional_z  * gradients_z;

	const Vec4f sum_weighted_dot = (weighted_dot_x + weighted_dot_y + weighted_dot_z)/* * uv */* copyToAll<2>(one_uvw);

	//==================== Corners 4 - 7 ======================
	transpose(gradient_i_4, gradient_i_5, gradient_i_6, gradient_i_7,
		gradients_x, gradients_y, gradients_z, gradients_w);

	const Vec4f weighted_dot_x_2 = frac_offset_x * gradients_x;
	const Vec4f weighted_dot_y_2 = frac_offset_y * gradients_y;
	const Vec4f weighted_dot_z_2 = (fractional_z - Vec4f(1,1,1,1)) * gradients_z;
		
	const Vec4f sum_weighted_dot_2 = (weighted_dot_x_2 + weighted_dot_y_2 + weighted_dot_z_2)/* * uv*/ * copyToAll<2>(uvw);

	const Vec4f sum = (sum_weighted_dot + sum_weighted_dot_2) * uv;
	
	return sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
}


// 2-vector input
template <bool use_sse4>
float PerlinNoise::noiseImpl(float x, float y)
{
	const Vec4f point(x, y, 0, 0);
	Vec4f floored;
	if(use_sse4)
		floored = floor(point); // SSE 4 floor.
	else
		floored = Vec4f(std::floor(point.x[0]), std::floor(point.x[1]), 0, 0);

	// Get lowest 8 bits of integer lattice coordinates
	const Vec4i floored_i = toVec4i(floored) & Vec4i(0xFF, 0xFF, 0xFF, 0xFF);

	const int X = floored_i.x[0];
	const int Y = floored_i.x[1];
	
	const int hash_x  = p_x[X];
	const int hash_x1 = p_x[(X + 1) & 0xFF];
	const int hash_y  = p_y[Y];
	const int hash_y1 = p_y[(Y + 1) & 0xFF];

	
	const Vec4f fractional = point - floored;
	const Vec4f uvw = fade(fractional);

	const Vec4f fractional_x(fractional.x[0]);
	const Vec4f fractional_y(fractional.x[1]);

	const Vec4f u_scale(-1, 1, -1, 1), u_offset(1,0,1,0);
	const Vec4f v_scale(-1, -1, 1, 1), v_offset(1,1,0,0);

	const Vec4f u = copyToAll<0>(uvw); // (u, u, u, u)
	const Vec4f uvec = u * u_scale + u_offset; // (1 - u, u, 1 - u, u)

	const Vec4f v = copyToAll<1>(uvw); // (v, v, v, v)
	const Vec4f vvec = v * v_scale + v_offset; // (1 - v, 1 - v, v, v)

	const Vec4f uv = uvec * vvec; // ((1 - u)(1 - v), u(1 - v), (1 - u)v, uv)

	const Vec4f gradient_i_0 = new_gradients[hash_x  ^ hash_y ];
	const Vec4f gradient_i_1 = new_gradients[hash_x1 ^ hash_y ];
	const Vec4f gradient_i_2 = new_gradients[hash_x  ^ hash_y1];
	const Vec4f gradient_i_3 = new_gradients[hash_x1 ^ hash_y1];

	/*
	Offsets (cube corners) are
	Vec4f(0,0,0,0),
	Vec4f(1,0,0,0),
	Vec4f(0,1,0,0),
	Vec4f(1,1,0,0),
	*/

	const Vec4f frac_offset_x = fractional_x - Vec4f(0, 1, 0, 1);
	const Vec4f frac_offset_y = fractional_y - Vec4f(0, 0, 1, 1);
	
	Vec4f gradients_x, gradients_y, gradients_z, gradients_w;

	transpose(gradient_i_0, gradient_i_1, gradient_i_2, gradient_i_3,
		gradients_x, gradients_y, gradients_z, gradients_w);

	const Vec4f weighted_dot_x = frac_offset_x * gradients_x;
	const Vec4f weighted_dot_y = frac_offset_y * gradients_y;

	const Vec4f sum = (weighted_dot_x + weighted_dot_y) * uv;
	
	return sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
}


/*
Fast Perlin noise implementation that returns a 4-vector, with each component decorrelated noise.
*/
template <bool use_sse4>
const Vec4f PerlinNoise::noise4ValuedImpl(const Vec4f& point)
{
	Vec4f floored;
	if(use_sse4)
		floored = floor(point); // SSE 4 floor.
	else
		floored = Vec4f(std::floor(point.x[0]), std::floor(point.x[1]), std::floor(point.x[2]), 0);

	// Get lowest 8 bits of integer lattice coordinates
	const Vec4i floored_i = toVec4i(floored) & Vec4i(0xFF, 0xFF, 0xFF, 0xFF);

	const int X = floored_i.x[0];
	const int Y = floored_i.x[1];
	const int Z = floored_i.x[2];
	
	const int hash_x  = p_x[X];
	const int hash_x1 = p_x[(X + 1) & 0xFF];
	const int hash_y  = p_y[Y];
	const int hash_y1 = p_y[(Y + 1) & 0xFF];
	const int hash_z  = p_z[Z];
	const int hash_z1 = p_z[(Z + 1) & 0xFF];



	const Vec4f fractional = point - floored;
	const Vec4f uvw = fade(fractional);
	const Vec4f one_uvw = Vec4f(1) - uvw;

	const Vec4f fractional_x(fractional.x[0]);
	const Vec4f fractional_y(fractional.x[1]);
	const Vec4f fractional_z(fractional.x[2]);

	const Vec4f u_scale(-1, 1, -1, 1), u_offset(1,0,1,0);
	const Vec4f v_scale(-1, -1, 1, 1), v_offset(1,1,0,0);

	const Vec4f u = copyToAll<0>(uvw); // (u, u, u, u)
	const Vec4f uvec = u * u_scale + u_offset; // (1 - u, u, 1 - u, u)

	const Vec4f v = copyToAll<1>(uvw); // (v, v, v, v)
	const Vec4f vvec = v * v_scale + v_offset; // (1 - v, 1 - v, v, v)

	const Vec4f uv = uvec * vvec;


	Vec4f res;
	const Vec4f offsets_x(0, 1, 0, 1);
	const Vec4f offsets_y(0, 0, 1, 1);
	const Vec4f rel_offset_x = fractional_x - offsets_x;
	const Vec4f rel_offset_y = fractional_y - offsets_y;
	const Vec4f rel_offset_z = fractional_z;
	const Vec4f rel_offset_z2 = fractional_z - Vec4f(1,1,1,1);

	const int h0 = hash_x  ^ hash_y  ^ hash_z ;
	const int h1 = hash_x1 ^ hash_y  ^ hash_z ;
	const int h2 = hash_x  ^ hash_y1 ^ hash_z ;
	const int h3 = hash_x1 ^ hash_y1 ^ hash_z ;
	const int h4 = hash_x  ^ hash_y  ^ hash_z1;
	const int h5 = hash_x1 ^ hash_y  ^ hash_z1;
	const int h6 = hash_x  ^ hash_y1 ^ hash_z1;
	const int h7 = hash_x1 ^ hash_y1 ^ hash_z1;

	Vec4f gradients_x, gradients_y, gradients_z, gradients_w;
	{
		const int mask = masks[0];
		const Vec4f gradient_0 = new_gradients[h0 ^ mask];
		const Vec4f gradient_1 = new_gradients[h1 ^ mask];
		const Vec4f gradient_2 = new_gradients[h2 ^ mask];
		const Vec4f gradient_3 = new_gradients[h3 ^ mask];
		const Vec4f gradient_4 = new_gradients[h4 ^ mask];
		const Vec4f gradient_5 = new_gradients[h5 ^ mask];
		const Vec4f gradient_6 = new_gradients[h6 ^ mask];
		const Vec4f gradient_7 = new_gradients[h7 ^ mask];

		transpose(gradient_0, gradient_1, gradient_2, gradient_3, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum_weighted_dot =   (rel_offset_x * gradients_x + rel_offset_y * gradients_y + rel_offset_z  * gradients_z) * copyToAll<2>(one_uvw);
		transpose(gradient_4, gradient_5, gradient_6, gradient_7, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum_weighted_dot_2 = (rel_offset_x * gradients_x + rel_offset_y * gradients_y + rel_offset_z2 * gradients_z) * copyToAll<2>(uvw);
		const Vec4f sum = (sum_weighted_dot + sum_weighted_dot_2) * uv;
		res.x[0] = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
	}
	{
		const int mask = masks[1];
		const Vec4f gradient_0 = new_gradients[h0 ^ mask];
		const Vec4f gradient_1 = new_gradients[h1 ^ mask];
		const Vec4f gradient_2 = new_gradients[h2 ^ mask];
		const Vec4f gradient_3 = new_gradients[h3 ^ mask];
		const Vec4f gradient_4 = new_gradients[h4 ^ mask];
		const Vec4f gradient_5 = new_gradients[h5 ^ mask];
		const Vec4f gradient_6 = new_gradients[h6 ^ mask];
		const Vec4f gradient_7 = new_gradients[h7 ^ mask];

		transpose(gradient_0, gradient_1, gradient_2, gradient_3, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum_weighted_dot =   (rel_offset_x * gradients_x + rel_offset_y * gradients_y + rel_offset_z  * gradients_z) * copyToAll<2>(one_uvw);
		transpose(gradient_4, gradient_5, gradient_6, gradient_7, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum_weighted_dot_2 = (rel_offset_x * gradients_x + rel_offset_y * gradients_y + rel_offset_z2 * gradients_z) * copyToAll<2>(uvw);
		const Vec4f sum = (sum_weighted_dot + sum_weighted_dot_2) * uv;
		res.x[1] = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
	}

	{
		const int mask = masks[2];
		const Vec4f gradient_0 = new_gradients[h0 ^ mask];
		const Vec4f gradient_1 = new_gradients[h1 ^ mask];
		const Vec4f gradient_2 = new_gradients[h2 ^ mask];
		const Vec4f gradient_3 = new_gradients[h3 ^ mask];
		const Vec4f gradient_4 = new_gradients[h4 ^ mask];
		const Vec4f gradient_5 = new_gradients[h5 ^ mask];
		const Vec4f gradient_6 = new_gradients[h6 ^ mask];
		const Vec4f gradient_7 = new_gradients[h7 ^ mask];

		transpose(gradient_0, gradient_1, gradient_2, gradient_3, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum_weighted_dot =   (rel_offset_x * gradients_x + rel_offset_y * gradients_y + rel_offset_z  * gradients_z) * copyToAll<2>(one_uvw);
		transpose(gradient_4, gradient_5, gradient_6, gradient_7, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum_weighted_dot_2 = (rel_offset_x * gradients_x + rel_offset_y * gradients_y + rel_offset_z2 * gradients_z) * copyToAll<2>(uvw);
		const Vec4f sum = (sum_weighted_dot + sum_weighted_dot_2) * uv;
		res.x[2] = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
	}

	{
		const int mask = masks[3];
		const Vec4f gradient_0 = new_gradients[h0 ^ mask];
		const Vec4f gradient_1 = new_gradients[h1 ^ mask];
		const Vec4f gradient_2 = new_gradients[h2 ^ mask];
		const Vec4f gradient_3 = new_gradients[h3 ^ mask];
		const Vec4f gradient_4 = new_gradients[h4 ^ mask];
		const Vec4f gradient_5 = new_gradients[h5 ^ mask];
		const Vec4f gradient_6 = new_gradients[h6 ^ mask];
		const Vec4f gradient_7 = new_gradients[h7 ^ mask];

		transpose(gradient_0, gradient_1, gradient_2, gradient_3, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum_weighted_dot =   (rel_offset_x * gradients_x + rel_offset_y * gradients_y + rel_offset_z  * gradients_z) * copyToAll<2>(one_uvw);
		transpose(gradient_4, gradient_5, gradient_6, gradient_7, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum_weighted_dot_2 = (rel_offset_x * gradients_x + rel_offset_y * gradients_y + rel_offset_z2 * gradients_z) * copyToAll<2>(uvw);
		const Vec4f sum = (sum_weighted_dot + sum_weighted_dot_2) * uv;
		res.x[3] = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
	}

	return res;
}


/*
Fast Perlin noise implementation that returns a 4-vector, with each component decorrelated noise.
*/
template <bool use_sse4>
const Vec4f PerlinNoise::noise4ValuedImpl(float x, float y)
{
	const Vec4f point(x, y, 0, 0);
	Vec4f floored;
	if(use_sse4)
		floored = floor(point); // SSE 4 floor.
	else
		floored = Vec4f(std::floor(point.x[0]), std::floor(point.x[1]), 0, 0);

	// Get lowest 8 bits of integer lattice coordinates
	const Vec4i floored_i = toVec4i(floored) & Vec4i(0xFF, 0xFF, 0xFF, 0xFF);

	const int X = floored_i.x[0];
	const int Y = floored_i.x[1];
	
	const int hash_x  = p_x[X];
	const int hash_x1 = p_x[(X + 1) & 0xFF];
	const int hash_y  = p_y[Y];
	const int hash_y1 = p_y[(Y + 1) & 0xFF];

	const Vec4f fractional = point - floored;
	const Vec4f uvw = fade(fractional);

	const Vec4f fractional_x(fractional.x[0]);
	const Vec4f fractional_y(fractional.x[1]);

	const Vec4f u_scale(-1, 1, -1, 1), u_offset(1,0,1,0);
	const Vec4f v_scale(-1, -1, 1, 1), v_offset(1,1,0,0);

	const Vec4f u = copyToAll<0>(uvw); // (u, u, u, u)
	const Vec4f uvec = u * u_scale + u_offset; // (1 - u, u, 1 - u, u)

	const Vec4f v = copyToAll<1>(uvw); // (v, v, v, v)
	const Vec4f vvec = v * v_scale + v_offset; // (1 - v, 1 - v, v, v)

	const Vec4f uv = uvec * vvec;


	Vec4f res;
	const Vec4f offsets_x(0, 1, 0, 1);
	const Vec4f offsets_y(0, 0, 1, 1);
	const Vec4f rel_offset_x = fractional_x - offsets_x;
	const Vec4f rel_offset_y = fractional_y - offsets_y;

	const int h0 = hash_x  ^ hash_y ;
	const int h1 = hash_x1 ^ hash_y ;
	const int h2 = hash_x  ^ hash_y1;
	const int h3 = hash_x1 ^ hash_y1;

	Vec4f gradients_x, gradients_y, gradients_z, gradients_w;
	{
		const int mask = masks[0];
		const Vec4f gradient_0 = new_gradients[h0 ^ mask];
		const Vec4f gradient_1 = new_gradients[h1 ^ mask];
		const Vec4f gradient_2 = new_gradients[h2 ^ mask];
		const Vec4f gradient_3 = new_gradients[h3 ^ mask];

		transpose(gradient_0, gradient_1, gradient_2, gradient_3, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum = (rel_offset_x * gradients_x + rel_offset_y * gradients_y) * uv;
		res.x[0] = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
	}
	{
		const int mask = masks[1];
		const Vec4f gradient_0 = new_gradients[h0 ^ mask];
		const Vec4f gradient_1 = new_gradients[h1 ^ mask];
		const Vec4f gradient_2 = new_gradients[h2 ^ mask];
		const Vec4f gradient_3 = new_gradients[h3 ^ mask];

		transpose(gradient_0, gradient_1, gradient_2, gradient_3, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum = (rel_offset_x * gradients_x + rel_offset_y * gradients_y) * uv;
		res.x[1] = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
	}

	{
		const int mask = masks[2];
		const Vec4f gradient_0 = new_gradients[h0 ^ mask];
		const Vec4f gradient_1 = new_gradients[h1 ^ mask];
		const Vec4f gradient_2 = new_gradients[h2 ^ mask];
		const Vec4f gradient_3 = new_gradients[h3 ^ mask];

		transpose(gradient_0, gradient_1, gradient_2, gradient_3, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum = (rel_offset_x * gradients_x + rel_offset_y * gradients_y) * uv;
		res.x[2] = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
	}

	{
		const int mask = masks[3];
		const Vec4f gradient_0 = new_gradients[h0 ^ mask];
		const Vec4f gradient_1 = new_gradients[h1 ^ mask];
		const Vec4f gradient_2 = new_gradients[h2 ^ mask];
		const Vec4f gradient_3 = new_gradients[h3 ^ mask];

		transpose(gradient_0, gradient_1, gradient_2, gradient_3, gradients_x, gradients_y, gradients_z, gradients_w);
		const Vec4f sum = (rel_offset_x * gradients_x + rel_offset_y * gradients_y) * uv;
		res.x[3] = sum.x[0] + sum.x[1] + sum.x[2] + sum.x[3];
	}

	return res;
}


float PerlinNoise::FBM(const Vec4f& point, unsigned int num_octaves)
{
	float sum = 0;
	float scale = 1;
	float weight = 1;
	if(true) // PerlinNoise::have_sse4)
	{
		for(unsigned int i=0; i<num_octaves; ++i)
		{
			sum += weight * PerlinNoise::noiseImpl<true>(point * scale); // Use SSE 4 in PerlinNoise::noiseImpl()
			scale *= (float)1.99;
			weight *= (float)0.5;
		}
	}
	else
	{
		for(unsigned int i=0; i<num_octaves; ++i)
		{
			sum += weight * PerlinNoise::noiseImpl<false>(point * scale);
			scale *= (float)1.99;
			weight *= (float)0.5;
		}
	}

	return sum;
}


float PerlinNoise::FBM(float x, float y, unsigned int num_octaves)
{
	float sum = 0;
	float scale = 1;
	float weight = 1;
	if(true) // PerlinNoise::have_sse4)
	{
		for(unsigned int i=0; i<num_octaves; ++i)
		{
			sum += weight * PerlinNoise::noiseImpl<true>(x * scale, y * scale); // Use SSE 4 in PerlinNoise::noiseImpl()
			scale *= (float)1.99;
			weight *= (float)0.5;
		}
	}
	else
	{
		for(unsigned int i=0; i<num_octaves; ++i)
		{
			sum += weight * PerlinNoise::noiseImpl<false>(x * scale, y * scale);
			scale *= (float)1.99;
			weight *= (float)0.5;
		}
	}

	return sum;
}


const Vec4f PerlinNoise::FBM4Valued(const Vec4f& point, unsigned int num_octaves)
{
	Vec4f sum(0);
	float scale = 1;
	float weight = 1;
	if(true) // PerlinNoise::have_sse4)
	{
		for(unsigned int i=0; i<num_octaves; ++i)
		{
			sum += PerlinNoise::noise4ValuedImpl<true>(point * scale) * weight; // Use SSE 4 in PerlinNoise::noiseImpl()
			scale *= (float)1.99;
			weight *= (float)0.5;
		}
	}
	else
	{
		for(unsigned int i=0; i<num_octaves; ++i)
		{
			sum += PerlinNoise::noise4ValuedImpl<false>(point * scale) * weight;
			scale *= (float)1.99;
			weight *= (float)0.5;
		}
	}

	return sum;
}


const Vec4f PerlinNoise::FBM4Valued(float x, float y, unsigned int num_octaves)
{
	Vec4f sum(0);
	float scale = 1;
	float weight = 1;
	if(true) // PerlinNoise::have_sse4)
	{
		for(unsigned int i=0; i<num_octaves; ++i)
		{
			sum += PerlinNoise::noise4ValuedImpl<true>(x * scale, y * scale) * weight; // Use SSE 4 in PerlinNoise::noiseImpl()
			scale *= (float)1.99;
			weight *= (float)0.5;
		}
	}
	else
	{
		for(unsigned int i=0; i<num_octaves; ++i)
		{
			sum += PerlinNoise::noise4ValuedImpl<false>(x * scale, y * scale) * weight;
			scale *= (float)1.99;
			weight *= (float)0.5;
		}
	}

	return sum;
}


//================================== Noise basis functions =============================================


struct PerlinBasisNoise01
{
	inline float eval(const Vec4f& p)
	{
		return PerlinNoise::noise(p) * 0.5f + 0.5f;
	}
};


struct PerlinBasisNoise
{
	inline float eval(const Vec4f& p)
	{
		assert(p.x[3] == 1.f);
		return PerlinNoise::noise(p);
	}
};


struct RidgedBasisNoise01
{
	inline float eval(const Vec4f& p)
	{
		assert(p.x[3] == 1.f);
		return 1 - std::fabs(PerlinNoise::noise(p));
	}
};


struct RidgedBasisNoise
{
	inline float eval(const Vec4f& p)
	{
		assert(p.x[3] == 1.f);
		return 0.5f - std::fabs(PerlinNoise::noise(p));
	}
};


struct VoronoiBasisNoise01
{
	inline float eval(const Vec4f& p)
	{
		Vec4f closest_p;
		float dist;
		Voronoi::evaluate3d(p, 1.0f, closest_p, dist);
		return dist;
	}
};


//================================== New (more general) FBM =============================================


/*
Adapted from 'Texturing and Modelling, a Procedural Approach'
Page 437, fBm()

Computing the weight for each frequency, from the book:
w = f^-H
f = l^n
where n = octave, f = freq, l = lacunarity
so
w = (l^n)^-H
  = l^(-nH)
  = (l^-H)^n

*/


template <class Real, class BasisFunction>
Real genericFBM(const Vec4f& p, Real H, Real lacunarity, Real octaves, BasisFunction basis_func)
{
	Real sum = 0;
	Real freq = 1;
	Real weight = 1;
	Real w_factor = std::pow(lacunarity, -H);

	const Vec4f v = p - Vec4f(0,0,0,1);

	for(int i=0; i<(int)octaves; ++i)
	{
		sum += weight * basis_func.eval(Vec4f(0,0,0,1) + v * freq);
		freq *= lacunarity;
		weight *= w_factor;
	}

	// Do remaining octaves
	Real d = octaves - (int)octaves;
	if(d > 0)
		sum += d * weight * basis_func.eval(Vec4f(0,0,0,1) + v * freq);
	
	return sum;
}


template <class Real>
Real PerlinNoise::FBM2(const Vec4f& p, Real H, Real lacunarity, Real octaves)
{
	return genericFBM<Real, PerlinBasisNoise>(p, H, lacunarity, octaves, PerlinBasisNoise());
}


template <class Real>
Real PerlinNoise::ridgedFBM(const Vec4f& p, Real H, Real lacunarity, Real octaves)
{
	return genericFBM<Real, RidgedBasisNoise>(p, H, lacunarity, octaves, RidgedBasisNoise());
}


template <class Real>
Real PerlinNoise::voronoiFBM(const Vec4f& p, Real H, Real lacunarity, Real octaves)
{
	return genericFBM<Real, VoronoiBasisNoise01>(p, H, lacunarity, octaves, VoronoiBasisNoise01());
}


//================================== Multifractal =============================================


template <class Real, class BasisFunction>
Real genericMultifractal(const Vec4f& p, Real H, Real lacunarity, Real octaves, Real offset, BasisFunction basis_func)
{
	Real value = 0;
	Real freq = 1;
	Real weight = 1;
	Real w_factor = std::pow(lacunarity, -H);

	const Vec4f v = p - Vec4f(0,0,0,1);

	value += weight * (basis_func.eval(Vec4f(0,0,0,1) + v * freq) + offset);

	for(int i=1; i<(int)octaves; ++i)
	{
		value += myMax<Real>(0, value) * weight * (basis_func.eval(Vec4f(0,0,0,1) + v * freq) + offset);
		
		freq *= lacunarity;
		weight *= w_factor;
	}
	return value;
}


template <class Real>
Real PerlinNoise::multifractal(const Vec4f& p, Real H, Real lacunarity, Real octaves, Real offset)
{
	return genericMultifractal<Real, PerlinBasisNoise01>(p, H, lacunarity, octaves, offset, PerlinBasisNoise01());
}


template <class Real>
Real PerlinNoise::ridgedMultifractal(const Vec4f& p, Real H, Real lacunarity, Real octaves, Real offset)
{
	return genericMultifractal<Real, RidgedBasisNoise01>(p, H, lacunarity, octaves, offset, RidgedBasisNoise01());
}


template <class Real>
Real PerlinNoise::voronoiMultifractal(const Vec4f& p, Real H, Real lacunarity, Real octaves, Real offset)
{
	return genericMultifractal<Real, VoronoiBasisNoise01>(p, H, lacunarity, octaves, offset, VoronoiBasisNoise01());
}
