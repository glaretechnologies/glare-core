/*=====================================================================
GeometrySampling.cpp
--------------------
File created by ClassTemplate on Fri May 22 13:23:14 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "GeometrySampling.h"


#include "Matrix4f.h"
#include "../utils/MTwister.h"
#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"


// Explicit template instantiation
//template const Vec4f GeometrySampling::sampleHemisphereCosineWeighted(const Matrix4f& basis, const SamplePair& unitsamples);
//template const Vec2<float> GeometrySampling::sampleUnitDisc(const SamplePair& unitsamples);
//template const Vec2<double> GeometrySampling::sampleUnitDisc(const SamplePair& unitsamples);


namespace GeometrySampling
{


void doTests()
{
	// Check shirleyUnitSquareToDisk and inverse
	{
		MTwister rng(1);
		for(int i=0; i<1000000; ++i)
		{
			const Vec2f u(rng.unitRandom(), rng.unitRandom());
			const Vec2f p = shirleyUnitSquareToDisk<float>(u);
			testAssert(p.length() <= 1.0f);
			const Vec2f u_primed = shirleyDiskToUnitSquare<float>(p);
			testAssert(epsEqual(u, u_primed));
		}
	}
	
	// Check unitSquareToHemisphere and inverse
	{
		MTwister rng(1);
		for(int i=0; i<1000000; ++i)
		{
			const Vec2f u(rng.unitRandom(), rng.unitRandom());
			const Vec3f p = unitSquareToHemisphere<float>(u);
			testAssert(p.isUnitLength());
			testAssert(p.z >= 0.0);
			const Vec2f u_primed = hemisphereToUnitSquare<float>(p);
			testAssert(epsEqual(u, u_primed));
		}
	}

	// Check unitSquareToSphere and inverse
	{
			const Vec2f u(0.2f, 0.5f);
			const Vec3f p = unitSquareToSphere<float>(u);
			testAssert(p.isUnitLength());
			const Vec2f u_primed = sphereToUnitSquare<float>(p);

			printVar(u);
			printVar(u_primed);

			//testAssert(epsEqual(u, u_primed));
	}
	{
		MTwister rng(1);
		for(int i=0; i<1000000; ++i)
		{
			const Vec2f u(rng.unitRandom(), rng.unitRandom());
			const Vec3f p = unitSquareToSphere<float>(u);
			testAssert(p.isUnitLength());
			const Vec2f u_primed = sphereToUnitSquare<float>(p);

			if(!epsEqual(u, u_primed))
			{
				printVar(u);
				printVar(u_primed);
			}

			if(u.y != 0.5f)
			{
				testAssert(epsEqual(u, u_primed));
			}
		}


	}

	{
		const Vec4f d = sphericalToCartesianCoords((float)NICKMATHS_PI * (3.0f/2.0f), cos((float)NICKMATHS_PI_2), Matrix4f::identity());
		assert(epsEqual(d, Vec4f(0, -1, 0,0)));
	}

	testAssert(epsEqual(sphericalCoordsForDir(Vec3d(1,0,0), 1.0), Vec2d(0.0, NICKMATHS_PI_2)));
	testAssert(epsEqual(sphericalCoordsForDir(Vec3d(0,1,0), 1.0), Vec2d(NICKMATHS_PI_2, NICKMATHS_PI_2)));
	testAssert(epsEqual(sphericalCoordsForDir(Vec3d(0,-1,0), 1.0), Vec2d(-NICKMATHS_PI_2, NICKMATHS_PI_2)));
	testAssert(epsEqual(sphericalCoordsForDir(Vec3d(0,0,1), 1.0), Vec2d(0.0, 0.0)));
	testAssert(epsEqual(sphericalCoordsForDir(Vec3d(0,0,-1), 1.0), Vec2d(0.0, NICKMATHS_PI)));

	testAssert(epsEqual(dirForSphericalCoords(0.0, NICKMATHS_PI_2), Vec3d(1,0,0)));
	testAssert(epsEqual(dirForSphericalCoords(NICKMATHS_PI_2, NICKMATHS_PI_2), Vec3d(0,1,0)));
	testAssert(epsEqual(dirForSphericalCoords(-NICKMATHS_PI_2, NICKMATHS_PI_2), Vec3d(0,-1,0)));
	testAssert(epsEqual(dirForSphericalCoords(0.0, 0.0), Vec3d(0,0,1)));
	testAssert(epsEqual(dirForSphericalCoords(0.0, NICKMATHS_PI), Vec3d(0,0,-1)));

	SSE_ALIGN Matrix4f basis;
	const Vec4f v = normalise(Vec4f(1,1,1, 0.f));
	basis.constructFromVector(v);

	const Vec4f res = sampleSolidAngleCone(SamplePair(0.5, 0.5), basis, 0.01f);

	testAssert(dot(v, res) > 0.95);

}




} // end namespace GeometrySampling
