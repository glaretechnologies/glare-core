/*=====================================================================
TreeTest.cpp
------------
File created by ClassTemplate on Tue Jun 26 20:19:05 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "TreeTest.h"

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


namespace js
{




TreeTest::TreeTest()
{
	
}


TreeTest::~TreeTest()
{
	
}


// returns mask == 0xFFFFFF ? b : a
static inline __m128 condMov(__m128 a, __m128 b, __m128 mask)
{
	b = _mm_and_ps(b, mask);
	a = _mm_andnot_ps(mask, a);
	return _mm_or_ps(a, b);
}


template <int i>
static inline __m128 copyToAll(__m128 a)
{
	return _mm_shuffle_ps(a, a, _MM_SHUFFLE(i, i, i, i));
}


/*typedef union {
	__m128 v;
	float f[4];
} vec4;*/


void intersectTris(
	const float* orig_x_,
	const float* orig_y_,
	const float* orig_z_,
	const float* dir_x_,
	const float* dir_y_,
	const float* dir_z_,
	const float* v0x_, // [t3_v0x, t2_v0x, t1_v0x, t0_v0x]
	const float* v0y_, // [t3_v0y, t2_v0y, t1_v0y, t0_v0y]
	const float* v0z_,
	const float* edge1_x_,
	const float* edge1_y_,
	const float* edge1_z_,
	const float* edge2_x_,
	const float* edge2_y_,
	const float* edge2_z_,
	float* best_t_, // [t, t, t, t]
	unsigned int* best_tri_index_, // [t3_index, t2_index, t1_index, t0_index]
	float* best_u_,
	float* best_v_
	)
{
	const __m128 orig_x = _mm_load_ps(orig_x_);
	const __m128 orig_y = _mm_load_ps(orig_y_);
	const __m128 orig_z = _mm_load_ps(orig_z_);

	const __m128 dir_x = _mm_load_ps(dir_x_);
	const __m128 dir_y = _mm_load_ps(dir_y_);
	const __m128 dir_z = _mm_load_ps(dir_z_);

	const __m128 v0x = _mm_load_ps(v0x_);
	const __m128 v0y = _mm_load_ps(v0y_);
	const __m128 v0z = _mm_load_ps(v0z_);

	const __m128 edge1_x = _mm_load_ps(edge1_x_);
	const __m128 edge1_y = _mm_load_ps(edge1_y_);
	const __m128 edge1_z = _mm_load_ps(edge1_z_);

	const __m128 edge2_x = _mm_load_ps(edge2_x_);
	const __m128 edge2_y = _mm_load_ps(edge2_y_);
	const __m128 edge2_z = _mm_load_ps(edge2_z_);

	// edge1 = vert1 - vert0
	/*const __m128 edge1_x = _mm_sub_ps(v1x, v0x);
	const __m128 edge1_y = _mm_sub_ps(v1y, v0y);
	const __m128 edge1_z = _mm_sub_ps(v1z, v0z);

	// edge2 = vert2 - vert0
	const __m128 edge2_x = _mm_sub_ps(v2x, v0x);
	const __m128 edge2_y = _mm_sub_ps(v2y, v0y);
	const __m128 edge2_z = _mm_sub_ps(v2z, v0z);*/

	/* v1 x v2:
	(v1.y * v2.z) - (v1.z * v2.y),
	(v1.z * v2.x) - (v1.x * v2.z),
	(v1.x * v2.y) - (v1.y * v2.x)
	*/

	// pvec = cross(dir, edge2)
	const __m128 pvec_x = _mm_sub_ps(_mm_mul_ps(dir_y, edge2_z), _mm_mul_ps(dir_z, edge2_y));
	const __m128 pvec_y = _mm_sub_ps(_mm_mul_ps(dir_z, edge2_x), _mm_mul_ps(dir_x, edge2_z));
	const __m128 pvec_z = _mm_sub_ps(_mm_mul_ps(dir_x, edge2_y), _mm_mul_ps(dir_y, edge2_x));


	// det = dot(edge1, pvec)
	
	const __m128 det = _mm_add_ps(_mm_mul_ps(edge1_x, pvec_x), _mm_add_ps(_mm_mul_ps(edge1_y, pvec_y), _mm_mul_ps(edge1_z, pvec_z)));

	// TODO: det ~= 0 test

	const __m128 one = _mm_load_ps(one_4vec);

	const __m128 inv_det = _mm_div_ps(one, det);

	// tvec = orig - vert0
	const __m128 tvec_x = _mm_sub_ps(orig_x, v0x);
	const __m128 tvec_y = _mm_sub_ps(orig_y, v0y);
	const __m128 tvec_z = _mm_sub_ps(orig_z, v0z);

	// u = dot(tvec, pvec) * inv_det
	const __m128 u = _mm_mul_ps(
		_mm_add_ps(_mm_mul_ps(tvec_x, pvec_x), _mm_add_ps(_mm_mul_ps(tvec_y, pvec_y), _mm_mul_ps(tvec_z, pvec_z))),
		inv_det
		);

	// qvec = cross(tvec, edge1)
	const __m128 qvec_x = _mm_sub_ps(_mm_mul_ps(tvec_y, edge1_z), _mm_mul_ps(tvec_z, edge1_y));
	const __m128 qvec_y = _mm_sub_ps(_mm_mul_ps(tvec_z, edge1_x), _mm_mul_ps(tvec_x, edge1_z));
	const __m128 qvec_z = _mm_sub_ps(_mm_mul_ps(tvec_x, edge1_y), _mm_mul_ps(tvec_y, edge1_x));

	// v = dot(dir, qvec) * inv_det
	const __m128 v = _mm_mul_ps(
		_mm_add_ps(_mm_mul_ps(dir_x, qvec_x), _mm_add_ps(_mm_mul_ps(dir_y, qvec_y), _mm_mul_ps(dir_z, qvec_z))),
		inv_det
		);

	// t = dot(edge2, qvec) * inv_det
	const __m128 t = _mm_mul_ps(
		_mm_add_ps(_mm_mul_ps(edge2_x, qvec_x), _mm_add_ps(_mm_mul_ps(edge2_y, qvec_y), _mm_mul_ps(edge2_z, qvec_z))),
		inv_det
		);

	// if(u < 0.0 || u > 1.0) return 0
	// hit = (u >= 0.0 && u <= 1.0 && v >= 0.0 && u+v <= 1.0)
	const __m128 hit = _mm_and_ps(
		_mm_and_ps(_mm_cmpge_ps(u, zeroVec()), _mm_cmple_ps(u, one)),
		_mm_and_ps(_mm_cmpge_ps(v, zeroVec()), _mm_cmple_ps(_mm_add_ps(u, v), one))
		);

	__m128 best_t = _mm_load_ps(best_t_);
	const __m128 hit_and_closer = _mm_and_ps(hit, _mm_cmplt_ps(t, best_t));

	// [best_u, best_v, best_triindex, dummy]

	//__m128 best_t = _mm_load_ps(&best_t_);
	__m128 best_u = _mm_load_ps(best_u_);
	__m128 best_v = _mm_load_ps(best_v_);
	__m128 best_tri_index = _mm_load_ps((const float*)best_tri_index_);

	__m128 closer;
	closer = copyToAll<0>(hit_and_closer);
	best_t = condMov(best_t, copyToAll<0>(t), closer);
	best_u = condMov(best_u, copyToAll<0>(u), closer);
	best_v = condMov(best_v, copyToAll<0>(v), closer);
	best_tri_index = condMov(best_tri_index, copyToAll<0>(best_tri_index), closer);

	// Store back
	_mm_store_ps(best_t_, best_t);
	_mm_store_ps(best_u_, best_u);
	_mm_store_ps(best_v_, best_v);
	_mm_store_ps((float*)best_tri_index_, best_tri_index);
}	


