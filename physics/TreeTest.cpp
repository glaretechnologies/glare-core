/*=====================================================================
TreeTest.cpp
------------
File created by ClassTemplate on Tue Jun 26 20:19:05 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "TreeTest.h"


#ifndef NO_EMBREE
#include "../indigo/EmbreeAccel.h"
#endif
#include "BVH.h"
#include "MollerTrumboreTri.h"
#include "jscol_boundingsphere.h"
#include "../simpleraytracer/raymesh.h"
#include "../maths/PCG32.h"
#include "../raytracing/hitinfo.h"
#include "../indigo/FullHitInfo.h"
#include "../indigo/DistanceHitInfo.h"
#include "../indigo/RendererSettings.h"
#include "../simpleraytracer/raymesh.h"
#include "../utils/ShouldCancelCallback.h"
#include "../utils/Timer.h"
#include "../utils/TestUtils.h"
#include "../indigo/MeshLoader.h"
#include "../utils/Exception.h"
#include "../utils/PlatformUtils.h"
#include "../utils/MemAlloc.h"
#include "../indigo/globals.h"
#include "../utils/StringUtils.h"
#include "../utils/TaskManager.h"
#include "../utils/StandardPrintOutput.h"
#include "../dll/include/IndigoException.h"
#include "../dll/include/IndigoMesh.h"
#include "../dll/IndigoStringUtils.h"
#include <algorithm>


namespace js
{


#if BUILD_TESTS


void TreeTest::testBuildCorrect()
{
	conPrint("TreeTest::testBuildCorrect()");

	glare::TaskManager task_manager;

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

	Geometry::BuildOptions options;
	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;
	raymesh.build(
		options,
		should_cancel_callback,
		print_output,
		true,
		task_manager
		);

	{
	const Ray ray(Vec4f(0,-2,0,1), Vec4f(0,1,0,0),
		1.0e-5f, // min_t
		std::numeric_limits<float>::max() // max_t
	);
	HitInfo hitinfo;
	const double dist = raymesh.traceRay(ray, hitinfo);
	testAssert(::epsEqual(dist, 2.0));
	testAssert(hitinfo.sub_elem_index == 0);
	}

	{
	const Ray ray(Vec4f(9,0,0,1), Vec4f(0,1,0,0),
		1.0e-5f, // min_t
		std::numeric_limits<float>::max() // max_t
	);
	HitInfo hitinfo;
	const double dist = raymesh.traceRay(ray, hitinfo);
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
	DummyShouldCancelCallback should_cancel_callback;
	Geometry::BuildOptions options;
	raymesh.build(
		options,
		should_cancel_callback,
		print_output,
		true,
		task_manager
		);


	raymesh.printTreeStats();
	//const js::TriTree* kdtree = dynamic_cast<const js::TriTree*>(raymesh.getTreeDebug());
	//testAssert(kdtree != NULL);

	const js::AABBox bbox_ws = raymesh.getAABBox();

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
	DummyShouldCancelCallback should_cancel_callback;
	Geometry::BuildOptions options;
	raymesh.build(
		options,
		should_cancel_callback,
		print_output,
		true,
		task_manager
		);

	//const js::TriTree* kdtree = dynamic_cast<const js::TriTree*>(raymesh.getTreeDebug());
	//testAssert(kdtree != NULL);

	const js::AABBox bbox_ws = raymesh.getAABBox();

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


#if 0
static void testSelfIntersectionAvoidance()
{
	// We will construct a scene with two quads coplanar to the y-z plane, one at x=0, and the other at x=1.

	StandardPrintOutput print_output;
	glare::TaskManager task_manager;

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
#ifndef NO_EMBREE
	trees.push_back(new EmbreeAccel(&raymesh, true));
	trees.back()->build(print_output, true, task_manager);
#endif
	// Check AABBox
	const AABBox box = trees[0]->getAABBoxWS();
	for(size_t i = 0; i < trees.size(); ++i)
		testAssert(trees[i]->getAABBoxWS() == box);


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
			const Tree::Real dist = (Tree::Real)trees[i]->traceRay(ray, 500.0f, hitinfo);

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
				hitinfo);

			testAssert(dist < 0.0f);
		}

		// Trace the same ray, but with a max distance of 1.0 + nudge.
		// The ray *should* hit the other quad.
		for(size_t i = 0; i < trees.size(); ++i)
		{
			HitInfo hitinfo;
			const Tree::Real dist = (Tree::Real)trees[i]->traceRay(ray,
				1.0f + nudge, // max_t
				hitinfo);

			testAssert(::epsEqual(dist, 1.0f));
			testAssert(hitinfo.sub_elem_index == 2);
		}
	}

	// Delete trees
	for(size_t i = 0; i < trees.size(); ++i)
		delete trees[i];
}
#endif


static void testTree(PCG32& rng, RayMesh& raymesh)
{
	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;
	glare::TaskManager task_manager;

	//------------------------------------------------------------------------
	//Init KD-tree, BVH and Embree accel
	//------------------------------------------------------------------------
	std::vector<Tree*> trees;

	trees.push_back(new BVH(&raymesh));
	trees.back()->build(print_output, should_cancel_callback, task_manager);
#ifndef NO_EMBREE
	trees.push_back(new EmbreeAccel(NULL, &raymesh, /*do_fast_low_quality_build=*/false));
	trees.back()->build(print_output, should_cancel_callback, task_manager);
