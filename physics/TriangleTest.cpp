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
	//SSE_ALIGN Vec4 best_t = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
	//SSE_ALIGN Vec4 best_tri_index = {666, 666, 666, 666};
	/*SSE_ALIGN Vec4 tri_indices;// = { 0, 1, 2, 3 };
	tri_indices.i[0] = 0;
	tri_indices.i[1] = 1;
	tri_indices.i[2] = 2;
	tri_indices.i[3] = 3;*/
	//SSE_ALIGN Vec4 best_u = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };
	//SSE_ALIGN Vec4 best_v = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max() };

	//SSE_ALIGN Vec4 best_data;


	/*SSE_ALIGN Vec4 orig_x = {orig.x, orig.x, orig.x, orig.x};
	SSE_ALIGN Vec4 orig_y = {orig.y, orig.y, orig.y, orig.y};
	SSE_ALIGN Vec4 orig_z = {orig.z, orig.z, orig.z, orig.z};*/

	/*SSE_ALIGN Vec4 dir_x = {dir.x, dir.x, dir.x, dir.x};
	SSE_ALIGN Vec4 dir_y = {dir.y, dir.y, dir.y, dir.y};
	SSE_ALIGN Vec4 dir_z = {dir.z, dir.z, dir.z, dir.z};*/

	const float min_t = 0.0f;

	UnionVec4 u, v, t, hit;
	MollerTrumboreTri::intersectTris(&ray,
		min_t,
		/*&orig_x, &orig_y, &orig_z, &dir_x, &dir_y, &dir_z*/
		tri[0].data, tri[1].data, tri[2].data, tri[3].data,
		//&best_t,
		&u, &v, &t, &hit
		//&best_tri_index,
		//&tri_indices,

		//&best_u,
		//&best_v
		//&best_data

		);

	//float ref_u, ref_v;
	//unsigned int ref_best_index;
	//float ref_best_t = std::numeric_limits<float>::max();
	//bool hit_a_tri = false;

	for(int i=0; i<4; ++i)
	{
		float ref_u, ref_v, ref_t;
		const bool ref_hit = tri[i].referenceIntersect(ray, &ref_u, &ref_v, &ref_t);

		if(ref_hit || (hit.i[i] != 0))
		{
			testAssert(::epsEqual(ref_t, t.f[i]));
			testAssert(::epsEqual(ref_u, u.f[i]));
			testAssert(::epsEqual(ref_v, v.f[i]));
			testAssert(ref_hit == (hit.i[i] != 0));
		}

		/*if(tri[i].referenceIntersect(ray, &u, &v, &t))
		{
			hit_a_tri = true;

			if(t < ref_best_t)
			{
				ref_best_index = i;
				ref_best_t = t;
				ref_u = u;
				ref_v = v;
			}
		}*/
	}

	/*if(hit_a_tri)
	{
		const float best_u = best_data.f[0];
		const float best_v = best_data.f[1];
		const unsigned int best_index = best_data.i[2];

		testAssert(best_index == ref_best_index);
		testAssert(::epsEqual(ref_best_t, best_t.f[0]));
		testAssert(::epsEqual(ref_u, best_u));
		testAssert(::epsEqual(ref_v, best_v));
	}*/
}


static void testTriangleIntersection()
{
	conPrint("testTriangleIntersection()");

	MTwister rng(1);

	SSE_ALIGN MollerTrumboreTri tris[4];

	for(int i=0; i<100000; ++i)
	{
		for(int t=0; t<4; ++t)
			tris[t].set(
				Vec3f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()),
				Vec3f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()),
				Vec3f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom())
				);

		const SSE_ALIGN Ray ray(
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

		const SSE_ALIGN Ray ray(
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
	testTriangleIntersection();
	testBadouelTriIntersection();
}


}