static void testTriangleIntersection()
{



}








void TreeTest::testBuildCorrect()
{
	conPrint("TreeTest::testBuildCorrect()");

	ThreadContext thread_context(1, 0);

	{
	RayMesh raymesh("raymesh", false);
	raymesh.addMaterialUsed("dummy");
	
	const std::vector<Vec2f> texcoord_sets;

	const unsigned int uv_indices[] = {0, 0, 0};

	// Tri a
	{
	raymesh.addVertex(Vec3f(0,0,0));//, Vec3f(0,0,1)); // , texcoord_sets);
	raymesh.addVertex(Vec3f(5,1,0));//, Vec3f(0,0,1)); // , texcoord_sets);
	raymesh.addVertex(Vec3f(4,4,1));//, Vec3f(0,0,1)); // , texcoord_sets);
	const unsigned int vertex_indices[] = {0, 1, 2};
	raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}

	{
	// Tri b
	raymesh.addVertex(Vec3f(1,3,0));//, Vec3f(0,0,1)); // , texcoord_sets);
	raymesh.addVertex(Vec3f(8,12,0));//, Vec3f(0,0,1)); // , texcoord_sets);
	raymesh.addVertex(Vec3f(4,9,1));//, Vec3f(0,0,1)); // , texcoord_sets);
	const unsigned int vertex_indices[] = {3, 4, 5};
	raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}
	{
	// Tri C
	raymesh.addVertex(Vec3f(10,5,0));//, Vec3f(0,0,1)); // , texcoord_sets);
	raymesh.addVertex(Vec3f(10,10,0));//, Vec3f(0,0,1)); // , texcoord_sets);
	raymesh.addVertex(Vec3f(10,10,1));//, Vec3f(0,0,1)); // , texcoord_sets);
	const unsigned int vertex_indices[] = {6, 7, 8};
	raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}
	{
	// Tri D
	raymesh.addVertex(Vec3f(6,14,0));//, Vec3f(0,0,1)); // , texcoord_sets);
	raymesh.addVertex(Vec3f(12,14,0));//, Vec3f(0,0,1)); // , texcoord_sets);
	raymesh.addVertex(Vec3f(12,14,1));//, Vec3f(0,0,1)); // , texcoord_sets);
	const unsigned int vertex_indices[] = {9, 10, 11};
	raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}

	RendererSettings settings;
	settings.cache_trees = false;
	raymesh.build(
		".",
		settings
		);

	{
	const SSE_ALIGN Ray ray(Vec3d(0,-2,0), Vec3d(0,1,0));
	HitInfo hitinfo;
	js::ObjectTreePerThreadData tree_context(true);
	const double dist = raymesh.traceRay(ray, 1.0e20f, thread_context, tree_context, NULL, hitinfo);
	testAssert(::epsEqual(dist, 2.0));
	testAssert(hitinfo.sub_elem_index == 0);
	}

	{
	const SSE_ALIGN Ray ray(Vec3d(9,0,0), Vec3d(0,1,0));
	HitInfo hitinfo;
	js::ObjectTreePerThreadData tree_context(true);
	const double dist = raymesh.traceRay(ray, 1.0e20f, thread_context, tree_context, NULL, hitinfo);
	testAssert(::epsEqual(dist, 14.0));
	testAssert(hitinfo.sub_elem_index == 3);
	}




	}







	{
	RayMesh raymesh("raymesh", false);
	raymesh.addMaterialUsed("dummy");
	
	//const std::vector<Vec2f> texcoord_sets;
	const unsigned int uv_indices[] = {0, 0, 0};

	//x=0 tri
	{
	raymesh.addVertex(Vec3f(0,0,1));//, Vec3f(0,0,1));//, texcoord_sets);
	raymesh.addVertex(Vec3f(0,1,0));//, Vec3f(0,0,1));//, texcoord_sets);
	raymesh.addVertex(Vec3f(0,1,1));//, Vec3f(0,0,1));//, texcoord_sets);
	const unsigned int vertex_indices[] = {0, 1, 2};
	raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}

	{
	//x=1 tri
	raymesh.addVertex(Vec3f(1.f,0,1));//, Vec3f(0,0,1));//, texcoord_sets);
	raymesh.addVertex(Vec3f(1.f,1,0));//, Vec3f(0,0,1));//, texcoord_sets);
	raymesh.addVertex(Vec3f(1.f,1,1));//, Vec3f(0,0,1));//, texcoord_sets);
	const unsigned int vertex_indices[] = {3, 4, 5};
	raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}
	{
	//x=10 tri
	raymesh.addVertex(Vec3f(10.f,0,1));//, Vec3f(0,0,1));//, texcoord_sets);
	raymesh.addVertex(Vec3f(10.f,1,0));//, Vec3f(0,0,1));//, texcoord_sets);
	raymesh.addVertex(Vec3f(10.f,1,1));//, Vec3f(0,0,1));//, texcoord_sets);
	const unsigned int vertex_indices[] = {6, 7, 8};
	raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}

	RendererSettings settings;
	settings.cache_trees = false;
	raymesh.build(
		".",
		settings
		);


	raymesh.printTreeStats();
	//const js::TriTree* kdtree = dynamic_cast<const js::TriTree*>(raymesh.getTreeDebug());
	//testAssert(kdtree != NULL);

	const js::AABBox bbox_ws = raymesh.getAABBoxWS();

	testAssert(bbox_ws.min_ == Vec3f(0, 0, 0));
	testAssert(bbox_ws.max_ == Vec3f(10.f, 1, 1));

//	testAssert(kdtree->getNodesDebug().size() == 3);
/*	TEMP testAssert(!kdtree->getNodesDebug()[0].isLeafNode() != 0);
	testAssert(kdtree->getNodesDebug()[0].getPosChildIndex() == 2);
	testAssert(kdtree->getNodesDebug()[0].getSplittingAxis() == 0);
	testAssert(kdtree->getNodesDebug()[0].data2.dividing_val == 1.0f);

	testAssert(kdtree->getNodesDebug()[1].isLeafNode() != 0);
	testAssert(kdtree->getNodesDebug()[1].getLeafGeomIndex() == 0);
	testAssert(kdtree->getNodesDebug()[1].getNumLeafGeom() == 1);

	testAssert(kdtree->getNodesDebug()[2].isLeafNode() != 0);
	testAssert(kdtree->getNodesDebug()[2].getLeafGeomIndex() == 1);
	testAssert(kdtree->getNodesDebug()[2].getNumLeafGeom() == 2);*/

	}

	{
	RayMesh raymesh("raymesh", false);
	raymesh.addMaterialUsed("dummy");
	
	//const std::vector<Vec2f> texcoord_sets;
	const unsigned int uv_indices[] = {0, 0, 0};

	//x=0 tri
	{
	raymesh.addVertex(Vec3f(0,0,1));//, Vec3f(0,0,1));//, texcoord_sets);
	raymesh.addVertex(Vec3f(0,1,0));//, Vec3f(0,0,1));//, texcoord_sets);
	raymesh.addVertex(Vec3f(0,1,1));//, Vec3f(0,0,1));//, texcoord_sets);
	const unsigned int vertex_indices[] = {0, 1, 2};
	raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}

	{
	//x=1 tri
	raymesh.addVertex(Vec3f(1.f,0,1));//, Vec3f(0,0,1));//, texcoord_sets);
	raymesh.addVertex(Vec3f(1.f,1,0));//, Vec3f(0,0,1));//, texcoord_sets);
	raymesh.addVertex(Vec3f(1.f,1,1));//, Vec3f(0,0,1));//, texcoord_sets);
	const unsigned int vertex_indices[] = {3, 4, 5};
	raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}

	{
	//x=1 tri
	raymesh.addVertex(Vec3f(1.f,0,1));//, Vec3f(0,0,1));//, texcoord_sets);
	raymesh.addVertex(Vec3f(1.f,1,0));//, Vec3f(0,0,1));//, texcoord_sets);
	raymesh.addVertex(Vec3f(1.f,1,1));//, Vec3f(0,0,1));//, texcoord_sets);
	const unsigned int vertex_indices[] = {6, 7, 8};
	raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}

	{
	//x=10 tri
	raymesh.addVertex(Vec3f(10.f,0,1));//, Vec3f(0,0,1));//, texcoord_sets);
	raymesh.addVertex(Vec3f(10.f,1,0));//, Vec3f(0,0,1));//, texcoord_sets);
	raymesh.addVertex(Vec3f(10.f,1,1));//, Vec3f(0,0,1));//, texcoord_sets);
	const unsigned int vertex_indices[] = {9, 10, 11};
	raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}

	RendererSettings settings;
	settings.cache_trees = false;
	raymesh.build(
		".",
		settings
		);

	//const js::TriTree* kdtree = dynamic_cast<const js::TriTree*>(raymesh.getTreeDebug());
	//testAssert(kdtree != NULL);

	const js::AABBox bbox_ws = raymesh.getAABBoxWS();

	testAssert(bbox_ws.min_ == Vec3f(0, 0, 0));
	testAssert(bbox_ws.max_ == Vec3f(10.f, 1, 1));

	/* TEMP testAssert(kdtree->getNodesDebug().size() == 3);
	testAssert(!kdtree->getNodesDebug()[0].isLeafNode());
	testAssert(kdtree->getNodesDebug()[0].getPosChildIndex() == 2);
	testAssert(kdtree->getNodesDebug()[0].getSplittingAxis() == 0);
	testAssert(kdtree->getNodesDebug()[0].data2.dividing_val == 1.0f);

	testAssert(kdtree->getNodesDebug()[1].isLeafNode() != 0);
	testAssert(kdtree->getNodesDebug()[1].getLeafGeomIndex() == 0);
	testAssert(kdtree->getNodesDebug()[1].getNumLeafGeom() == 1);

	testAssert(kdtree->getNodesDebug()[2].isLeafNode() != 0);
	testAssert(kdtree->getNodesDebug()[2].getLeafGeomIndex() == 1);
	testAssert(kdtree->getNodesDebug()[2].getNumLeafGeom() == 3);*/

	}
}