#endif
	// Check AABBox
	const AABBox box = trees[0]->getAABBox();
	for(size_t i = 0; i < trees.size(); ++i)
		testAssert(trees[i]->getAABBox() == box);


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
			1.0e-5f, // min_t
			std::numeric_limits<float>::max() // max_t
		);

		// Trace against all tris individually
		HitInfo all_tris_hitinfo;
		testAssert(dynamic_cast<BVH*>(trees[0]) != NULL);
		const Tree::Real alltrisdist = 100;// dynamic_cast<BVH*>(trees[0])->traceRayAgainstAllTris(ray, max_t, all_tris_hitinfo);
		failTest("TEMP"); // TODO: implement traceRayAgainstAllTris in this .cpp file.

		// Trace through the trees
		for(size_t t = 0; t < trees.size(); ++t)
		{
			HitInfo hitinfo;
			const Tree::Real dist = (Tree::Real)trees[t]->traceRay(ray, hitinfo);

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
	} // End for each ray

	//------------------------------------------------------------------------
	// Test traceRay() with a ray with NaN components
	// We just want to make sure the trace methods don't crash.  (Embree was crashing on Mac with NaN rays)

	// NOTE: the NaN check is now done in objecttree, so this will crash embree.
	//------------------------------------------------------------------------
	/*const float NaN = std::numeric_limits<float>::quiet_NaN();
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
		const Tree::Real dist = (Tree::Real)trees[t]->traceRay(ray, max_t, NULL, std::numeric_limits<unsigned int>::max(), hitinfo);
		}
		// Try with NaNs in the position vector
		{
		Ray ray(
			Vec4f(NaN, NaN, NaN, 1.0f), 
			Vec4f(0, 0, 0, 0),
			1.0e-05f // t_min
		);
		HitInfo hitinfo;
		const Tree::Real dist = (Tree::Real)trees[t]->traceRay(ray, max_t, NULL, std::numeric_limits<unsigned int>::max(), hitinfo);
		}

		

		// Try with Infs in the direction vector
		{
		Ray ray(
			Vec4f(0, 0, 0, 1.0f), 
			Vec4f(inf, inf, inf, 0),
			1.0e-05f // t_min
		);
		HitInfo hitinfo;
		const Tree::Real dist = (Tree::Real)trees[t]->traceRay(ray, max_t, NULL, std::numeric_limits<unsigned int>::max(), hitinfo);
		}
		// Try with Infs in the position vector
		{
		Ray ray(
			Vec4f(inf, inf, inf, 1.0f), 
			Vec4f(0, 0, 0, 0),
			1.0e-05f // t_min
		);
		HitInfo hitinfo;
		const Tree::Real dist = (Tree::Real)trees[t]->traceRay(ray, max_t, NULL, std::numeric_limits<unsigned int>::max(), hitinfo);
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
		trees[t]->getAllHits(ray, NULL, hitinfos);
		}
		// Try with NaNs in the position vector
		{
		Ray ray(
			Vec4f(NaN, NaN, NaN, 1.0f), 
			Vec4f(0, 0, 0, 0),
			1.0e-05f // t_min
		);
		std::vector<DistanceHitInfo> hitinfos;
		trees[t]->getAllHits(ray, NULL, hitinfos);
		}
	}*/

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

	const SSE_ALIGN Ray ray(Vec3d(1,0,-1), Vec3d(0,0,1));
	HitInfo hitinfo;
	//const double dist = kdtree->traceRay(ray, 1.0e20f, tree_context, NULL, hitinfo);
	}
