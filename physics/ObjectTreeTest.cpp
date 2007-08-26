/*=====================================================================
ObjectTreeTest.cpp
------------------
File created by ClassTemplate on Sat Apr 22 20:17:04 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "ObjectTreeTest.h"

#include "jscol_tritree.h"
#include "jscol_ObjectTree.h"
#include "../raytracing/hitinfo.h"
#include "jscol_TriTreePerThreadData.h"
#include "jscol_ObjectTreePerThreadData.h"
#include "../utils/random.h"
#include "../simpleraytracer/raysphere.h"
#include "../utils/MTwister.h"
#include <iostream>
#include "../indigo/TestUtils.h"
#include "../utils/timer.h"
//#include "../indigo/InstancedGeom.h"
#include "../simpleraytracer/CSModelLoader.h"
#include "../simpleraytracer/raymesh.h"


namespace js
{

/*
ObjectTreeTest::ObjectTreeTest()
{
	
}


ObjectTreeTest::~ObjectTreeTest()
{
	
}*/

void ObjectTreeTest::doTests()
{	
	conPrint("ObjectTreeTest::doTests()");
	MTwister rng(1);

	ObjectTree ob_tree;

	/// Add some random spheres ////
	const int N = 1000;
	for(int i=0; i<N; ++i)
	{
		Object* ob = new Object(new RaySphere(Vec3d(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()), rng.unitRandom() * 0.05), Vec3d(0,0,0), Matrix3d::identity());
		ob->build();
		ob_tree.insertObject(ob); 
	}
	ob_tree.build();

	//ob_tree.printTree(0, 0, std::cout);
	ObjectTreeStats stats;
	ob_tree.getTreeStats(stats);

	TriTreePerThreadData tritree_context;
	ObjectTreePerThreadData* obtree_context = ob_tree.allocContext();

	

	/// Do some random traces through the tree ///
	for(int i=0; i<10000; ++i)
	{
		const SSE_ALIGN Ray ray(
			(Vec3d(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()) - Vec3d(0.2, 0.2, 0.2)) * 1.4,
			normalise(Vec3d(Vec3d(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()) - Vec3d(0.5, 0.5, 0.5)))
			);

		//ray.buildRecipRayDir();

		//------------------------------------------------------------------------
		//do traceRay() test
		//------------------------------------------------------------------------
		HitInfo hitinfo, hitinfo2;
		js::ObjectTree::INTERSECTABLE_TYPE* hitob = (js::ObjectTree::INTERSECTABLE_TYPE*)0xF;
		const double t = ob_tree.traceRay(ray, tritree_context, *obtree_context, hitob, hitinfo);
		const double t2 = ob_tree.traceRayAgainstAllObjects(ray, tritree_context, *obtree_context, hitob, hitinfo2);
		testAssert(hitob != (js::ObjectTree::INTERSECTABLE_TYPE*)0xF);

		testAssert(t > 0.0 == t2 > 0.0);

		if(t >= 0.0 || t2 >= 0.0)
		{
			testAssert(t == t2);
			testAssert(hitinfo.hittricoords == hitinfo2.hittricoords);
			testAssert(hitinfo.hittriindex == hitinfo2.hittriindex);
		}

		//------------------------------------------------------------------------
		//Do a doesFiniteRayHitAnything() test
		//------------------------------------------------------------------------
		const double len = rng.unitRandom() * 1.5;
		const bool a = ob_tree.doesFiniteRayHitAnything(ray, len, tritree_context, *obtree_context);
		const bool b = ob_tree.allObjectsDoesFiniteRayHitAnything(ray, len, tritree_context, *obtree_context);
		testAssert(a == b);

		if(t >= 0.0)//if the trace hit something after distance t
		{
			//then either we should have hit, or len < t
			testAssert(a || len < t);
		}
	}

/*
	{
	js::Triangle tri(Vec3(0,0,0), Vec3(0,1,0), Vec3(1,1,0));

	TriTree tritree;
	tritree.insertTri(tri);

	tritree.build();
	//context = tritree.allocPerThreadData();

	{
	SSE_ALIGN Ray ray(Vec3(0.2f, 0.8f, 1.0f), Vec3(0,0,-1));
	ray.buildRecipRayDir();
	dist = tritree.traceRay(ray, 1e9f, context, hitinfo);

	assert(::epsEqual(dist, 1.0f));
	assert(hitinfo.hittricoords.x > 0.0f && hitinfo.hittricoords.x < 1.0f);
	assert(hitinfo.hittricoords.y > 0.0f && hitinfo.hittricoords.y < 1.0f);
	assert(hitinfo.hittriindex == 0);
	}
	{
	SSE_ALIGN Ray ray(Vec3(0.0f, 0.0f, 1.0f), Vec3(0,0,-1));
	ray.buildRecipRayDir();
	dist = tritree.traceRay(ray, 1e9f, context, hitinfo);

	assert(epsEqual(dist, 1.0f));
	assert(epsEqual(hitinfo.hittricoords.x, 0.0f));
	assert(epsEqual(hitinfo.hittricoords.y, 0.0f));
	assert(hitinfo.hittriindex == 0);
	}
	{
	SSE_ALIGN Ray ray(Vec3(0.999999f, 0.999999f, 1.0f), Vec3(0,0,-1));
	ray.buildRecipRayDir();
	dist = tritree.traceRay(ray, 1e9f, context, hitinfo);

	assert(epsEqual(dist, 1.0f));
	assert(epsEqual(hitinfo.hittricoords.x, 0.0f));
	assert(epsEqual(hitinfo.hittricoords.y, 1.0f));
	assert(hitinfo.hittriindex == 0);

	dist2 = tritree.traceRayAgainstAllTris(ray, hitinfo2);
	assert(dist == dist2);
	assert(hitinfo == hitinfo2);
	}

	}

	//------------------------------------------------------------------------
	//try building up a random set of triangles and inserting into a tree
	//------------------------------------------------------------------------
	{
	TriTree tritree;
	const int NUM_TRIS = 1000;
	for(int i=0; i<NUM_TRIS; ++i)
	{
		js::Triangle tri(Vec3::randomVec(-2, 2), Vec3::randomVec(-2, 2), Vec3::randomVec(-2, 2));
		tritree.insertTri(tri);
	}

	tritree.build();

	//------------------------------------------------------------------------
	//compare tests against all tris with tests against the tree
	//------------------------------------------------------------------------
	const int NUM_RAYS = 1000;
	for(int i=0; i<NUM_RAYS; ++i)
	{
		const float max_t = 1e9f;

		SSE_ALIGN Ray ray(Vec3::randomVec(-3, 3), normalise(Vec3::randomVec(-1, 1)));
		ray.buildRecipRayDir();
		dist = tritree.traceRayAgainstAllTris(ray, hitinfo);
		dist2 = tritree.traceRay(ray, max_t, context, hitinfo2);
		assert(dist == dist2);
		assert(hitinfo == hitinfo2);
	}
	}

	//------------------------------------------------------------------------
	//test out object tree
	//------------------------------------------------------------------------
	{
	ObjectTree obtree;
	const int NUM_OBJECTS = 1000;
	std::vector<TriTree*> tritrees;
	for(int i=0; i<NUM_OBJECTS; ++i)
	{
		TriTree* tritree = new TriTree;
		const int NUM_TRIS = 3;
		for(int i=0; i<NUM_TRIS; ++i)
		{
			const Vec3 v0 = Vec3::randomVec(-2, 2);
			js::Triangle tri(v0, v0 + Vec3::randomVec(-0.3f, 0.3f), v0 + Vec3::randomVec(-0.3f, 0.3f));
			tritree->insertTri(tri);
		}

		tritree->build();
		obtree.insertObject(tritree);
		tritrees.push_back(tritree);
	}
	obtree.build();
	js::ObjectTreePerThreadData* objecttree_context = obtree.allocContext();

	//------------------------------------------------------------------------
	//compare tests against all tritrees with tests against the object tree
	//------------------------------------------------------------------------
	const int NUM_RAYS = 1000;
	for(int i=0; i<NUM_RAYS; ++i)
	{
		const float max_t = 1e9f;

		SSE_ALIGN Ray ray(Vec3::randomVec(-3, 3), normalise(Vec3::randomVec(-1, 1)));
		ray.buildRecipRayDir();

		Intersectable* hitob = NULL;
		Intersectable* hitob2 = NULL;
		dist = obtree.traceRayAgainstAllObjects(ray, context, *objecttree_context, hitob, hitinfo);
		dist2 = obtree.traceRay(ray, context, *objecttree_context, hitob2, hitinfo2);

		assert(dist == dist2);
		assert(hitob == hitob2);
		assert(hitinfo == hitinfo2);

		//------------------------------------------------------------------------
		//test doesFiniteRayHit query
		//------------------------------------------------------------------------
		const float length = Random::unit() * 7.0f;
		bool hit1 = obtree.doesFiniteRayHitAnything(ray, length, context, *objecttree_context);
		bool hit2 = obtree.allObjectsDoesFiniteRayHitAnything(ray, length, context, *objecttree_context);
		assert(hit1 == hit2);
	}


	for(int i=0; i<(int)tritrees.size(); ++i)
	{
		delete tritrees[i];
	}


	}*/
}