static void testTree(MTwister& rng, RayMesh& raymesh)
{
	//------------------------------------------------------------------------
	//Init KD-tree and BIH
	//------------------------------------------------------------------------
	std::vector<Tree*> trees;
	trees.push_back(new KDTree(&raymesh));
	trees.back()->build();

	trees.push_back(new BIHTree(&raymesh));
	trees.back()->build();

	trees.push_back(new BVH(&raymesh));
	trees.back()->build();

	// Check AABBox
	SSE_ALIGN AABBox box = trees[0]->getAABBoxWS();
	for(unsigned int i=0; i<trees.size(); ++i)
		testAssert(trees[i]->getAABBoxWS() == box);

	ThreadContext thread_context(1, 0);

	//------------------------------------------------------------------------
	//compare tests against all tris with tests against the tree
	//------------------------------------------------------------------------
	const int NUM_RAYS = 1000;
	for(int i=0; i<NUM_RAYS; ++i)
	{
		//------------------------------------------------------------------------
		//test first hit traces
		//------------------------------------------------------------------------
		const double max_t = 1.0e9;

		const SSE_ALIGN Ray ray(
			Vec3d(-1.0 + rng.unitRandom()*2.0, -1.0 + rng.unitRandom()*2.0, -1.0 + rng.unitRandom()*2.0) * 1.5, 
			normalise(Vec3d(-1.0 + rng.unitRandom()*2.0, -1.0 + rng.unitRandom()*2.0, -1.0 + rng.unitRandom()*2.0))
			);

		HitInfo hitinfo, hitinfo2;
		js::TriTreePerThreadData tree_context;

		const double dist = trees[0]->traceRay(ray, max_t, thread_context, tree_context, NULL, hitinfo);
		
		const double alltrisdist = dynamic_cast<KDTree*>(trees[0])->traceRayAgainstAllTris(ray, max_t, hitinfo);
		testAssert(dist == alltrisdist);

		for(unsigned int t=0; t<trees.size(); ++t)
		{
			HitInfo hitinfo_, hitinfo2_;
			const double dist_ = trees[t]->traceRay(ray, max_t, thread_context, tree_context, NULL, hitinfo_);

			testAssert(dist == dist_);
			if(dist_ >= 0.0)
				testAssert(hitinfo == hitinfo_);
		}



		//------------------------------------------------------------------------
		//test getAllHits()
		//------------------------------------------------------------------------
		std::vector<DistanceHitInfo> hitinfos;

		trees[0]->getAllHits(ray, thread_context, tree_context, NULL, hitinfos);
		std::sort(hitinfos.begin(), hitinfos.end(), distanceHitInfoComparisonPred);

		if(dist > 0.0)
		{
			//if ray hit anything before
			testAssert(hitinfos.size() >= 1);
			testAssert(hitinfos[0].dist == dist);
			testAssert(hitinfos[0].sub_elem_index == hitinfo.sub_elem_index);
			testAssert(hitinfos[0].sub_elem_coords == hitinfo.sub_elem_coords);
		}

		// Do a check against all tris
		std::vector<DistanceHitInfo> hitinfos_d;
		dynamic_cast<KDTree*>(trees[0])->getAllHitsAllTris(ray, hitinfos_d);
		std::sort(hitinfos_d.begin(), hitinfos_d.end(), distanceHitInfoComparisonPred);

		// Compare results
		testAssert(hitinfos.size() == hitinfos_d.size());
		for(unsigned int z=0; z<hitinfos.size(); ++z)
		{
			testAssert(hitinfos[z].dist == hitinfos_d[z].dist);
			testAssert(hitinfos[z].sub_elem_index == hitinfos_d[z].sub_elem_index);
			testAssert(hitinfos[z].sub_elem_coords == hitinfos_d[z].sub_elem_coords);
		}

		//------------------------------------------------------------------------
		//test getAllHits() on BIH
		//------------------------------------------------------------------------
		for(unsigned int t=0; t<trees.size(); ++t)
		{

			std::vector<DistanceHitInfo> hitinfos_bih;
			trees[t]->getAllHits(ray, thread_context, tree_context, NULL, hitinfos_bih);
			std::sort(hitinfos_bih.begin(), hitinfos_bih.end(), distanceHitInfoComparisonPred);

			// Compare results
			testAssert(hitinfos.size() == hitinfos_bih.size());
			for(unsigned int z=0; z<hitinfos.size(); ++z)
			{
				testAssert(hitinfos[z].dist == hitinfos_bih[z].dist);
				testAssert(hitinfos[z].sub_elem_index == hitinfos_bih[z].sub_elem_index);
				testAssert(hitinfos[z].sub_elem_coords == hitinfos_bih[z].sub_elem_coords);
			}
		}

		//------------------------------------------------------------------------
		//Test doesFiniteRayHit()
		//------------------------------------------------------------------------
		const double testlength = rng.unitRandom() * 2.0;
		const bool hit = trees[0]->doesFiniteRayHit(ray, testlength, thread_context, tree_context, NULL);
		
		for(unsigned int t=0; t<trees.size(); ++t)
		{
			const bool hit_ = trees[t]->doesFiniteRayHit(ray, testlength, thread_context, tree_context, NULL);
			testAssert(hit == hit_);
		}

		/*
		bool bih_hit = bih_tree.doesFiniteRayHit(ray, testlength, thread_context, tree_context, NULL);
		testAssert(kd_hit == bih_hit);
		if(dist > 0.0)
		{
			testAssert(kd_hit == testlength >= dist);
		}
		else
		{
			testAssert(!kd_hit);
		}

		if(dist > 0.0)
		{
			//try with the testlength just on either side of the first hit dist
			kd_hit = tritree.doesFiniteRayHit(ray, dist + 0.0001, thread_context, tree_context, NULL);
			bih_hit = bih_tree.doesFiniteRayHit(ray, dist + 0.0001, thread_context, tree_context, NULL);
			testAssert(kd_hit == bih_hit);

			kd_hit = tritree.doesFiniteRayHit(ray, dist - 0.0001, thread_context, tree_context, NULL);
			bih_hit = bih_tree.doesFiniteRayHit(ray, dist - 0.0001, thread_context, tree_context, NULL);
			testAssert(kd_hit == bih_hit);
		}*/

	}
}