#endif
}





void TreeTest::doVaryingNumtrisBuildTests()
{
	PCG32 rng(1);
	glare::TaskManager task_manager;

	unsigned int num_tris = 1;
	for(unsigned int i=0; i<21; ++i)
	{
		//------------------------------------------------------------------------
		//Build up a random set of triangles and inserting into a tree
		//------------------------------------------------------------------------
		RayMesh raymesh("raymesh", false);

		const std::vector<Vec2f> texcoord_sets;
		for(unsigned int t=0; t<num_tris; ++t)
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
		DummyShouldCancelCallback should_cancel_callback;
		Geometry::BuildOptions options;

		raymesh.build(
			options,
			should_cancel_callback,
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
#if 0
void TreeTest::doSpeedTest(int treetype)
{
	const std::string BUNNY_PATH = TestUtils::getTestReposDir() + "/testfiles/bun_zipper.ply";

	RayMesh raymesh("bunny", false);
	Indigo::Mesh indigoMesh;
	try
	{
		MeshLoader::loadMesh(BUNNY_PATH, indigoMesh, 1.0);
		raymesh.fromIndigoMesh(indigoMesh);
	}
	catch(glare::Exception&)
	{
		testAssert(false);
	}

	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;
	glare::TaskManager task_manager;


	Timer buildtimer;

	Geometry::BuildOptions options;
	
	raymesh.build(
		options,
		should_cancel_callback,
		print_output,
		true,
		task_manager
		);

	conPrint("Build time: " + toString(buildtimer.getSecondsElapsed()) + " s");

	raymesh.printTreeStats();

	const Vec3d aabb_center(-0.016840, 0.110154, -0.001537);

	SphereUnitVecPool vecpool;//create pool of random points

	HitInfo hitinfo;

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
			1.0e-5f, // min_t
			std::numeric_limits<float>::max() // max_t
		);

		//do the trace
		//ray.buildRecipRayDir();
		const double dist = raymesh.traceRay(ray, hitinfo);

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
#endif


#if 0
void TreeTest::buildSpeedTest()
{
	conPrint("TreeTest::buildSpeedTest()");

	glare::TaskManager task_manager;


	RayMesh raymesh("raymesh", false);
	Indigo::Mesh indigoMesh;
	try
	{
		MeshLoader::loadMesh("c:\\programming\\models\\ply\\happy_recon\\happy_vrip_res3.ply", indigoMesh, 1.0);
		raymesh.fromIndigoMesh(indigoMesh);
	}
	catch(glare::Exception&)
	{
		testAssert(false);
	}

	Timer timer;
	Geometry::BuildOptions options;
	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;
	raymesh.build(
		options,
		should_cancel_callback,
		print_output,
		true,
		task_manager
		);

	printVar(timer.getSecondsElapsed());
}
#endif


// Adapted from RaySphere::traceRay().
static inline float traceRayAgainstSphere(const Vec4f& ray_origin, const Vec4f& ray_unitdir, const Vec4f& sphere_centre, float radius)
{
	// We are using a numerically robust ray-sphere intersection algorithm as described here: http://www.cg.tuwien.ac.at/courses/CG1/textblaetter/englisch/10%20Ray%20Tracing%20(engl).pdf

	const Vec4f raystart_to_centre = sphere_centre - ray_origin;

	const float r2 = radius*radius;

	// Return zero if ray origin is inside sphere.
	if(raystart_to_centre.length2() <= r2)
		return 0.f;

	const float u_dot_del_p = dot(raystart_to_centre, ray_unitdir);

	const float discriminant = r2 - raystart_to_centre.getDist2(ray_unitdir * u_dot_del_p);

	if(discriminant < 0)
		return -1; // No intersection.

	const float sqrt_discriminant = std::sqrt(discriminant);

	const float t_0 = u_dot_del_p - sqrt_discriminant; // t_0 is the smaller of the two solutions.
	return t_0;
}


static void testSphereTracingOnMesh(RayMesh& raymesh)
{
	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;
	glare::TaskManager task_manager;
	PCG32 rng(1);

	BVH bvh(&raymesh);
	bvh.build(print_output, should_cancel_callback, task_manager);

	const Matrix4f to_world = Matrix4f::identity();
	const Matrix4f to_object = Matrix4f::identity();

	//------------------------------------------------------------------------
	//compare tests against all tris with tests against the trees
	//------------------------------------------------------------------------
	const int NUM_RAYS = 10000;
	for(int i=0; i<NUM_RAYS; ++i)
	{
		//------------------------------------------------------------------------
		//test first hit traces
		//------------------------------------------------------------------------
		const double max_t = rng.unitRandom() * 2.0f;
		const Ray ray(
			Vec4f(0, 0, 0, 1.0f) + Vec4f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, 0) * 1.5f,
			normalise(Vec4f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, 0)),
			1.0e-5f, // min_t
			(float)max_t // max_t
		);
		const float radius = rng.unitRandom() * 0.2f;
		
		Vec4f hit_normal;
		const double d = bvh.traceSphere(ray, to_object, to_world, radius, hit_normal);

		// Do reference trace against all triangles
		const Vec4f sourcePoint(ray.startPos());
		const Vec4f unitdir(ray.unitDir());
		const js::BoundingSphere sphere_os(ray.startPos(), radius);
		float closest_dist = (float)max_t;
		Vec4f ref_hit_normal;
		for(unsigned int t=0; t<raymesh.getNumTris(); ++t)
		{
			const Vec3f orig_v0 = raymesh.triVertPos(t, 0);
			const Vec3f orig_v1 = raymesh.triVertPos(t, 1);
			const Vec3f orig_v2 = raymesh.triVertPos(t, 2);

			MollerTrumboreTri tri;
			tri.set(orig_v0, orig_v1, orig_v2);

			const Vec4f v0(tri.data[0], tri.data[1], tri.data[2], 1.f);
			const Vec4f e1(tri.data[3], tri.data[4], tri.data[5], 0.f);
			const Vec4f e2(tri.data[6], tri.data[7], tri.data[8], 0.f);

			const Vec4f normal = normalise(crossProduct(e1, e2));
			js::Triangle js_tri(v0, e1, e2, normal);


			Planef tri_plane(v0, normal);

			// Determine the distance from the plane to the sphere center
			float pDist = tri_plane.signedDistToPoint(sourcePoint);

			//-----------------------------------------------------------------
			//Invert normal if doing backface collision, so 'usenormal' is always facing
			//towards sphere center.
			//-----------------------------------------------------------------
			Vec4f usenormal = normal;
			if(pDist < 0)
			{
				usenormal *= -1;
				pDist *= -1;
			}

			assert(pDist >= 0);

			//-----------------------------------------------------------------
			//check if sphere is heading away from tri
			//-----------------------------------------------------------------
			const float approach_rate = -dot(usenormal, unitdir);
			if(approach_rate <= 0)
				continue;

			assert(approach_rate > 0);

			// trans_len_needed = dist to approach / dist approached per unit translation len
			const float trans_len_needed = (pDist - sphere_os.getRadius()) / approach_rate;

			if(closest_dist < trans_len_needed)
				continue; // then sphere will never get to plane

			//-----------------------------------------------------------------
			//calc the point where the sphere intersects with the triangle plane (planeIntersectionPoint)
			//-----------------------------------------------------------------
			Vec4f planeIntersectionPoint;

			// Is the plane embedded in the sphere?
			if(trans_len_needed <= 0)//pDist <= sphere.getRadius())//make == trans_len_needed < 0
			{
				// Calculate the plane intersection point
				planeIntersectionPoint = tri_plane.closestPointOnPlane(sourcePoint);

			}
			else
			{
				assert(trans_len_needed >= 0);

				planeIntersectionPoint = sourcePoint + (unitdir * trans_len_needed) - (usenormal * sphere_os.getRadius());

				//assert point is actually on plane
				//			assert(epsEqual(tri.getTriPlane().signedDistToPoint(planeIntersectionPoint), 0.0f, 0.0001f));
			}

			//-----------------------------------------------------------------
			//now restrict collision point on tri plane to inside tri if neccessary.
			//-----------------------------------------------------------------
			Vec4f triIntersectionPoint = planeIntersectionPoint;

			const bool point_in_tri = js_tri.pointInTri(triIntersectionPoint);
			float dist; // Distance until sphere hits triangle
			if(point_in_tri)
			{
				dist = myMax(0.f, trans_len_needed);
			}
			else
			{
				// Restrict to inside tri
				triIntersectionPoint = js_tri.closestPointOnTriangle(triIntersectionPoint);

				// Using the triIntersectionPoint, we need to reverse-intersect with the sphere
				//dist = sphere_os.rayIntersect(triIntersectionPoint, -ray.unitDir()); // returns dist till hit sphere or -1 if missed
				dist = traceRayAgainstSphere(/*ray_origin=*/triIntersectionPoint, /*ray_unitdir=*/-ray.unitDir(), /*sphere_centre=*/ray.startPos(), radius);
			}

			if(dist >= 0 && dist < closest_dist)
			{
				closest_dist = dist;

				//-----------------------------------------------------------------
				//calc hit normal
				//-----------------------------------------------------------------
				if(point_in_tri)
					ref_hit_normal = usenormal;
				else
				{
					//-----------------------------------------------------------------
					//calc point sphere will be when it hits edge of tri
					//-----------------------------------------------------------------
					const Vec4f hit_spherecenter = sourcePoint + unitdir * dist;

					ref_hit_normal = normalise(hit_spherecenter - triIntersectionPoint);
				}
			}
		}

		const bool bvh_hit = d >= 0;
		const bool ref_hit = closest_dist < max_t;
		testAssert(bvh_hit == ref_hit);

		if(bvh_hit)
		{
			testAssert(d <= max_t);
			testEpsEqual(hit_normal.length(), 1.0f);

			testEpsEqualWithEps((float)d, closest_dist, 1.0e-4f);

			if(d > 0) // In the d==0 case, this means one or more triangles were embedded in the sphere.  Therefore we may not get the closest tri, so normals may differ.
				testEpsEqualWithEps(hit_normal, ref_hit_normal, 1.0e-4f);
			
			testEpsEqual(ref_hit_normal.length(), 1.0f);
		}
	}
}


void TreeTest::doSphereTracingTests(const std::string& appdata_path)
{
	Geometry::BuildOptions options;
	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;
	glare::TaskManager task_manager;
	

	{
		// Test with a single triangle in the x-y plane.
		RayMesh raymesh("raymesh", false);
		raymesh.addVertex(Vec3f(0.f, 0.f, 0.f));
		raymesh.addVertex(Vec3f(1.f, 0.f, 0.f));
		raymesh.addVertex(Vec3f(0.f, 1.f, 0.f));
		const unsigned int vertex_indices[] = { 0, 1, 2 };
		const unsigned int uv_indices[] = { 0, 0, 0 };
		raymesh.addTriangle(vertex_indices, uv_indices, 0);

		raymesh.build(options, should_cancel_callback, print_output, false, task_manager);

		testSphereTracingOnMesh(raymesh);
	}
	

	//------------------------------------------------------------------------
	//try building up a random set of triangles and inserting into a tree
	//------------------------------------------------------------------------
	{
		RayMesh raymesh("raymesh", false);

		const unsigned int NUM_TRIS = 20;
		const std::vector<Vec2f> texcoord_sets;
		PCG32 rng(1);
		for(unsigned int i=0; i<NUM_TRIS; ++i)
		{
			const Vec3f pos(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f);

			raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f);//, Vec3f(0,0,1));
			raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f);//, Vec3f(0,0,1));
			raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f);//, Vec3f(0,0,1));
			const unsigned int vertex_indices[] ={ i*3, i*3+1, i*3+2 };
			const unsigned int uv_indices[] ={ 0, 0, 0 };
			raymesh.addTriangle(vertex_indices, uv_indices, 0);
		}

		raymesh.build(options, should_cancel_callback, print_output, false, task_manager);

		testSphereTracingOnMesh(raymesh);
	}
}


