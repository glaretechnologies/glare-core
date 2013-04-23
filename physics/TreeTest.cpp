/*=====================================================================
TreeTest.cpp
------------
File created by ClassTemplate on Tue Jun 26 20:19:05 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "TreeTest.h"


#include "../indigo/EmbreeAccel.h"
#include "../indigo/EmbreeInstance.h"
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
#include "../utils/TaskManager.h"
#include "../indigo/StandardPrintOutput.h"
#include "../dll/include/IndigoMesh.h"



namespace js
{


#if BUILD_TESTS


void TreeTest::testBuildCorrect()
{
	conPrint("TreeTest::testBuildCorrect()");

	ThreadContext thread_context;
	Indigo::TaskManager task_manager;

	{
	RayMesh raymesh("raymesh", false);

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
	StandardPrintOutput print_output;
	raymesh.build(
		".",
		settings,
		print_output,
		true,
		task_manager
		);

	{
	const Ray ray(Vec4f(0,-2,0,1), Vec4f(0,1,0,0),
		1.0e-5f // min_t
#if USE_LAUNCH_NORMAL
		, Vec4f(0,1,0,0)
#endif
		);
	HitInfo hitinfo;
	const double dist = raymesh.traceRay(ray, 1.0e20f, thread_context, NULL, std::numeric_limits<unsigned int>::max(), hitinfo);
	testAssert(::epsEqual(dist, 2.0));
	testAssert(hitinfo.sub_elem_index == 0);
	}

	{
	const Ray ray(Vec4f(9,0,0,1), Vec4f(0,1,0,0),
		1.0e-5f // min_t
#if USE_LAUNCH_NORMAL
		, Vec4f(0,1,0,0)
#endif
	);
	HitInfo hitinfo;
	const double dist = raymesh.traceRay(ray, 1.0e20f, thread_context, NULL, std::numeric_limits<unsigned int>::max(), hitinfo);
	testAssert(::epsEqual(dist, 14.0));
	testAssert(hitinfo.sub_elem_index == 3);
	}




	}







	{
	RayMesh raymesh("raymesh", false);

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

	StandardPrintOutput print_output;
	RendererSettings settings;
	settings.cache_trees = false;
	raymesh.build(
		".",
		settings,
		print_output,
		true,
		task_manager
		);


	raymesh.printTreeStats();
	//const js::TriTree* kdtree = dynamic_cast<const js::TriTree*>(raymesh.getTreeDebug());
	//testAssert(kdtree != NULL);

	const js::AABBox bbox_ws = raymesh.getAABBoxWS();

	testAssert(bbox_ws.min_ == Vec4f(0, 0, 0, 1.0f));
	testAssert(bbox_ws.max_ == Vec4f(10.f, 1.0f, 1.0f, 1.0f));

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

	StandardPrintOutput print_output;
	RendererSettings settings;
	settings.cache_trees = false;
	raymesh.build(
		".",
		settings,
		print_output,
		true,
		task_manager
		);

	//const js::TriTree* kdtree = dynamic_cast<const js::TriTree*>(raymesh.getTreeDebug());
	//testAssert(kdtree != NULL);

	const js::AABBox bbox_ws = raymesh.getAABBoxWS();

	testAssert(bbox_ws.min_ == Vec4f(0, 0, 0, 1.0f));
	testAssert(bbox_ws.max_ == Vec4f(10.f, 1, 1, 1.0f));

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



static void testSelfIntersectionAvoidance()
{
	// We will construct a scene with two quads coplanar to the y-z plane, one at x=0, and the other at x=1.

	StandardPrintOutput print_output;
	Indigo::TaskManager task_manager;

	RayMesh raymesh("testmesh", false);
	const unsigned int uv_indices[] = {0, 0, 0};

	{
	raymesh.addVertex(Vec3f(0,0,0));
	raymesh.addVertex(Vec3f(0,1,0));
	raymesh.addVertex(Vec3f(0,1,1));
	const unsigned int vertex_indices[] = {0, 1, 2};
	raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}
	{
	raymesh.addVertex(Vec3f(0,0,0));
	raymesh.addVertex(Vec3f(0,1,1));
	raymesh.addVertex(Vec3f(0,0,1));
	const unsigned int vertex_indices[] = {3, 4, 5};
	raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}
	{
	raymesh.addVertex(Vec3f(1,0,0));
	raymesh.addVertex(Vec3f(1,1,0));
	raymesh.addVertex(Vec3f(1,1,1));
	const unsigned int vertex_indices[] = {6, 7, 8};
	raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}
	{
	raymesh.addVertex(Vec3f(1,0,0));
	raymesh.addVertex(Vec3f(1,1,1));
	raymesh.addVertex(Vec3f(1,0,1));
	const unsigned int vertex_indices[] = {9, 10, 11};
	raymesh.addTriangle(vertex_indices, uv_indices, 0);
	}

	//------------------------------------------------------------------------
	//Init KD-tree, BVH and Embree accel
	//------------------------------------------------------------------------
	std::vector<Tree*> trees;
	trees.push_back(new KDTree(&raymesh));
	trees.back()->build(print_output, true, task_manager);

	trees.push_back(new BVH(&raymesh));
	trees.back()->build(print_output, true, task_manager);

	trees.push_back(new EmbreeAccel(&raymesh, true));
	trees.back()->build(print_output, true, task_manager);

	// Check AABBox
	const AABBox box = trees[0]->getAABBoxWS();
	for(size_t i = 0; i < trees.size(); ++i)
		testAssert(trees[i]->getAABBoxWS() == box);

	ThreadContext thread_context;

	// Start a ray on one quad, trace to the other quad.
	{
		const Ray ray(Vec4f(0.0f, 0.25f, 0.1f, 1.0f), Vec4f(1.0f, 0.0f, 0.0f, 0.0f),
			1.0e-5f // min_t
#if USE_LAUNCH_NORMAL
			, Vec4f(1.0f, 0.0f, 0.0f, 0.0f)
#endif
		);

		for(size_t i = 0; i < trees.size(); ++i)
		{
			HitInfo hitinfo;
			const Tree::Real dist = (Tree::Real)trees[i]->traceRay(ray, 500.0f, thread_context, NULL, std::numeric_limits<unsigned int>::max(), hitinfo);

			testAssert(::epsEqual(dist, 1.0f));
			testAssert(hitinfo.sub_elem_index == 2);
		}

		const float nudge = 0.0001f;

		// Trace the same ray, but with a max distance of 1.0 - nudge.
		// So the ray shouldn't hit the other quad.
		for(size_t i = 0; i < trees.size(); ++i)
		{
			HitInfo hitinfo;
			const Tree::Real dist = (Tree::Real)trees[i]->traceRay(ray,
				1.0f - nudge, // max_t
				thread_context, NULL, std::numeric_limits<unsigned int>::max(), hitinfo);

			testAssert(dist < 0.0f);
		}

		// Trace the same ray, but with a max distance of 1.0 + nudge.
		// The ray *should* hit the other quad.
		for(size_t i = 0; i < trees.size(); ++i)
		{
			HitInfo hitinfo;
			const Tree::Real dist = (Tree::Real)trees[i]->traceRay(ray,
				1.0f + nudge, // max_t
				thread_context, NULL, std::numeric_limits<unsigned int>::max(), hitinfo);

			testAssert(::epsEqual(dist, 1.0f));
			testAssert(hitinfo.sub_elem_index == 2);
		}

		// Test doesFiniteRayHit
		for(size_t i = 0; i < trees.size(); ++i)
		{
			HitInfo hitinfo;
			const bool hit = trees[i]->doesFiniteRayHit(ray,
				1.0f - nudge, // max_t
				thread_context,
				NULL,
				std::numeric_limits<unsigned int>::max()
				);

			testAssert(!hit);
		}

		for(size_t i = 0; i < trees.size(); ++i)
		{
			HitInfo hitinfo;
			const bool hit = trees[i]->doesFiniteRayHit(ray,
				1.0f + nudge, // max_t
				thread_context,
				NULL,
				std::numeric_limits<unsigned int>::max()
				);

			testAssert(hit);
		}

		// Test doesFiniteRayHit, but setting ignore_tri = 2, so no intersection should be registered.
		for(size_t i = 0; i < trees.size(); ++i)
		{
			HitInfo hitinfo;
			const bool hit = trees[i]->doesFiniteRayHit(ray,
				1.0f + nudge, // max_t
				thread_context,
				NULL,
				2
				);

			testAssert(!hit);
		}

		// Test doesFiniteRayHit, but setting ignore_tri = 3 so an intersection *should* be registered.
		for(size_t i = 0; i < trees.size(); ++i)
		{
			HitInfo hitinfo;
			const bool hit = trees[i]->doesFiniteRayHit(ray,
				1.0f + nudge, // max_t
				thread_context,
				NULL,
				3
				);

			testAssert(hit);
		}
	}

	// Delete trees
	for(size_t i = 0; i < trees.size(); ++i)
		delete trees[i];
}


static void testTree(MTwister& rng, RayMesh& raymesh)
{
	StandardPrintOutput print_output;
	Indigo::TaskManager task_manager;

	//------------------------------------------------------------------------
	//Init KD-tree, BVH and Embree accel
	//------------------------------------------------------------------------
	std::vector<Tree*> trees;
	trees.push_back(new KDTree(&raymesh));
	trees.back()->build(print_output, true, task_manager);

	trees.push_back(new BVH(&raymesh));
	trees.back()->build(print_output, true, task_manager);

	// We want to test Embree, so let's require that the Embree DLL has been successfully loaded.
	testAssert(EmbreeInstance::isNonNull());

	trees.push_back(new EmbreeAccel(&raymesh, true));
	trees.back()->build(print_output, true, task_manager);

	// Check AABBox
	const AABBox box = trees[0]->getAABBoxWS();
	for(size_t i = 0; i < trees.size(); ++i)
		testAssert(trees[i]->getAABBoxWS() == box);

	ThreadContext thread_context;

	//------------------------------------------------------------------------
	//compare tests against all tris with tests against the trees
	//------------------------------------------------------------------------
	const int NUM_RAYS = 10000;
	for(int i=0; i<NUM_RAYS; ++i)
	{
		//------------------------------------------------------------------------
		//test first hit traces
		//------------------------------------------------------------------------
		const Tree::Real max_t = 1.0e9;

		const Ray ray(
			Vec4f(0,0,0,1.0f) + Vec4f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, 0) * 1.5f,
			normalise(Vec4f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f,0)),
			1.0e-5f // min_t
#if USE_LAUNCH_NORMAL
			, Vec4f(1.0f, 0.0f, 0.0f, 0.0f)
#endif
			);

		// Trace against all tris individually
		HitInfo all_tris_hitinfo;
		const Tree::Real alltrisdist = dynamic_cast<KDTree*>(trees[0])->traceRayAgainstAllTris(ray, max_t, all_tris_hitinfo);

		// Trace through the trees
		for(size_t t = 0; t < trees.size(); ++t)
		{
			HitInfo hitinfo;
			unsigned int ignore_tri = std::numeric_limits<unsigned int>::max();
			const Tree::Real dist = (Tree::Real)trees[t]->traceRay(ray, max_t, thread_context, NULL, ignore_tri, hitinfo);

			if(dist >= 0.0 || alltrisdist >= 0.0) // If either ray hit
			{
				testAssert(dist >= 0.0 && alltrisdist >= 0.0);
				testAssert(hitinfo.sub_elem_index == all_tris_hitinfo.sub_elem_index);//NOTE: FAILING on Linux 64 with release mode under valgrind, but only
				// if this test is run in the middle of the testsuite.  Running at the start works fine.
				testAssert(::epsEqual(dist, alltrisdist, (Tree::Real)0.0001));

				testAssert(::epsEqual(hitinfo.sub_elem_coords.x, all_tris_hitinfo.sub_elem_coords.x, (HitInfo::SubElemCoordsRealType)0.0001));
				testAssert(::epsEqual(hitinfo.sub_elem_coords.y, all_tris_hitinfo.sub_elem_coords.y, (HitInfo::SubElemCoordsRealType)0.0001));
			}
		}



		//------------------------------------------------------------------------
		//test getAllHits()
		//------------------------------------------------------------------------
		std::vector<DistanceHitInfo> hitinfos;

		trees[0]->getAllHits(ray, thread_context, NULL, hitinfos);
		std::sort(hitinfos.begin(), hitinfos.end(), distanceHitInfoComparisonPred);

		if(alltrisdist > 0.0)
		{
			//if ray hit anything before
			testAssert(hitinfos.size() >= 1);
			testAssert(::epsEqual(hitinfos[0].dist, alltrisdist));
			testAssert(hitinfos[0].sub_elem_index == all_tris_hitinfo.sub_elem_index);
			testAssert(epsEqual(hitinfos[0].sub_elem_coords, all_tris_hitinfo.sub_elem_coords));
		}

		// Do a check against all tris
		std::vector<DistanceHitInfo> hitinfos_d;
		dynamic_cast<KDTree*>(trees[0])->getAllHitsAllTris(ray, hitinfos_d);
		std::sort(hitinfos_d.begin(), hitinfos_d.end(), distanceHitInfoComparisonPred); // Sort hits

		// Compare results
		testAssert(hitinfos.size() == hitinfos_d.size());
		for(size_t z = 0; z < hitinfos.size(); ++z)
		{
			testAssert(::epsEqual(hitinfos[z].dist, hitinfos_d[z].dist));
			testAssert(hitinfos[z].sub_elem_index == hitinfos_d[z].sub_elem_index);
			testAssert(::epsEqual(hitinfos[z].sub_elem_coords, hitinfos_d[z].sub_elem_coords));
		}

		//------------------------------------------------------------------------
		//Test getAllHits() on other trees
		//------------------------------------------------------------------------
		for(size_t t = 0; t < trees.size(); ++t)
		{
			std::vector<DistanceHitInfo> hitinfos_other;
			trees[t]->getAllHits(ray, thread_context/*, tree_context*/, NULL, hitinfos_other);
			std::sort(hitinfos_other.begin(), hitinfos_other.end(), distanceHitInfoComparisonPred); // Sort hits

			// Compare results
			testAssert(hitinfos.size() == hitinfos_other.size());
			for(size_t z = 0; z < hitinfos.size(); ++z)
			{
				testAssert(::epsEqual(hitinfos[z].dist, hitinfos_other[z].dist, 0.0001f));
				testAssert(hitinfos[z].sub_elem_index == hitinfos_other[z].sub_elem_index);
				//testAssert(::epsEqual(hitinfos[z].sub_elem_coords, hitinfos_other[z].sub_elem_coords));
				testAssert(::epsEqual(hitinfos[z].sub_elem_coords.x, hitinfos_other[z].sub_elem_coords.x, (HitInfo::SubElemCoordsRealType)0.001));
				testAssert(::epsEqual(hitinfos[z].sub_elem_coords.y, hitinfos_other[z].sub_elem_coords.y, (HitInfo::SubElemCoordsRealType)0.001));
			}
		}

		//------------------------------------------------------------------------
		//Test doesFiniteRayHit()
		//------------------------------------------------------------------------
		const Tree::Real testlength = rng.unitRandom() * (Tree::Real)2.0;
		const bool hit = trees[0]->doesFiniteRayHit(ray, testlength, thread_context, NULL, std::numeric_limits<unsigned int>::max());

		for(size_t t = 0; t < trees.size(); ++t)
		{
			const bool hit_ = trees[t]->doesFiniteRayHit(ray, testlength, thread_context, NULL, std::numeric_limits<unsigned int>::max());
			testAssert(hit == hit_);
		}
	} // End for each ray

	//------------------------------------------------------------------------
	// Test traceRay() with a ray with NaN components
	// We just want to make sure the trace methods don't crash.  (Embree was crashing on Mac with NaN rays)
	//------------------------------------------------------------------------
	const float NaN = std::numeric_limits<float>::quiet_NaN();
	const float inf = std::numeric_limits<float>::infinity();
	const float max_t = 100000.0f;
	for(size_t t = 0; t < trees.size(); ++t)
	{
		// Try with NaNs in the direction vector
		{
		Ray ray(
			Vec4f(0, 0, 0, 1.0f), 
			Vec4f(NaN, NaN, NaN, 0),
			1.0e-05f // t_min
		);
		HitInfo hitinfo;
		const Tree::Real dist = (Tree::Real)trees[t]->traceRay(ray, max_t, thread_context, NULL, std::numeric_limits<unsigned int>::max(), hitinfo);
		}
		// Try with NaNs in the position vector
		{
		Ray ray(
			Vec4f(NaN, NaN, NaN, 1.0f), 
			Vec4f(0, 0, 0, 0),
			1.0e-05f // t_min
		);
		HitInfo hitinfo;
		const Tree::Real dist = (Tree::Real)trees[t]->traceRay(ray, max_t, thread_context, NULL, std::numeric_limits<unsigned int>::max(), hitinfo);
		}

		

		// Try with Infs in the direction vector
		{
		Ray ray(
			Vec4f(0, 0, 0, 1.0f), 
			Vec4f(inf, inf, inf, 0),
			1.0e-05f // t_min
		);
		HitInfo hitinfo;
		const Tree::Real dist = (Tree::Real)trees[t]->traceRay(ray, max_t, thread_context, NULL, std::numeric_limits<unsigned int>::max(), hitinfo);
		}
		// Try with Infs in the position vector
		{
		Ray ray(
			Vec4f(inf, inf, inf, 1.0f), 
			Vec4f(0, 0, 0, 0),
			1.0e-05f // t_min
		);
		HitInfo hitinfo;
		const Tree::Real dist = (Tree::Real)trees[t]->traceRay(ray, max_t, thread_context, NULL, std::numeric_limits<unsigned int>::max(), hitinfo);
		}
	}

	//------------------------------------------------------------------------
	//Test doesFiniteRayHit() with a ray with NaN components
	//------------------------------------------------------------------------
	for(size_t t = 0; t < trees.size(); ++t)
	{
		// Try with NaNs in the direction vector
		{
		Ray ray(
			Vec4f(0, 0, 0, 1.0f), 
			Vec4f(NaN, NaN, NaN, 0),
			1.0e-05f // t_min
		);
		HitInfo hitinfo;
		trees[t]->doesFiniteRayHit(ray, 1000.0f, thread_context, NULL, std::numeric_limits<unsigned int>::max());
		}
		// Try with NaNs in the position vector
		{
		Ray ray(
			Vec4f(NaN, NaN, NaN, 1.0f), 
			Vec4f(0, 0, 0, 0),
			1.0e-05f // t_min
		);
		HitInfo hitinfo;
		trees[t]->doesFiniteRayHit(ray, 1000.0f, thread_context, NULL, std::numeric_limits<unsigned int>::max());
		}
	}

	//------------------------------------------------------------------------
	// Test getAllHits() with a ray with NaN components
	//------------------------------------------------------------------------
	for(size_t t = 0; t < trees.size(); ++t)
	{
		// Try with NaNs in the direction vector
		{
		Ray ray(
			Vec4f(0, 0, 0, 1.0f), 
			Vec4f(NaN, NaN, NaN, 0),
			1.0e-05f // t_min
		);
		std::vector<DistanceHitInfo> hitinfos;
		trees[t]->getAllHits(ray, thread_context, NULL, hitinfos);
		}
		// Try with NaNs in the position vector
		{
		Ray ray(
			Vec4f(NaN, NaN, NaN, 1.0f), 
			Vec4f(0, 0, 0, 0),
			1.0e-05f // t_min
		);
		std::vector<DistanceHitInfo> hitinfos;
		trees[t]->getAllHits(ray, thread_context, NULL, hitinfos);
		}
	}

	// Delete trees
	for(size_t i = 0; i < trees.size(); ++i)
		delete trees[i];
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


