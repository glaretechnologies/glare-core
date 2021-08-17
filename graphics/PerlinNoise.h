/*=====================================================================
PerlinNoise.h
-------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "../maths/Vec4f.h"
#include "../utils/Platform.h"


/*=====================================================================
PerlinNoise
-----------
http://mrl.nyu.edu/~perlin/noise/

Some tests in NoiseTests::test()
=====================================================================*/
class PerlinNoise
{
public:
	//==================== Perlin noise ======================

	// Fast Perlin noise implementation using SSE, for 3-vector input, returns scalar value.
	static float noise(const Vec4f& point);

	// Fast Perlin noise implementation using SSE, for 2-vector input
	static float noise(float x, float y);

	static float periodicNoise(float x, float y, int period); // Period with period in x and y directions.  Period must be a power of two.  Period currently is clamped to <= 256.

	// Fast Perlin noise implementation that returns a 4-vector, with each component decorrelated noise.  3-vector input.
	static const Vec4f noise4Valued(const Vec4f& point);

	// Fast Perlin noise implementation that returns a 4-vector, with each component decorrelated noise.  2-vector input.
	static const Vec4f noise4Valued(float x, float y);


	//==================== FBM noise ======================

	// FBM with 3-vector input, single valued.
	static float FBM(const Vec4f& p, unsigned int num_octaves);

	// FBM with 2-vector input, single valued.
	static float FBM(float x, float y, unsigned int num_octaves);

	static float periodicFBM(float x, float y, unsigned int num_octaves, int period);

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


	//==================== Data building ======================

	static void buildData();
};
