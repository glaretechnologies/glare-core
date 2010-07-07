/*=====================================================================
TriangleTest.cpp
----------------
File created by ClassTemplate on Thu May 07 13:27:06 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "TriangleTest.h"


#include "KDTree.h"
#include "jscol_BIHTree.h"
#include "BVH.h"
#include "../simpleraytracer/raymesh.h"
#include "../utils/MTwister.h"
#include "../raytracing/hitinfo.h"
#include "jscol_TriTreePerThreadData.h"
#include "../indigo/FullHitInfo.h"
#include "../indigo/DistanceHitInfo.h"
#include "../indigo/RendererSettings.h"
#include <algorithm>
#include "../simpleraytracer/csmodelloader.h"
#include "../simpleraytracer/raymesh.h"
#include "../utils/sphereunitvecpool.h"
#include "../utils/timer.h"
#include "../indigo/TestUtils.h"
#include "../utils/platformutils.h"
#include "../indigo/ThreadContext.h"
#include "../maths/SSE.h"
#include "MollerTrumboreTri.h"
#include "../indigo/globals.h"
#include "../utils/stringutils.h"
#include "../indigo/StandardPrintOutput.h"


namespace js
{


TriangleTest::TriangleTest()
{
	
}


TriangleTest::~TriangleTest()
{
	
}


static void testIntersection(const Ray& ray, const MollerTrumboreTri* tri)
{
	//const float min_t = 0.0f;
	//const float epsilon = ray.startPos().length() * TREE_EPSILON_FACTOR;
	const float epsilon = ray.origin_error;

	UnionVec4 u, v, t, hit;
	MollerTrumboreTri::intersectTris(&ray,
		//min_t,
		epsilon,
		tri[0].data, tri[1].data, tri[2].data, tri[3].data,
		&u, &v, &t, &hit
		);

	for(int i=0; i<4; ++i)
	{
		float ref_u, ref_v, ref_t;
		const bool ref_hit = tri[i].rayIntersect(ray, 1.0e9f, epsilon, ref_t, ref_u, ref_v) != 0;

		if(ref_hit || (hit.i[i] != 0))
		{
			testAssert(::epsEqual(ref_t, t.f[i]));
			testAssert(::epsEqual(ref_u, u.f[i]));
			testAssert(::epsEqual(ref_v, v.f[i]));
			testAssert(ref_hit == (hit.i[i] != 0));
		}
	}
}


static void testSingleTriIntersection(const Ray& ray, const MollerTrumboreTri& tri)
{
	MollerTrumboreTri tris[4];
	tris[0] = tris[1] = tris[2] = tris[3] = tri;


	testIntersection(ray, tris);
}


static void testTriangleIntersection()
{
	conPrint("testTriangleIntersection()");

	MTwister rng(1);

	MollerTrumboreTri tris[4];

	for(int i=0; i<100000; ++i)
	{
		for(int t=0; t<4; ++t)
			tris[t].set(
				Vec3f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()),
				Vec3f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()),
				Vec3f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom())
				);

		const Ray ray(
			Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1.f),
			normalise(Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 0.f))
#if USE_LAUNCH_NORMAL
			, normalise(Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 0.f))
#endif
			);

		testIntersection(ray, tris);
	}

	conPrint("testTriangleIntersection() Done.");
}


static void testIndividualBadouelIntersection(const js::BadouelTri& tri, const Ray& ray)
{
	const float ray_t_max = 1.0e9f;
	float ref_dist, ref_u, ref_v;
	const bool ref_hit = tri.referenceIntersect(ray, ray_t_max, ref_dist, ref_u, ref_v) != 0;

	float dist, u, v;
	const bool hit = tri.rayIntersect(ray, ray_t_max, dist, u, v) != 0;

	testAssert(hit == ref_hit);
	if(hit)
	{
		testAssert(::epsEqual(dist, ref_dist, 0.0001f));
		testAssert(::epsEqual(u, ref_u, 0.00005f));
		testAssert(::epsEqual(v, ref_v, 0.00005f));
	}
}


void testBadouelTriIntersection()
{
	conPrint("testBadouelTriIntersection()");

	MTwister rng(1);

	const int N = 1000000;

	for(int i=0; i<N; ++i)
	{
		js::BadouelTri tri;

		tri.set(
				Vec3f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()),
				Vec3f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()),
				Vec3f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom())
		);

		const Ray ray(
			Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(),1),
			normalise(Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(),0))
#if USE_LAUNCH_NORMAL
			, normalise(Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(),0))
#endif
			);

		testIndividualBadouelIntersection(tri, ray);
	}


	conPrint("testBadouelTriIntersection() Done");
}


void TriangleTest::doTests()
{


	// Test self intersection avoidance
	{
		MollerTrumboreTri t;
		t.set(Vec3f(0,0,0), Vec3f(0.5f,0,0), Vec3f(0,0.5f,0));

		//testAssert(epsEqual(t.getNormal(), Vec3f(0,0,1)));


		// Start ray just under tri, with same launch normal.  Shouldn't self intersect.
		{
		const Ray ray(
			Vec4f(0.1f, 0.1f, -0.00001f, 1.f), // startpos
			normalise(Vec4f(0.1f, 0.1f, 1.0f, 0.f)) // dir
#if USE_LAUNCH_NORMAL
			,Vec4f(0,0,1, 0.f) // launch normal
#endif
		);

		//const float epsilon = ray.startPos().length() * TREE_EPSILON_FACTOR;
		const float epsilon = ray.origin_error;

		float dist, u, v;
		const unsigned int hit = t.rayIntersect(ray, 10000.0f, epsilon, dist, u, v);
		testAssert(hit == 0);

		testSingleTriIntersection(ray, t);
		}

		// Start ray just under tri, with same different normal.  Should self intersect.
		/*{
		const Ray ray(
			Vec4f(0.1f, 0.1f, -0.00001f, 1.f), // startpos
			normalise(Vec4f(0.1f, 0.1f, 1.0f, 0.f)) // dir
#if USE_LAUNCH_NORMAL
			normalise(Vec4f(1,0,1, 0.f)) // launch normal
#endif
		);

		const float epsilon = ray.startPos().length() * TREE_EPSILON_FACTOR;

		float dist, u, v;
		const unsigned int hit = t.rayIntersect(ray, 10000.0f, epsilon, dist, u, v);
		testAssert(hit != 0);

		testSingleTriIntersection(ray, t);
		}*/

	}

	testTriangleIntersection();
	testBadouelTriIntersection();

}


}