static void doEdgeCaseTests()
{
	conPrint("TreeTest::doEdgeCaseTests()");
#if 0 
	//TEMP reenable this


	{
	RayMesh raymesh("raymesh", false);
	raymesh.addMaterialUsed("dummy");
	
	const std::vector<Vec2f> texcoord_sets;

	//x=0 tri
	{ 
	raymesh.addVertex(Vec3f(0,0,1), Vec3f(0,0,1)); // , texcoord_sets);
	raymesh.addVertex(Vec3f(0,1,0), Vec3f(0,0,1)); // , texcoord_sets);
	raymesh.addVertex(Vec3f(0,1,1), Vec3f(0,0,1)); // , texcoord_sets);
	const unsigned int vertex_indices[] = {0, 1, 2};
	raymesh.addTriangle(vertex_indices, 0);
	}

	{
	//x=1 tri
	raymesh.addVertex(Vec3f(1.f,0,1), Vec3f(0,0,1)); // , texcoord_sets);
	raymesh.addVertex(Vec3f(1.f,1,0), Vec3f(0,0,1)); // , texcoord_sets);
	raymesh.addVertex(Vec3f(1.f,1,1), Vec3f(0,0,1)); // , texcoord_sets);
	const unsigned int vertex_indices[] = {3, 4, 5};
	raymesh.addTriangle(vertex_indices, 0);
	}
	{
	//x=10 tri
	raymesh.addVertex(Vec3f(10.f,0,1), Vec3f(0,0,1)); // , texcoord_sets);
	raymesh.addVertex(Vec3f(10.f,1,0), Vec3f(0,0,1)); // , texcoord_sets);
	raymesh.addVertex(Vec3f(10.f,1,1), Vec3f(0,0,1)); // , texcoord_sets);
	const unsigned int vertex_indices[] = {6, 7, 8};
	raymesh.addTriangle(vertex_indices, 0);
	}

	raymesh.build(
		".",
		false // use cached trees
		);

	//const js::TriTree* kdtree = dynamic_cast<const js::TriTree*>(raymesh.getTreeDebug());
	//testAssert(kdtree != NULL);

	const js::AABBox bbox_ws = raymesh.getAABBoxWS();

	testAssert(bbox_ws.min_ == Vec3f(0, 0, 0));
	testAssert(bbox_ws.max_ == Vec3f(10.f, 1, 1));

	/* TEMP testAssert(kdtree->getNodesDebug().size() == 3);
	testAssert(!kdtree->getNodesDebug()[0].isLeafNode() != 0);
	testAssert(kdtree->getNodesDebug()[0].getPosChildIndex() == 2);
	testAssert(kdtree->getNodesDebug()[0].getSplittingAxis() == 0);
	testAssert(kdtree->getNodesDebug()[0].data2.dividing_val == 1.0f);*/

	js::TriTreePerThreadData tree_context;

	const SSE_ALIGN Ray ray(Vec3d(1,0,-1), Vec3d(0,0,1));
	HitInfo hitinfo;
	//const double dist = kdtree->traceRay(ray, 1.0e20f, tree_context, NULL, hitinfo);
	}
#endif
}



