/*=====================================================================
NBVH.cpp
--------
File created by ClassTemplate on Sun Oct 26 01:59:14 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "NBVH.h"


#if 0


#include "jscol_aabbox.h"
#include "../indigo/globals.h"
#include "../utils/Timer.h"
#include "../simpleraytracer/raymesh.h"
#include "../utils/StringUtils.h"

namespace js
{


NBVH::NBVH(RayMesh* raymesh_)
:	raymesh(raymesh_),
	num_intersect_tris(0),
	intersect_tris(NULL),
	root_aabb(NULL),
	tri_aabbs(NULL)
{
	assert(raymesh);

	num_intersect_tris = 0;
	intersect_tris = NULL;

	root_aabb = (js::AABBox*)SSE::alignedSSEMalloc(sizeof(AABBox));
	new(root_aabb) AABBox(Vec3f(0,0,0), Vec3f(0,0,0));
}


NBVH::~NBVH()
{
	SSE::alignedSSEFree(root_aabb);
	SSE::alignedSSEFree(intersect_tris);
	intersect_tris = NULL;
}


const Vec3f& NBVH::triVertPos(unsigned int tri_index, unsigned int vert_index_in_tri) const
{
	return raymesh->triVertPos(tri_index, vert_index_in_tri);
}


unsigned int NBVH::numTris() const
{
	return raymesh->getNumTris();
}


void NBVH::build()
{
	conPrint("\tNBVH::build()");
	Timer buildtimer;

	//------------------------------------------------------------------------
	//calc root node's aabbox
	//NOTE: could do this faster by looping over vertices instead.
	//------------------------------------------------------------------------
	conPrint("\tCalcing root AABB.");
	root_aabb->min_ = triVertPos(0, 0);
	root_aabb->max_ = triVertPos(0, 0);
	for(unsigned int i=0; i<numTris(); ++i)
	{
		root_aabb->enlargeToHoldPoint(triVertPos(i, 0));
		root_aabb->enlargeToHoldPoint(triVertPos(i, 1));
		root_aabb->enlargeToHoldPoint(triVertPos(i, 2));
	}
	conPrint("\tdone.");
	conPrint("\tRoot AABB min: " + root_aabb->min_.toString());
	conPrint("\tRoot AABB max: " + root_aabb->max_.toString());

	{
	std::vector<TRI_INDEX> tris(numTris());
	for(unsigned int i=0; i<numTris(); ++i)
		tris[i] = i;

	// Build tri AABBs
	SSE::alignedSSEArrayMalloc(numTris(), tri_aabbs);

	for(unsigned int i=0; i<numTris(); ++i)
	{
		tri_aabbs[i].min_ = tri_aabbs[i].max_ = triVertPos(i, 0);
		tri_aabbs[i].enlargeToHoldPoint(triVertPos(i, 1));
		tri_aabbs[i].enlargeToHoldPoint(triVertPos(i, 2));
	}

	build_nodes.resize(1);
	build_nodes[0].min = root_aabb->min_;
	build_nodes[0].max = root_aabb->max_;
	doBuild(tris, 0, numTris(), 0, 0);
	}

	//------------------------------------------------------------------------
	//alloc intersect tri array
	//------------------------------------------------------------------------
	conPrint("\tAllocing and copying intersect triangles...");
	this->num_intersect_tris = numTris();
	if(::atDebugLevel(DEBUG_LEVEL_VERBOSE))
		conPrint("\tintersect_tris mem usage: " + ::getNiceByteSize(num_intersect_tris * sizeof(INTERSECT_TRI_TYPE)));

	SSE::alignedSSEArrayMalloc(num_intersect_tris, intersect_tris);

	// Copy tri data.
	for(unsigned int i=0; i<num_intersect_tris; ++i)
		intersect_tris[i].set(triVertPos(i, 0), triVertPos(i, 1), triVertPos(i, 2));
	conPrint("\tdone.");

	SSE::alignedSSEArrayFree(tri_aabbs);


	conPrint("\tBuild Stats:");
	conPrint("\t\ttotal leafgeom size: " + ::toString((unsigned int)leafgeom.size()) + " (" + ::getNiceByteSize(leafgeom.size() * sizeof(TRI_INDEX)) + ")");
	conPrint("\t\ttotal nodes used: " + ::toString((unsigned int)nodes.size()) + " (" + ::getNiceByteSize(nodes.size() * sizeof(js::NBVHNode)) + ")");

	conPrint("\tFinished building tree. (Build time: " + toString(buildtimer.getSecondsElapsed()) + "s)");	
}



/*
The triangles [left, right) are considered to belong to this node.
The AABB for this node (build_nodes[node_index]) should be set already.
*/
void NBVH::doBuild(std::vector<TRI_INDEX>& tris, int left, int right, unsigned int node_index, int depth)
{
	//assert(left >= 0 && left < (int)numTris() && right >= 0 && right < (int)numTris());
	assert(node_index < build_nodes.size());

	const int LEAF_NUM_TRI_THRESHOLD = 64;

	if(right - left <= LEAF_NUM_TRI_THRESHOLD)
	{
		//------------------------------------------------------------------------
		//Make a Leaf node
		//------------------------------------------------------------------------
		build_nodes[node_index].leaf = 1;

		// Add tris
		build_nodes[node_index].geometry_index = leafgeom.size();

		for(int i=left; i<right; ++i)
			leafgeom.push_back(tris[i]);

		build_nodes[node_index].num_geom = myMax(right - left, 0);
		return;
	}

	//------------------------------------------------------------------------
	//make interior node
	//------------------------------------------------------------------------
	build_nodes[node_index].leaf = 0;

	const SSE_ALIGN AABBox aabb(build_nodes[node_index].min, build_nodes[node_index].max);

	const unsigned int split_axis = aabb.longestAxis();
	const float split_val = 0.5f * (build_nodes[node_index].min[split_axis] + build_nodes[node_index].max[split_axis]);

	SSE_ALIGN AABBox neg_aabb(
		Vec3f(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()),
		Vec3f(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity())
		);

	SSE_ALIGN AABBox pos_aabb(
		Vec3f(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()),
		Vec3f(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity())
		);

	int d = right-1;
	int i = left;
	while(i <= d)
	{
		// Categorise triangle at i

		assert(i >= 0 && i < (int)numTris());
		assert(d >= 0 && d < (int)numTris());
		assert(tris[i] < numTris());

		const float tri_min = tri_aabbs[tris[i]].min_[split_axis];
		const float tri_max = tri_aabbs[tris[i]].max_[split_axis];
		if(tri_max - split_val >= split_val - tri_min)
		{
			// If mostly on positive side of split - categorise as R
			pos_aabb.enlargeToHoldAABBox(tri_aabbs[tris[i]]); // Update right AABB

			// Swap element at i with element at d
			mySwap(tris[i], tris[d]);

			assert(d >= 0);
			--d;
		}
		else
		{
			// Tri mostly on negative side of split - categorise as L
			neg_aabb.enlargeToHoldAABBox(tri_aabbs[tris[i]]); // Update left AABB
			++i;
		}
	}
	assert(i == d+1);

	const int num_in_left = (d - left) + 1;
	const int num_in_right = right - i;//+1;
	assert(num_in_left >= 0);
	assert(num_in_right >= 0);
	assert(num_in_left + num_in_right == right - left);
	
	const unsigned int left_child_index = nodes.size();
	const unsigned int right_child_index = left_child_index + 1;

	build_nodes[node_index].left_child_index = left_child_index;
	build_nodes[node_index].right_child_index = right_child_index;
	
	build_nodes.resize(nodes.size() + 2); // Reserve space for children

	// Recurse to build left subtree
	build_nodes[left_child_index].min = neg_aabb.min_;
	build_nodes[left_child_index].max = neg_aabb.max_;
	doBuild(tris, left, d + 1, left_child_index, depth+1);

	// Recurse to build right subtree
	build_nodes[right_child_index].min = pos_aabb.min_;
	build_nodes[right_child_index].max = pos_aabb.max_;
	doBuild(tris, i, right, right_child_index, depth+1);
}


double NBVH::traceRay(const Ray& ray, double ray_max_t, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const
{
	return -1.0;
}


void NBVH::getAllHits(const Ray& ray, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	hitinfos_out.resize(0);
}


const js::AABBox& NBVH::getAABBoxWS() const
{
	return *root_aabb;
}


const Vec3f NBVH::triGeometricNormal(unsigned int tri_index) const //slow
{
	return intersect_tris[tri_index].getNormal();
}


} //end namespace js


#endif




