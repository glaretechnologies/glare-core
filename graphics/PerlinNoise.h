/*=====================================================================
PerlinNoise.h
-------------
File created by ClassTemplate on Fri Jun 08 01:50:46 2007
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "../maths/Vec4f.h"
#include "../utils/Platform.h"


/*=====================================================================
PerlinNoise
-----------

=====================================================================*/
class PerlinNoise
{
public:
	static void init();

	//==================== Perlin noise ======================

	template <class Real>
	static Real noiseRef(Real x, Real y, Real z);
	
	// Fast Perlin noise implementation using SSE, for 3-vector input
	static float noise(const Vec4f& point);

	// Fast Perlin noise implementation using SSE, for 2-vector input
	static float noise(float x, float y);

	// Fast Perlin noise implementation that returns a 4-vector, with each component decorrelated noise.  3-vector input.
	static const Vec4f noise4Valued(const Vec4f& point);

	// Fast Perlin noise implementation that returns a 4-vector, with each component decorrelated noise.  2-vector input.
	static const Vec4f noise4Valued(float x, float y);


	//==================== FBM noise ======================

	// FBM with 3-vector input, single valued.
	static float FBM(const Vec4f& p, unsigned int num_octaves);

	// FBM with 2-vector input, single valued.
	static float FBM(float x, float y, unsigned int num_octaves);

	// FBM with 3-vector input, 4-valued.
	static const Vec4f FBM4Valued(const Vec4f& p, unsigned int num_octaves);

	// FBM with 2-vector input, 4-valued.
	static const Vec4f FBM4Valued(float x, float y, unsigned int num_octaves);


	//==================== FBM for various basis functions ======================
	template <class Real>
	static Real FBM2(const Vec4f& p, Real H, Real lacunarity, Real octaves);

	template <class Real>
	static Real ridgedFBM(const Vec4f& p, Real H, Real lacunarity, Real octaves);

	template <class Real>
	static Real voronoiFBM(const Vec4f& p, Real H, Real lacunarity, Real octaves);

	//==================== Multifractals ======================
	template <class Real>
	static Real multifractal(const Vec4f& p, Real H, Real lacunarity, Real octaves, Real offset);

	template <class Real>
	static Real ridgedMultifractal(const Vec4f& p, Real H, Real lacunarity, Real octaves, Real offset);

	template <class Real>
	static Real voronoiMultifractal(const Vec4f& p, Real H, Real lacunarity, Real octaves, Real offset);

private:
	template <bool use_sse4>
	static float noiseImpl(const Vec4f& point);

	template <bool use_sse4>
	static float noiseImpl(float x, float y);

	template <bool use_sse4>
	static const Vec4f noise4ValuedImpl(const Vec4f& point);

	template <bool use_sse4>
	static const Vec4f noise4ValuedImpl(float x, float y);

	static uint8 p[512];
	static uint8 p_masked[512];
	static bool have_sse4;
};