void ObjectTreeTest::doSpeedTest()
{
	MTwister rng(1);

	ObjectTree ob_tree;

	/// Add some random spheres ////
	const int N = 1000;
	for(int i=0; i<N; ++i)
	{
		Object* ob = new Object(new RaySphere(Vec3d(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()), rng.unitRandom() * 0.05), Vec3d(0,0,0), Matrix3d::identity());
		ob->build();
		ob_tree.insertObject(ob); 
	}
	ob_tree.build();

	//ob_tree.printTree(0, 0, std::cout);
	ObjectTreeStats stats;
	ob_tree.getTreeStats(stats);

	TriTreePerThreadData tritree_context;
	ObjectTreePerThreadData* obtree_context = ob_tree.allocContext();

	{
	Timer testtimer;//start timer
	const int NUM_ITERS = 10000000;

	/// Do some random traces through the tree ///
	for(int i=0; i<NUM_ITERS; ++i)
	{
		const SSE_ALIGN Ray ray(
			(Vec3d(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()) - Vec3d(0.2, 0.2, 0.2)) * 1.4,
			normalise(Vec3d(Vec3d(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()) - Vec3d(0.5, 0.5, 0.5)))
			);

		//ray.buildRecipRayDir();

		//------------------------------------------------------------------------
		//do traceRay() test
		//------------------------------------------------------------------------
		HitInfo hitinfo;
		js::ObjectTree::INTERSECTABLE_TYPE* hitob;
		const double t = ob_tree.traceRay(ray, tritree_context, *obtree_context, hitob, hitinfo);
	}

	const double traces_per_sec = (double)NUM_ITERS / testtimer.getSecondsElapsed();
	printVar(traces_per_sec);

	}

	{
	Timer testtimer;//start timer
	const int NUM_ITERS = 10000000;

	/// Do some random traces through the tree ///
	for(int i=0; i<NUM_ITERS; ++i)
	{
		const SSE_ALIGN Ray ray(
			(Vec3d(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()) - Vec3d(0.2, 0.2, 0.2)) * 1.4,
			normalise(Vec3d(Vec3d(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()) - Vec3d(0.5, 0.5, 0.5)))
			);

		//ray.buildRecipRayDir();

		//------------------------------------------------------------------------
		//Do a doesFiniteRayHitAnything() test
		//------------------------------------------------------------------------
		const double len = rng.unitRandom() * 1.5;
		const bool a = ob_tree.doesFiniteRayHitAnything(ray, len, tritree_context, *obtree_context);
	}

	const double traces_per_sec = (double)NUM_ITERS / testtimer.getSecondsElapsed();
	printVar(traces_per_sec);

	}
}