static void cornellBoxTest()
{
	//cornellbox_jotero\cornellbox_jotero.3DS

}




void TreeTest::doTests()
{
	
	doEdgeCaseTests();

	conPrint("TreeTest::doTests()");

	MTwister rng(1);
	//------------------------------------------------------------------------
	//try building up a random set of triangles and inserting into a tree
	//------------------------------------------------------------------------
	{
	RayMesh raymesh("raymesh", false);
	raymesh.addMaterialUsed("dummy");
	
	const int NUM_TRIS = 1000;
	const std::vector<Vec2f> texcoord_sets;
	for(int i=0; i<NUM_TRIS; ++i)
	{
		const Vec3f pos(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f);

		raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f);//, Vec3f(0,0,1));
		raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f);//, Vec3f(0,0,1));
		raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f);//, Vec3f(0,0,1));
		const unsigned int vertex_indices[] = {i*3, i*3+1, i*3+2};
		const unsigned int uv_indices[] = {0, 0, 0};
		raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}

	testTree(rng, raymesh);
	}

	//------------------------------------------------------------------------
	//build a tree with lots of axis-aligned triangles - a trickier case
	//------------------------------------------------------------------------
	{
	RayMesh raymesh("raymesh", false);
	raymesh.addMaterialUsed("dummy");
	
	const int NUM_TRIS = 1000;
	const std::vector<Vec2f> texcoord_sets;
	for(int i=0; i<NUM_TRIS; ++i)
	{
		const unsigned int axis = rng.genrand_int32() % 3;
		const float axis_val = rng.unitRandom();

		const Vec3f pos(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f);

		Vec3f v;
		v = pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f;
		v[axis] = axis_val;
		raymesh.addVertex(v);//, Vec3f(0,0,1));
		
		v = pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f;
		v[axis] = axis_val;
		raymesh.addVertex(v);//, Vec3f(0,0,1));
		
		v = pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f;
		v[axis] = axis_val;
		raymesh.addVertex(v);//, Vec3f(0,0,1));

		const unsigned int vertex_indices[] = {i*3, i*3+1, i*3+2};
		const unsigned int uv_indices[] = {0, 0, 0};
		raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}

	testTree(rng, raymesh);
	}

	conPrint("TreeTest::doTests(): Done.");
}