void TreeTest::doTests(const std::string& appdata_path)
{
//	doVaryingNumtrisBuildTests();

	//testSelfIntersectionAvoidance();

	doEdgeCaseTests();

	conPrint("TreeTest::doTests()");

	RendererSettings settings;
	settings.cache_trees = false;
	StandardPrintOutput print_output;
	Indigo::TaskManager task_manager;
	MTwister rng(1);

	///////////////////////////////////////
	/*
	This mesh is too big to use in this way during the normal course of testing :(

	{
	// Load tricky mesh from disk
	const std::string MODEL_PATH = "../testfiles/ring_kdbug_scene/models.ringe/ringe-2.igmesh";
	CSModelLoader model_loader;
	RayMesh raymesh("ring", false);
	try
	{
		model_loader.streamModel(MODEL_PATH, raymesh, 1.0);
	}
	catch(CSModelLoaderExcep&)
	{
		testAssert(false);
	}
	testTree(rng, raymesh);
	}*/
	/////////////////////////////////////////////

	///////////////////////////////////////
	{
	// Load tricky mesh from disk
	const std::string MODEL_PATH = TestUtils::getIndigoTestReposDir() + "/testfiles/bug-2.igmesh";
	CSModelLoader model_loader;
	RayMesh raymesh("tricky", false);
	Indigo::Mesh indigoMesh;
	try
	{
		model_loader.streamModel(MODEL_PATH, indigoMesh, 1.0);
		raymesh.fromIndigoMesh(indigoMesh);

		raymesh.build(appdata_path, settings, print_output, false, task_manager);
	}
	catch(CSModelLoaderExcep&)
	{
		testAssert(false);
	}
	testTree(rng, raymesh);
	}
	/////////////////////////////////////////////





	//------------------------------------------------------------------------
	//try building up a random set of triangles and inserting into a tree
	//------------------------------------------------------------------------
	{
	RayMesh raymesh("raymesh", false);

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

	raymesh.build(appdata_path, settings, print_output, false, task_manager);

	testTree(rng, raymesh);
	}

	//------------------------------------------------------------------------
	//build a tree with lots of axis-aligned triangles - a trickier case
	//------------------------------------------------------------------------
	{
	RayMesh raymesh("raymesh", false);

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

	raymesh.build(appdata_path, settings, print_output, false, task_manager);

	testTree(rng, raymesh);
	}

	conPrint("TreeTest::doTests(): Done.");
}


