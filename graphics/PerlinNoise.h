/*=====================================================================
PerlinNoise.h
-------------
File created by ClassTemplate on Fri Jun 08 01:50:46 2007
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "../maths/Vec4f.h"


/*=====================================================================
PerlinNoise
-----------

=====================================================================*/
class PerlinNoise
{
public:
	PerlinNoise();
	~PerlinNoise();

	template <class Real>
	static Real noise(Real x, Real y, Real z);

	template <class Real>
	static Real FBM(Real x, Real y, Real z, unsigned int num_octaves);

	template <class Real>
	static Real FBM2(const Vec4f& p, Real H, Real lacunarity, Real octaves);

	template <class Real>
	static Real ridgedFBM(const Vec4f& p, Real H, Real lacunarity, Real octaves);

	template <class Real>
	static Real multifractal(const Vec4f& p, Real H, Real lacunarity, Real octaves, Real offset);

	template <class Real>
	static Real ridgedMultifractal(const Vec4f& p, Real H, Real lacunarity, Real octaves, Real offset);

	static void init();

private:
	static int p[512];
};
