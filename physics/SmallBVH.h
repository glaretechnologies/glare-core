/*=====================================================================
SmallBVH.h
----------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "jscol_Tree.h"
#include "jscol_aabbox.h"
#include "../maths/vec3.h"
#include "../maths/SSE.h"
#include "../utils/Vector.h"


class RayMesh;


namespace js
{


class SmallBVHNode
{
public:
	Vec4f x; // (left_min_x, right_min_x, left_max_x, right_max_x)
	Vec4f y; // (left_min_y, right_min_y, left_max_y, right_max_y)
	Vec4f z; // (left_min_z, right_min_z, left_max_z, right_max_z)

	int32 child[2];
	int32 padding[2]; // Pad to 64 bytes.
};


/*=====================================================================
SmallBVH
--------
A BVH that uses a small amount of memory, and is built quickly.

We will use this for GPU rendering:
We still need CPU mesh tracing (currently) for a few things -
* We want a good default target point to rotate around (MainWindow::startRendering())
* trace ray to get focus distance if autofocus is enabled
* World::getContainingMedia() for emitter (16 probe paths per light object)
* World::getContainingMedia() for camera (32 probe paths)
Solution: Use some kind of low-memory acceleration structure for this purpose.
=====================================================================*/
class SmallBVH : public Tree
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	SmallBVH(const RayMesh* const raymesh);
	virtual ~SmallBVH();

	virtual void build(PrintOutput& print_output, bool verbose, Indigo::TaskManager& task_manager); // throws Indigo::Exception

	virtual DistType traceRay(const Ray& ray, ThreadContext& thread_context, HitInfo& hitinfo_out) const;
	virtual const js::AABBox& getAABBoxWS() const;

	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, std::vector<DistanceHitInfo>& hitinfos_out) const;

	virtual void printStats() const {}
	virtual void printTraceStats() const {}
	virtual size_t getTotalMemUsage() const;

private:
	AABBox root_aabb; // AABB of whole thing
	js::Vector<SmallBVHNode, 64> nodes; // Nodes of the tree.
	js::Vector<uint32, 64> leaf_tri_indices; // Indices into the raymesh triangles array.
	const RayMesh* const raymesh;
	int32 root_node_index;
};


} //end namespace js