void TreeTest::doVaryingNumtrisBuildTests()
{
	MTwister rng(1);
	Indigo::TaskManager task_manager;

	int num_tris = 1;
	for(int i=0; i<21; ++i)
	{
		//------------------------------------------------------------------------
		//Build up a random set of triangles and inserting into a tree
		//------------------------------------------------------------------------
		RayMesh raymesh("raymesh", false);

		const std::vector<Vec2f> texcoord_sets;
		for(int t=0; t<num_tris; ++t)
		{
			const Vec3f pos(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f);

			raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f);//, Vec3f(0,0,1));
			raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f);//, Vec3f(0,0,1));
			raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f);//, Vec3f(0,0,1));
			const unsigned int vertex_indices[] = {t*3, t*3+1, t*3+2};
			const unsigned int uv_indices[] = {0, 0, 0};
			raymesh.addTriangle(vertex_indices, uv_indices, 0);
		}

		Timer timer;
		StandardPrintOutput print_output;
		RendererSettings settings;
		settings.bih_tri_threshold = 10000000;
		settings.cache_trees = false;
		raymesh.build(
			".", // appdata path
			settings,
			print_output,
			false, // verbose
			task_manager
		);

		const double elapsed = timer.elapsed();

		conPrint("Kdtree with " + toString(num_tris) + " tris built in " + toString(elapsed) + " s");

		num_tris *= 2;
	}
	
	exit(666);//TEMP
}


