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
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../utils/MTwister.h"


namespace GeometrySampling
{


#if BUILD_TESTS



void doTests()
{
	
	// Performance tests
	if(false)
	{
		{
			MTwister rng(1);
			const int N = 100000;
			std::vector<Vec2f> unitr(N);
			for(int i=0; i<N; ++i)
				unitr[i] = Vec2f(rng.unitRandom(), rng.unitRandom());

			{
				conPrint("");
				conPrint("shirleyUnitSquareToDisk()");
				Timer t;
			
				Vec2f sum(0.f);
				for(int i=0; i<N; ++i)
				{
					Vec2f d = shirleyUnitSquareToDisk(unitr[i]);
					sum += d;
				}

				double elapsed = t.elapsed();
				conPrint("sum: " + sum.toString());
				conPrint("elapsed: " + toString(1.0e9 * elapsed / N) + " ns");
			}

			{
				conPrint("");
				conPrint("shirleyUnitSquareToDiskOld()");
				Timer t;
			
				Vec2f sum(0.f);
				for(int i=0; i<N; ++i)
				{
					Vec2f d = shirleyUnitSquareToDiskOld(unitr[i]);
					sum += d;
				}

				double elapsed = t.elapsed();
				conPrint("sum: " + sum.toString());
				conPrint("elapsed: " + toString(1.0e9 * elapsed / N) + " ns");
			}
		}

		conPrint("");
		conPrint("");
	}

	//=========================== sampleHemisphereCosineWeighted ===========================
	{
		MTwister rng(1);
		for(int i=0; i<1000000; ++i)
		{
			const Vec2f u(rng.unitRandom(), rng.unitRandom());
			
			const Vec4f n = normalise(Vec4f(1,1,1,0));
			Matrix4f m;
			m.constructFromVector(n);
			float p;
			const Vec4f dir = sampleHemisphereCosineWeighted<Vec4f>(m, u, p);

			testAssert(dir.isUnitLength());
			testAssert(dot(dir, n) > 0);
			testAssert(epsEqual(p, dot(dir, n) * Maths::recipPi<float>()));
			testAssert(epsEqual(p, hemisphereCosineWeightedPDF(n, dir)));
		}
	}


	//=========================== sampleBothHemispheresCosineWeighted ===========================
	{
		MTwister rng(1);
		for(int i=0; i<1000000; ++i)
		{
			const Vec2f u(rng.unitRandom(), rng.unitRandom());
			
			const Vec4f n = normalise(Vec4f(1,1,1,0));
			Matrix4f m;
			m.constructFromVector(n);
			float p;
			const Vec4f dir = sampleBothHemispheresCosineWeighted<Vec4f>(m, u, p);

			testAssert(dir.isUnitLength());
			testAssert(epsEqual(p, bothHemispheresCosineWeightedPDF(n, dir)));
			testAssert(epsEqual(p, ::absDot(dir, n) * Maths::recip2Pi<float>()));
		}
	}

	//=========================== Check shirleyUnitSquareToDisk and inverse ===========================
	{
		MTwister rng(1);
		for(int i=0; i<1000000; ++i)
		{
			const Vec2f u(rng.unitRandom(), rng.unitRandom());
			const Vec2f p = shirleyUnitSquareToDisk<float>(u);
			testAssert(p.length() <= 1.0f);

			const Vec2f u_primed = shirleyDiskToUnitSquare<float>(p);
			testAssert(epsEqual(u, u_primed));

			const Vec2f p_old = shirleyUnitSquareToDiskOld<float>(u);
			testAssert(epsEqual(p_old, p));
		}

		// Check (0,0) case
		{
			const Vec2f p = shirleyUnitSquareToDisk<float>(Vec2f(0.5f, 0.5f));
			testAssert(isFinite(p.x));
			testAssert(p.length() <= 1.0f);
			testAssert(epsEqual(p, p));

			const Vec2f p_old = shirleyUnitSquareToDiskOld<float>(Vec2f(0.5f, 0.5f));
			testAssert(isFinite(p_old.x));
			testAssert(p_old.length() <= 1.0f);
			testAssert(epsEqual(p_old, p));
		}
	}
	
	//=========================== Check unitSquareToHemisphere and inverse ===========================
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

	//=========================== Check unitSquareToSphere and inverse ===========================
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
		testAssert(epsEqual(d, Vec4f(0, -1, 0,0)));
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


#endif // BUILD_TESTS


} // end namespace GeometrySampling