void TreeTest::doSpeedTest()
{
	const std::string BUNNY_PATH = "c:\\programming\\models\\bunny\\reconstruction\\bun_zipper.ply";
	//const std::string BUNNY_PATH = "C:\\programming\\models\\ply\\happy_recon\\happy_vrip.ply";

	CSModelLoader model_loader;
	RayMesh raymesh("bunny", false);
	try
	{
		model_loader.streamModel(BUNNY_PATH, raymesh, 1.0);
	}
	catch(CSModelLoaderExcep& e)
	{
		::fatalError(e.what());
	}

	Timer buildtimer;

	RendererSettings settings;
	settings.cache_trees = false;
	settings.bih_tri_threshold = 100000000;
	raymesh.build(
		".", // base indigo dir path
		settings
		);

	conPrint("Build time: " + toString(buildtimer.getSecondsElapsed()) + " s");

	raymesh.printTreeStats();

	const Vec3d aabb_center(-0.016840, 0.110154, -0.001537);

	SphereUnitVecPool vecpool;//create pool of random points

	HitInfo hitinfo;
	//js::TriTreePerThreadData tree_context;
	js::ObjectTreePerThreadData context(true);
	ThreadContext thread_context(1, 0);

	conPrint("Running test...");

	Timer testtimer;//start timer
	int num_hits = 0;//number of rays that actually hit the model

	#ifdef DEBUG
	const int NUM_ITERS = 1000000;
	#else
	const int NUM_ITERS = 20000000;
	#endif
	for(int i=0; i<NUM_ITERS; ++i)
	{
		const double RADIUS = 0.2f;//radius of sphere

		///generate ray origin///
		const PoolVec3 pool_rayorigin = vecpool.getVec();//get random point on unit ball from pool

		Vec3d rayorigin(pool_rayorigin.x, pool_rayorigin.y, pool_rayorigin.z);//convert to usual 3-vector object
		rayorigin *= RADIUS;
		rayorigin += aabb_center;//aabb_center is (-0.016840, 0.110154, -0.001537)

		///generate other point on ray path///
		const PoolVec3 pool_rayend = vecpool.getVec();//get random point on unit ball from pool
			
		Vec3d rayend(pool_rayend.x, pool_rayend.y, pool_rayend.z);//convert to usual 3-vector object
		rayend *= RADIUS;
		rayend += aabb_center;

		//form the ray object
		if(rayend == rayorigin)
			continue;

		const SSE_ALIGN Ray ray(rayorigin, normalise(rayend - rayorigin));

		//do the trace
		//ray.buildRecipRayDir();
		const double dist = raymesh.traceRay(ray, 1.0e20f, thread_context, context, NULL, hitinfo);

		if(dist >= 0.0)//if hit the model
			num_hits++;//count the hit.
	}

	const double timetaken = testtimer.getSecondsElapsed();

	const double fraction_hit = (double)num_hits / (double)NUM_ITERS;

	const double traces_per_sec = (double)NUM_ITERS / timetaken;

	printVar(timetaken);
	printVar(fraction_hit);
	printVar(traces_per_sec);

	//raymesh.printTraceStats();
	const double clock_freq = 2.4e9;

	/*
	TEMP COMMENTED OUT WHILE DOING SUBDIVISION STUFFS
	const js::TriTree* kdtree = dynamic_cast<const js::TriTree*>(raymesh.getTreeDebug());

	{
	printVar(kdtree->num_traces);
	conPrint("AABB hit fraction: " + toString((double)kdtree->num_root_aabb_hits / (double)kdtree->num_traces));
	conPrint("av num nodes touched: " + toString((double)kdtree->total_num_nodes_touched / (double)kdtree->num_traces));
	conPrint("av num leaves touched: " + toString((double)kdtree->total_num_leafs_touched / (double)kdtree->num_traces));
	conPrint("av num tris considered: " + toString((double)kdtree->total_num_tris_considered / (double)kdtree->num_traces));
	conPrint("av num tris tested: " + toString((double)kdtree->total_num_tris_intersected / (double)kdtree->num_traces));
	const double cycles_per_trace = clock_freq * timetaken / (double)kdtree->num_traces;
	printVar(cycles_per_trace);
	}
	
	conPrint("Stats for rays that intersect root AABB:");
	conPrint("av num nodes touched: " + toString((double)kdtree->total_num_nodes_touched / (double)kdtree->num_root_aabb_hits));
	conPrint("av num leaves touched: " + toString((double)kdtree->total_num_leafs_touched / (double)kdtree->num_root_aabb_hits));
	conPrint("av num tris considered: " + toString((double)kdtree->total_num_tris_considered / (double)kdtree->num_root_aabb_hits));
	conPrint("av num tris tested: " + toString((double)kdtree->total_num_tris_intersected / (double)kdtree->num_root_aabb_hits));
	const double cycles_per_trace = clock_freq * timetaken / (double)kdtree->num_root_aabb_hits;
	printVar(cycles_per_trace);
	}
	*/
}



void TreeTest::buildSpeedTest()
{
	conPrint("TreeTest::buildSpeedTest()");

	CSModelLoader model_loader;
	RayMesh raymesh("raymesh", false);
	try
	{
		model_loader.streamModel("c:\\programming\\models\\ply\\happy_recon\\happy_vrip_res3.ply", raymesh, 1.0);
	}
	catch(CSModelLoaderExcep& e)
	{
		::fatalError(e.what());
	}

	Timer timer;
	RendererSettings settings;
	settings.cache_trees = false;
	raymesh.build(
		".", // base indigo dir path
		settings
		);

	printVar(timer.getSecondsElapsed());

	PlatformUtils::Sleep(10000);
}







void TreeTest::doRayTests()
{
	Ray ray(Vec3d(0,0,0), Vec3d(0,0,1));

	/*const float recip_x = ray.getRecipRayDirF().x;

	testAssert(!isInf(recip_x));
	testAssert(!isInf(ray.getRecipRayDirF().y));
	testAssert(!isInf(ray.getRecipRayDirF().z));*/

}


} //end namespace js