// Aka. the 'Bunnybench' :)
void TreeTest::doSpeedTest(int treetype)
{
	const std::string BUNNY_PATH = TestUtils::getIndigoTestReposDir() + "/testfiles/bun_zipper.ply";

	CSModelLoader model_loader;
	RayMesh raymesh("bunny", false);
	Indigo::Mesh indigoMesh;
	try
	{
		model_loader.streamModel(BUNNY_PATH, indigoMesh, 1.0);
		raymesh.fromIndigoMesh(indigoMesh);
	}
	catch(CSModelLoaderExcep&)
	{
		testAssert(false);
	}

	StandardPrintOutput print_output;
	Indigo::TaskManager task_manager;


	Timer buildtimer;

	RendererSettings settings;
	settings.cache_trees = false;
	if(treetype == 0)
		settings.bih_tri_threshold = 100000000;
	else
		settings.bih_tri_threshold = 0;

	
	raymesh.build(
		".", // base indigo dir path
		settings,
		print_output,
		true,
		task_manager
		);

	conPrint("Build time: " + toString(buildtimer.getSecondsElapsed()) + " s");

	raymesh.printTreeStats();

	const Vec3d aabb_center(-0.016840, 0.110154, -0.001537);

	SphereUnitVecPool vecpool;//create pool of random points

	HitInfo hitinfo;
	ThreadContext thread_context;

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

		Vec4f rayorigin_f, dir_f;
		rayorigin.pointToVec4f(rayorigin_f);
		normalise(rayend - rayorigin).vectorToVec4f(dir_f);

		const Ray ray(rayorigin_f, dir_f,
			1.0e-5f // min_t
#if USE_LAUNCH_NORMAL
			, dir_f
#endif
		);

		//do the trace
		//ray.buildRecipRayDir();
		const double dist = raymesh.traceRay(ray, 1.0e20f, thread_context, NULL, std::numeric_limits<unsigned int>::max(), hitinfo);

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
	//const double clock_freq = 2.4e9;

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

	Indigo::TaskManager task_manager;


	CSModelLoader model_loader;
	RayMesh raymesh("raymesh", false);
	Indigo::Mesh indigoMesh;
	try
	{
		model_loader.streamModel("c:\\programming\\models\\ply\\happy_recon\\happy_vrip_res3.ply", indigoMesh, 1.0);
		raymesh.fromIndigoMesh(indigoMesh);
	}
	catch(CSModelLoaderExcep&)
	{
		testAssert(false);
	}

	Timer timer;
	RendererSettings settings;
	settings.cache_trees = false;
	StandardPrintOutput print_output;
	raymesh.build(
		".", // base indigo dir path
		settings,
		print_output,
		true,
		task_manager
		);

	printVar(timer.getSecondsElapsed());
}







void TreeTest::doRayTests()
{
	//Ray ray(Vec3d(0,0,0), Vec3d(0,0,1));

	/*const float recip_x = ray.getRecipRayDirF().x;

	testAssert(!isInf(recip_x));
	testAssert(!isInf(ray.getRecipRayDirF().y));
	testAssert(!isInf(ray.getRecipRayDirF().z));*/

}
#endif
	

} //end namespace js






