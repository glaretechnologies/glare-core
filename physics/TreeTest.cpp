/*=====================================================================
TreeTest.cpp
------------
File created by ClassTemplate on Tue Jun 26 20:19:05 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "TreeTest.h"

#include "jscol_tritree.h"
#include "jscol_BIHTree.h"
#include "../simpleraytracer/raymesh.h"
#include "../utils/MTwister.h"
#include "../raytracing/HitInfo.h"
#include "jscol_TriTreePerThreadData.h"
#include "../indigo/FullHitInfo.h"
#include <algorithm>
#include "../simpleraytracer/CSModelLoader.h"
#include "../simpleraytracer/raymesh.h"
#include "../utils/SphereUnitVecPool.h"
#include "../utils/timer.h"
#include "../indigo/TestUtils.h"
#include "../utils/PlatformUtils.h"

namespace js
{




TreeTest::TreeTest()
{
	
}


TreeTest::~TreeTest()
{
	
}

void TreeTest::testBuildCorrect()
{
	conPrint("TreeTest::testBuildCorrect()");

	{
	RayMesh raymesh("raymesh", false);
	
	const std::vector<Vec2f> texcoord_sets;

	//x=0 tri
	{
	raymesh.addVertex(Vec3f(0,0,1), Vec3f(0,0,1), texcoord_sets);
	raymesh.addVertex(Vec3f(0,1,0), Vec3f(0,0,1), texcoord_sets);
	raymesh.addVertex(Vec3f(0,1,1), Vec3f(0,0,1), texcoord_sets);
	const unsigned int vertex_indices[] = {0, 1, 2};
	raymesh.addTriangle(vertex_indices, 0);
	}

	{
	//x=1 tri
	raymesh.addVertex(Vec3f(1.f,0,1), Vec3f(0,0,1), texcoord_sets);
	raymesh.addVertex(Vec3f(1.f,1,0), Vec3f(0,0,1), texcoord_sets);
	raymesh.addVertex(Vec3f(1.f,1,1), Vec3f(0,0,1), texcoord_sets);
	const unsigned int vertex_indices[] = {3, 4, 5};
	raymesh.addTriangle(vertex_indices, 0);
	}
	{
	//x=10 tri
	raymesh.addVertex(Vec3f(10.f,0,1), Vec3f(0,0,1), texcoord_sets);
	raymesh.addVertex(Vec3f(10.f,1,0), Vec3f(0,0,1), texcoord_sets);
	raymesh.addVertex(Vec3f(10.f,1,1), Vec3f(0,0,1), texcoord_sets);
	const unsigned int vertex_indices[] = {6, 7, 8};
	raymesh.addTriangle(vertex_indices, 0);
	}

	raymesh.build(false);

	const js::TriTree* kdtree = dynamic_cast<const js::TriTree*>(raymesh.getTreeDebug());
	testAssert(kdtree != NULL);

	const js::AABBox bbox_ws = raymesh.getAABBoxWS();

	testAssert(bbox_ws.min_ == Vec3f(0, 0, 0));
	testAssert(bbox_ws.max_ == Vec3f(10.f, 1, 1));

	testAssert(kdtree->getNodesDebug().size() == 3);
	testAssert(!kdtree->getNodesDebug()[0].isLeafNode() != 0);
	testAssert(kdtree->getNodesDebug()[0].getPosChildIndex() == 2);
	testAssert(kdtree->getNodesDebug()[0].getSplittingAxis() == 0);
	testAssert(kdtree->getNodesDebug()[0].data2.dividing_val == 1.0f);

	testAssert(kdtree->getNodesDebug()[1].isLeafNode() != 0);
	testAssert(kdtree->getNodesDebug()[1].getLeafGeomIndex() == 0);
	testAssert(kdtree->getNodesDebug()[1].getNumLeafGeom() == 1);

	testAssert(kdtree->getNodesDebug()[2].isLeafNode() != 0);
	testAssert(kdtree->getNodesDebug()[2].getLeafGeomIndex() == 1);
	testAssert(kdtree->getNodesDebug()[2].getNumLeafGeom() == 2);

	}

	{
	RayMesh raymesh("raymesh", false);
	
	const std::vector<Vec2f> texcoord_sets;

	//x=0 tri
	{
	raymesh.addVertex(Vec3f(0,0,1), Vec3f(0,0,1), texcoord_sets);
	raymesh.addVertex(Vec3f(0,1,0), Vec3f(0,0,1), texcoord_sets);
	raymesh.addVertex(Vec3f(0,1,1), Vec3f(0,0,1), texcoord_sets);
	const unsigned int vertex_indices[] = {0, 1, 2};
	raymesh.addTriangle(vertex_indices, 0);
	}

	{
	//x=1 tri
	raymesh.addVertex(Vec3f(1.f,0,1), Vec3f(0,0,1), texcoord_sets);
	raymesh.addVertex(Vec3f(1.f,1,0), Vec3f(0,0,1), texcoord_sets);
	raymesh.addVertex(Vec3f(1.f,1,1), Vec3f(0,0,1), texcoord_sets);
	const unsigned int vertex_indices[] = {3, 4, 5};
	raymesh.addTriangle(vertex_indices, 0);
	}

	{
	//x=1 tri
	raymesh.addVertex(Vec3f(1.f,0,1), Vec3f(0,0,1), texcoord_sets);
	raymesh.addVertex(Vec3f(1.f,1,0), Vec3f(0,0,1), texcoord_sets);
	raymesh.addVertex(Vec3f(1.f,1,1), Vec3f(0,0,1), texcoord_sets);
	const unsigned int vertex_indices[] = {6, 7, 8};
	raymesh.addTriangle(vertex_indices, 0);
	}

	{
	//x=10 tri
	raymesh.addVertex(Vec3f(10.f,0,1), Vec3f(0,0,1), texcoord_sets);
	raymesh.addVertex(Vec3f(10.f,1,0), Vec3f(0,0,1), texcoord_sets);
	raymesh.addVertex(Vec3f(10.f,1,1), Vec3f(0,0,1), texcoord_sets);
	const unsigned int vertex_indices[] = {9, 10, 11};
	raymesh.addTriangle(vertex_indices, 0);
	}

	raymesh.build(false);

	const js::TriTree* kdtree = dynamic_cast<const js::TriTree*>(raymesh.getTreeDebug());
	testAssert(kdtree != NULL);

	const js::AABBox bbox_ws = raymesh.getAABBoxWS();

	testAssert(bbox_ws.min_ == Vec3f(0, 0, 0));
	testAssert(bbox_ws.max_ == Vec3f(10.f, 1, 1));

	testAssert(kdtree->getNodesDebug().size() == 3);
	testAssert(!kdtree->getNodesDebug()[0].isLeafNode());
	testAssert(kdtree->getNodesDebug()[0].getPosChildIndex() == 2);
	testAssert(kdtree->getNodesDebug()[0].getSplittingAxis() == 0);
	testAssert(kdtree->getNodesDebug()[0].data2.dividing_val == 1.0f);

	testAssert(kdtree->getNodesDebug()[1].isLeafNode() != 0);
	testAssert(kdtree->getNodesDebug()[1].getLeafGeomIndex() == 0);
	testAssert(kdtree->getNodesDebug()[1].getNumLeafGeom() == 1);

	testAssert(kdtree->getNodesDebug()[2].isLeafNode() != 0);
	testAssert(kdtree->getNodesDebug()[2].getLeafGeomIndex() == 1);
	testAssert(kdtree->getNodesDebug()[2].getNumLeafGeom() == 3);

	}
}


static void testTree(MTwister& rng, RayMesh& raymesh)
{
	//------------------------------------------------------------------------
	//Init KD-tree and BIH
	//------------------------------------------------------------------------
	TriTree tritree(&raymesh);
	tritree.build();

	BIHTree bih_tree(&raymesh);
	bih_tree.build();

	// Check AABBox
	testAssert(tritree.getAABBoxWS() == bih_tree.getAABBoxWS());

	//------------------------------------------------------------------------
	//compare tests against all tris with tests against the tree
	//------------------------------------------------------------------------
	const int NUM_RAYS = 1000;
	double dist, dist2, dist3, dist4;
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

		//ray.buildRecipRayDir();
		HitInfo hitinfo, hitinfo2;
		js::TriTreePerThreadData tree_context;

		dist = tritree.traceRayAgainstAllTris(ray, max_t, hitinfo);
		dist2 = tritree.traceRay(ray, max_t, tree_context, hitinfo2);
		
		testAssert(dist == dist2);
		testAssert(hitinfo == hitinfo2);

		HitInfo hitinfo3, hitinfo4;
		dist3 = bih_tree.traceRayAgainstAllTris(ray, max_t, hitinfo3);
		dist4 = bih_tree.traceRay(ray, max_t, tree_context, hitinfo4);

		testAssert(dist2 == dist3);
		testAssert(dist3 == dist4);
		testAssert(hitinfo2 == hitinfo3);
		testAssert(hitinfo2 == hitinfo4);

		//------------------------------------------------------------------------
		//test getAllHits()
		//------------------------------------------------------------------------
		std::vector<FullHitInfo> hitinfos;
		tritree.getAllHits(ray, tree_context, hitinfos);
		std::sort(hitinfos.begin(), hitinfos.end(), FullHitInfoComparisonPred);

		if(dist > 0.0)
		{
			//if ray hit anything before
			testAssert(hitinfos.size() >= 1);
			testAssert(hitinfos[0].dist == dist);
			testAssert(hitinfos[0].hittri_index == hitinfo.hittriindex);
			testAssert(hitinfos[0].tri_coords == hitinfo.hittricoords);
		}

		// Do a check against all tris
		std::vector<FullHitInfo> hitinfos_d;
		tritree.getAllHitsAllTris(ray, hitinfos_d);
		std::sort(hitinfos_d.begin(), hitinfos_d.end(), FullHitInfoComparisonPred);

		// Compare results
		testAssert(hitinfos.size() == hitinfos_d.size());
		for(unsigned int z=0; z<hitinfos.size(); ++z)
		{
			testAssert(hitinfos[z].dist == hitinfos_d[z].dist);
			testAssert(hitinfos[z].hittri_index == hitinfos_d[z].hittri_index);
			testAssert(hitinfos[z].tri_coords == hitinfos_d[z].tri_coords);
		}

		//------------------------------------------------------------------------
		//test getAllHits() on BIH
		//------------------------------------------------------------------------
		std::vector<FullHitInfo> hitinfos_bih;
		bih_tree.getAllHits(ray, tree_context, hitinfos_bih);
		std::sort(hitinfos_bih.begin(), hitinfos_bih.end(), FullHitInfoComparisonPred);

		// Compare results
		testAssert(hitinfos.size() == hitinfos_bih.size());
		for(unsigned int z=0; z<hitinfos.size(); ++z)
		{
			testAssert(hitinfos[z].dist == hitinfos_bih[z].dist);
			testAssert(hitinfos[z].hittri_index == hitinfos_bih[z].hittri_index);
			testAssert(hitinfos[z].tri_coords == hitinfos_bih[z].tri_coords);
		}


		//------------------------------------------------------------------------
		//Test doesFiniteRayHit()
		//------------------------------------------------------------------------
		const double testlength = rng.unitRandom() * 2.0;
		bool kd_hit = tritree.doesFiniteRayHit(ray, testlength, tree_context);
		bool bih_hit = bih_tree.doesFiniteRayHit(ray, testlength, tree_context);
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
			kd_hit = tritree.doesFiniteRayHit(ray, dist + 0.0001, tree_context);
			bih_hit = bih_tree.doesFiniteRayHit(ray, dist + 0.0001, tree_context);
			testAssert(kd_hit == bih_hit);

			kd_hit = tritree.doesFiniteRayHit(ray, dist - 0.0001, tree_context);
			bih_hit = bih_tree.doesFiniteRayHit(ray, dist - 0.0001, tree_context);
			testAssert(kd_hit == bih_hit);
		}
	}
}




void TreeTest::doTests()
{
	MTwister rng(1);
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

		raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f, Vec3f(0,0,1), texcoord_sets);
		raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f, Vec3f(0,0,1), texcoord_sets);
		raymesh.addVertex(pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f, Vec3f(0,0,1), texcoord_sets);
		const unsigned int vertex_indices[] = {i*3, i*3+1, i*3+2};
		raymesh.addTriangle(vertex_indices, 0);
	}

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
		raymesh.addVertex(v, Vec3f(0,0,1), texcoord_sets);
		
		v = pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f;
		v[axis] = axis_val;
		raymesh.addVertex(v, Vec3f(0,0,1), texcoord_sets);
		
		v = pos + Vec3f(-1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f, -1.0f + rng.unitRandom()*2.0f)*0.1f;
		v[axis] = axis_val;
		raymesh.addVertex(v, Vec3f(0,0,1), texcoord_sets);

		const unsigned int vertex_indices[] = {i*3, i*3+1, i*3+2};
		raymesh.addTriangle(vertex_indices, 0);
	}

	testTree(rng, raymesh);
	}

}




void TreeTest::doSpeedTest()
{
	const std::string BUNNY_PATH = "c:\\programming\\models\\bunny\\reconstruction\\bun_zipper.ply";

	CSModelLoader model_loader;
	RayMesh raymesh("raymesh", false);
	try
	{
		model_loader.streamModel(BUNNY_PATH, raymesh, 1.0);
	}
	catch(CSModelLoaderExcep& e)
	{
		::fatalError(e.what());
	}

	raymesh.build(false);

	const Vec3d aabb_center(-0.016840, 0.110154, -0.001537);

	SphereUnitVecPool vecpool;//create pool of random points

	HitInfo hitinfo;
	js::TriTreePerThreadData tree_context;

	Timer testtimer;//start timer
	int num_hits = 0;//number of rays that actually hit the model

	const int NUM_ITERS = 20000000;
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
		const double dist = raymesh.traceRay(ray, 1.0e20f, tree_context, hitinfo);

		if(dist >= 0.0)//if hit the model
			num_hits++;//count the hit.
	}

	const double timetaken = testtimer.getSecondsElapsed();

	const double fraction_hit = (double)num_hits / (double)NUM_ITERS;

	const double traces_per_sec = (double)NUM_ITERS / timetaken;

	printVar(fraction_hit);
	printVar(traces_per_sec);

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
	raymesh.build(false);

	printVar(timer.getSecondsElapsed());

	PlatformUtils::Sleep(10000);
}



} //end namespace js