//for sorting Vec3's
struct Vec4fLessThan
{
	bool operator () (const Vec4f& a, const Vec4f& b)
	{
		if(a[0] < b[0])
			return true;
		else if(a[0] > b[0])
			return false;
		else	//else x == rhs.x
		{
			if(a[1] < b[1])
				return true;
			else if(a[1] > b[1])
				return false;
			else
			{
				return a[2] < b[2];
			}
		}
	}
};


static void testAppendCollPoints(RayMesh& raymesh)
{
	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;
	glare::TaskManager task_manager;
	PCG32 rng(1);

	BVH bvh(&raymesh);
	bvh.build(print_output, should_cancel_callback, task_manager);

	std::vector<Vec4f> points_ws;
	std::vector<Vec4f> ref_points_ws;

	const Matrix4f to_object = Matrix4f::identity();
	const Matrix4f to_world = Matrix4f::identity();

	//------------------------------------------------------------------------
	//compare tests against all tris with tests against the trees
	//------------------------------------------------------------------------
	const int NUM_RAYS = 10000;
	for(int i=0; i<NUM_RAYS; ++i)
	{
		points_ws.resize(0);
		ref_points_ws.resize(0);

		//------------------------------------------------------------------------
		//test first hit traces
		//------------------------------------------------------------------------
		const Vec4f sphere_pos = Vec4f(0, 0, 0, 1.0f) + Vec4f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, 0) * 1.5f;
		const float radius = rng.unitRandom() * 0.6f;
		bvh.appendCollPoints(sphere_pos, radius, to_object, to_world, points_ws);

		// Do reference test against all triangles
		const float radius2 = radius*radius;

		for(unsigned int t=0; t<raymesh.getNumTris(); ++t)
		{
			const Vec3f orig_v0 = raymesh.triVertPos(t, 0);
			const Vec3f orig_v1 = raymesh.triVertPos(t, 1);
			const Vec3f orig_v2 = raymesh.triVertPos(t, 2);

			MollerTrumboreTri tri;
			tri.set(orig_v0, orig_v1, orig_v2);

			const Vec4f v0(tri.data[0], tri.data[1], tri.data[2], 1.f);
			const Vec4f e1(tri.data[3], tri.data[4], tri.data[5], 0.f);
			const Vec4f e2(tri.data[6], tri.data[7], tri.data[8], 0.f);

			const Vec4f normal = normalise(crossProduct(e1, e2));
			const js::Triangle js_tri(v0, e1, e2, normal);


			// See if sphere is touching plane
			const Planef tri_plane(v0, normal);
			const float disttoplane = tri_plane.signedDistToPoint(sphere_pos);
			if(fabs(disttoplane) > radius)
				continue;

			// Get closest point on plane to sphere center
			Vec4f planepoint = tri_plane.closestPointOnPlane(sphere_pos);

			// Restrict point to inside tri
			if(!js_tri.pointInTri(planepoint))
				planepoint = js_tri.closestPointOnTriangle(planepoint);

			if(planepoint.getDist2(sphere_pos) <= radius2)
			{
				if(ref_points_ws.empty() || (planepoint != ref_points_ws.back())) // HACK: Don't add if same as last point.  May happen due to packing of 4 tris together with possible duplicates.
					ref_points_ws.push_back(planepoint);
			}
		}

		std::sort(points_ws.begin(), points_ws.end(), Vec4fLessThan());
		std::sort(ref_points_ws.begin(), ref_points_ws.end(), Vec4fLessThan());
		testAssert(points_ws == ref_points_ws);
	}
}


