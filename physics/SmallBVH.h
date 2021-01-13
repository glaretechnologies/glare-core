/*=====================================================================
SmallBVH.h
----------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "jscol_Tree.h"
#include "jscol_aabbox.h"
#include "../maths/vec3.h"
#include "../utils/MemAlloc.h"
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

The way we use a small amount of memory is to have a lot of triangles indices (up to 63) in each leaf.
This will reduce the total number of tree nodes.
In addition we don't store triangles explictly, but access triangle data via the raymesh pointer when doing ray/tri intersection.

We also use a binning builder for fast builds.
=====================================================================*/
class SmallBVH : public Tree
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	SmallBVH(const RayMesh* const raymesh);
	virtual ~SmallBVH();

	virtual void build(PrintOutput& print_output, ShouldCancelCallback& should_cancel_callback, glare::TaskManager& task_manager); // throws Indigo::Exception

	virtual DistType traceRay(const Ray& ray, HitInfo& hitinfo_out) const;
	virtual const js::AABBox& getAABBox() const;

	virtual void printStats() const {}
	virtual void printTraceStats() const {}
	virtual size_t getTotalMemUsage() const;

	static void test(bool comprehensive_tests);
private:
	AABBox root_aabb; // AABB of whole thing
	js::Vector<SmallBVHNode, 64> nodes; // Nodes of the tree.
	js::Vector<uint32, 64> leaf_tri_indices; // Indices into the raymesh triangles array.
	const RayMesh* const raymesh;
	int32 root_node_index;
};


} //end namespace js