void ObjectTreeTest::instancedMeshSpeedTest()
{

	conPrint("ObjectTreeTest::instancedMeshSpeedTest()");

	MTwister rng(1);

	//------------------------------------------------------------------------
	//load bunny mesh
	//------------------------------------------------------------------------
	CSModelLoader model_loader;
	RayMesh raymesh("raymesh", false);
	try
	{
		model_loader.streamModel("D:\\programming\\models\\bunny\\reconstruction\\bun_zipper.ply", raymesh, 1.0);
	}
	catch(CSModelLoaderExcep& e)
	{
		::fatalError(e.what());
	}

	//------------------------------------------------------------------------
	//insert random instances
	//------------------------------------------------------------------------
	raymesh.build();

	ObjectTree ob_tree;

	for(int i=0; i<200; ++i)
	{
		Matrix3d rot = Matrix3d::identity();
		rot.rotAroundAxis(normalise(Vec3d(rng.unitRandom(), rng.unitRandom(), rng.unitRandom())), rng.unitRandom() * 6.0);

		rot.scale(0.3);

		Object* object = new Object(&raymesh, Vec3d(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()), rot);
		object->build();

		ob_tree.insertObject(object);
	}

	//------------------------------------------------------------------------
	//compile tree
	//------------------------------------------------------------------------
	ob_tree.build();

	ObjectTreeStats stats;
	ob_tree.getTreeStats(stats);

	TriTreePerThreadData tritree_context;
	ObjectTreePerThreadData* obtree_context = ob_tree.allocContext();


	//------------------------------------------------------------------------
	//do traces
	//------------------------------------------------------------------------
	{
	Timer testtimer;//start timer
	const int NUM_ITERS = 3000000;
	int num_hits = 0;

	/// Do some random traces through the tree ///
	for(int i=0; i<NUM_ITERS; ++i)
	{
		const Vec3d start(rng.unitRandom(), rng.unitRandom(), rng.unitRandom());
		const Vec3d end(rng.unitRandom(), rng.unitRandom(), rng.unitRandom());

		const SSE_ALIGN Ray ray(
			start, 
			normalise(end - start)
			);

		//ray.buildRecipRayDir();

		//------------------------------------------------------------------------
		//Do a doesFiniteRayHitAnything() test
		//------------------------------------------------------------------------
		const double len = start.getDist(end);//rng.unitRandom() * 1.5;
		const bool a = ob_tree.doesFiniteRayHitAnything(ray, len, tritree_context, *obtree_context);
		if(a)
			num_hits++;
	}

	const double frac_hit = (double)num_hits / (double)NUM_ITERS;
	const double traces_per_sec = (double)NUM_ITERS / testtimer.getSecondsElapsed();
	printVar(testtimer.getSecondsElapsed());
	printVar(frac_hit);
	printVar(traces_per_sec);

	}

}


} //end namespace js