void TreeTest::doAppendCollPointsTests(const std::string& appdata_path)
{
	Geometry::BuildOptions options;
	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;
	glare::TaskManager task_manager;
	PCG32 rng(1);

	//------------------------------------------------------------------------
	//try building up a random set of triangles and inserting into a tree
	//------------------------------------------------------------------------
	{
		RayMesh raymesh("raymesh", false);

		const unsigned int NUM_TRIS = 20;
		const std::vector<Vec2f> texcoord_sets;
		for(unsigned int i=0; i<NUM_TRIS; ++i)
		{
			const Vec3f pos(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f);

			raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f);
			raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f);
			raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f);
			const unsigned int vertex_indices[] ={ i*3, i*3+1, i*3+2 };
			const unsigned int uv_indices[] ={ 0, 0, 0 };
			raymesh.addTriangle(vertex_indices, uv_indices, 0);
		}

		raymesh.build(options, should_cancel_callback, print_output, false, task_manager);

		testAppendCollPoints(raymesh);
	}
}


void TreeTest::doTests(const std::string& appdata_path)
{
	conPrint("TreeTest::doTests()");

	//	doVaryingNumtrisBuildTests();

	//testSelfIntersectionAvoidance();

	doAppendCollPointsTests(appdata_path);

	doSphereTracingTests(appdata_path);

	doEdgeCaseTests();

	

	Geometry::BuildOptions options;
	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;
	glare::TaskManager task_manager;
	PCG32 rng(2);

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
		const std::string MODEL_PATH = TestUtils::getTestReposDir() + "/testfiles/bug-2.igmesh";
		RayMesh raymesh("tricky", false);
		Indigo::Mesh indigoMesh;
		try
		{
			//MeshLoader::loadMesh(MODEL_PATH, indigoMesh, 1.0);
			Indigo::Mesh::readFromFile(toIndigoString(MODEL_PATH), indigoMesh);
			raymesh.fromIndigoMesh(indigoMesh);

			raymesh.build(options, should_cancel_callback, print_output, false, task_manager);
		}
		catch(glare::Exception&)
		{
			testAssert(false);
		}
		catch(Indigo::IndigoException&)
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

		const unsigned int NUM_TRIS = 1000;
		const std::vector<Vec2f> texcoord_sets;
		for(unsigned int i=0; i<NUM_TRIS; ++i)
		{
			const Vec3f pos(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f);

			raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f);//, Vec3f(0,0,1));
			raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f);//, Vec3f(0,0,1));
			raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f);//, Vec3f(0,0,1));
			const unsigned int vertex_indices[] ={ i*3, i*3+1, i*3+2 };
			const unsigned int uv_indices[] ={ 0, 0, 0 };
			raymesh.addTriangle(vertex_indices, uv_indices, 0);
		}

		raymesh.build(options, should_cancel_callback, print_output, false, task_manager);

		testTree(rng, raymesh);
	}

	//------------------------------------------------------------------------
	//build a tree with lots of axis-aligned triangles - a trickier case
	//------------------------------------------------------------------------
	{
		RayMesh raymesh("raymesh", false);

		const unsigned int NUM_TRIS = 1000;
		const std::vector<Vec2f> texcoord_sets;
		for(unsigned int i=0; i<NUM_TRIS; ++i)
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

			const unsigned int vertex_indices[] ={ i*3, i*3+1, i*3+2 };
			const unsigned int uv_indices[] ={ 0, 0, 0 };
			raymesh.addTriangle(vertex_indices, uv_indices, 0);
		}

		raymesh.build(options, should_cancel_callback, print_output, false, task_manager);

		testTree(rng, raymesh);
	}

	conPrint("TreeTest::doTests(): Done.");
}



void TreeTest::doRayTests()
{
	//Ray ray(Vec3d(0,0,0), Vec3d(0,0,1));

	/*const float recip_x = ray.getRecipRayDirF().x;

	testAssert(!isInf(recip_x));
	testAssert(!isInf(ray.getRecipRayDirF().y));
	testAssert(!isInf(ray.getRecipRayDirF().z));*/

}


#endif // BUILD_TESTS


} //end namespace js






