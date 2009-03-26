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
#include "MollerTrumboreTri.h"
#include "../indigo/globals.h"
#include "../utils/stringutils.h"
#include "../indigo/StandardPrintOutput.h"


namespace js
{

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

	UnionVec4 u, v, t, hit;
	MollerTrumboreTri::intersectTris(&ray,
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
			Vec3d(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()),
			normalise(Vec3d(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()))
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
			Vec3d(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()),
			normalise(Vec3d(rng.unitRandom(), rng.unitRandom(), rng.unitRandom()))
			);

		testIndividualBadouelIntersection(tri, ray);
	}


	conPrint("testBadouelTriIntersection() Done");
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
	StandardPrintOutput print_output;
	raymesh.build(
		".",
		settings,
		print_output
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

	StandardPrintOutput print_output;
	RendererSettings settings;
	settings.cache_trees = false;
	raymesh.build(
		".",
		settings,
		print_output
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

	StandardPrintOutput print_output;
	RendererSettings settings;
	settings.cache_trees = false;
	raymesh.build(
		".",
		settings,
		print_output
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
	StandardPrintOutput print_output;
	trees.back()->build(print_output);

	trees.push_back(new BIHTree(&raymesh));
	trees.back()->build(print_output);

	trees.push_back(new BVH(&raymesh));
	trees.back()->build(print_output);

	// Check AABBox
	SSE_ALIGN AABBox box = trees[0]->getAABBoxWS();
	for(unsigned int i=0; i<trees.size(); ++i)
		testAssert(trees[i]->getAABBoxWS() == box);

	ThreadContext thread_context(1, 0);

	//------------------------------------------------------------------------
	//compare tests against all tris with tests against the tree
	//------------------------------------------------------------------------
	const int NUM_RAYS = 10000;
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

		HitInfo hitinfo;
		js::TriTreePerThreadData tree_context;

		const double dist = trees[0]->traceRay(ray, max_t, thread_context, tree_context, NULL, hitinfo);

		HitInfo all_tris_hitinfo;
		const double alltrisdist = dynamic_cast<KDTree*>(trees[0])->traceRayAgainstAllTris(ray, max_t, all_tris_hitinfo);
		testAssert(dist == alltrisdist);

		if(dist >= 0.0) // If hit
		{
			testAssert(hitinfo.sub_elem_index == all_tris_hitinfo.sub_elem_index);
			testAssert(::epsEqual(hitinfo.sub_elem_coords.x, all_tris_hitinfo.sub_elem_coords.x));
			testAssert(::epsEqual(hitinfo.sub_elem_coords.y, all_tris_hitinfo.sub_elem_coords.y));
		}

		for(unsigned int t=0; t<trees.size(); ++t)
		{
			HitInfo hitinfo_;
			const double dist_ = trees[t]->traceRay(ray, max_t, thread_context, tree_context, NULL, hitinfo_);

			if(dist >= 0.0 || dist_ >= 0.0)
			{
				testAssert(hitinfo.sub_elem_index == hitinfo_.sub_elem_index);
				testAssert(::epsEqual(dist, dist_, 0.0001));

				testAssert(::epsEqual(hitinfo.sub_elem_coords.x, hitinfo_.sub_elem_coords.x, (HitInfo::SubElemCoordsRealType)0.0001));
				testAssert(::epsEqual(hitinfo.sub_elem_coords.y, hitinfo_.sub_elem_coords.y, (HitInfo::SubElemCoordsRealType)0.0001));
			}
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
			testAssert(epsEqual(hitinfos[0].sub_elem_coords, hitinfo.sub_elem_coords));
		}

		// Do a check against all tris
		std::vector<DistanceHitInfo> hitinfos_d;
		dynamic_cast<KDTree*>(trees[0])->getAllHitsAllTris(ray, hitinfos_d);
		std::sort(hitinfos_d.begin(), hitinfos_d.end(), distanceHitInfoComparisonPred); // Sort hits

		// Compare results
		testAssert(hitinfos.size() == hitinfos_d.size());
		for(unsigned int z=0; z<hitinfos.size(); ++z)
		{
			testAssert(hitinfos[z].dist == hitinfos_d[z].dist);
			testAssert(hitinfos[z].sub_elem_index == hitinfos_d[z].sub_elem_index);
			testAssert(hitinfos[z].sub_elem_coords == hitinfos_d[z].sub_elem_coords);
		}

		//------------------------------------------------------------------------
		//test getAllHits() on other trees
		//------------------------------------------------------------------------
		for(unsigned int t=0; t<trees.size(); ++t)
		{
			std::vector<DistanceHitInfo> hitinfos_other;
			trees[t]->getAllHits(ray, thread_context, tree_context, NULL, hitinfos_other);
			std::sort(hitinfos_other.begin(), hitinfos_other.end(), distanceHitInfoComparisonPred); // Sort hits

			// Compare results
			testAssert(hitinfos.size() == hitinfos_other.size());
			for(unsigned int z=0; z<hitinfos.size(); ++z)
			{
				testAssert(::epsEqual(hitinfos[z].dist, hitinfos_other[z].dist, 0.0001));
				testAssert(hitinfos[z].sub_elem_index == hitinfos_other[z].sub_elem_index);
				//testAssert(::epsEqual(hitinfos[z].sub_elem_coords, hitinfos_other[z].sub_elem_coords));
				testAssert(::epsEqual(hitinfos[z].sub_elem_coords.x, hitinfos_other[z].sub_elem_coords.x, (HitInfo::SubElemCoordsRealType)0.001));
				testAssert(::epsEqual(hitinfos[z].sub_elem_coords.y, hitinfos_other[z].sub_elem_coords.y, (HitInfo::SubElemCoordsRealType)0.001));
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
	testBadouelTriIntersection();

	testTriangleIntersection();

	doEdgeCaseTests();

	conPrint("TreeTest::doTests()");

	MTwister rng(1);

	///////////////////////////////////////
	{
	// Load tricky mesh from disk
	const std::string MODEL_PATH = "../testfiles/bug-2.igmesh";
	CSModelLoader model_loader;
	RayMesh raymesh("tricky", false);
	try
	{
		model_loader.streamModel(MODEL_PATH, raymesh, 1.0);
	}
	catch(CSModelLoaderExcep& e)
	{
		::fatalError(e.what());
	}
	testTree(rng, raymesh);
	}
	/////////////////////////////////////////////



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




void TreeTest::doSpeedTest(int treetype)
{
	const std::string BUNNY_PATH = "../testfiles/bun_zipper.ply";
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
	if(treetype == 0)
		settings.bih_tri_threshold = 100000000;
	else
		settings.bih_tri_threshold = 0;

	StandardPrintOutput print_output;
	raymesh.build(
		".", // base indigo dir path
		settings,
		print_output
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
	StandardPrintOutput print_output;
	raymesh.build(
		".", // base indigo dir path
		settings,
		print_output
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






