/*=====================================================================
EmbreeBVHBuilder.h
------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "BVHBuilder.h"
#include "jscol_aabbox.h"
#include "../maths/vec3.h"
#include "../utils/Platform.h"
#include "../utils/Vector.h"
#include "../utils/Mutex.h"
#include "../utils/IndigoAtomic.h"
#include <vector>
namespace js { class AABBox; }
namespace Indigo { class TaskManager; }
class PrintOutput;
class RayMeshTriangle;
namespace EmbreeGlare {
	struct RTCDeviceTy;
	struct RTCSceneTy;
	struct RTCFilterFunctionNArguments;
}
using namespace EmbreeGlare;


struct ResultInteriorNode
{
	inline bool leftChildIsInterior()  const { return left_num_prims == -1;  }
	inline bool rightChildIsInterior() const { return right_num_prims == -1; }

	js::AABBox child_aabbs[2];
	int left;  // If left child is an interior node, then the index of the left child, otherwise if the left child is a leaf, this is the begin index for the left child primitives.
	int right; // If right child is an interior node, then the index of the right child, otherwise if the right child is a leaf, this is the begin index for the right child primitives.

	int left_num_prims; // If left child is an interior node, -1, otherwise number of leaves in left child leaf node.
	int right_num_prims;
};


/*=====================================================================
EmbreeBVHBuilder
----------------
=====================================================================*/
class EmbreeBVHBuilder
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	// leaf_num_object_threshold - if there are <= leaf_num_object_threshold objects assigned to a subtree, a leaf will be made out of them.  Should be >= 1.
	// max_num_objects_per_leaf - maximum num objects per leaf node.  Should be >= leaf_num_object_threshold.
	// intersection_cost - cost of ray-object intersection for SAH computation.  Relative to traversal cost which is assumed to be 1.
	EmbreeBVHBuilder(
		bool do_high_quality_build, // If true, does a spatial BVH build (SBVH).
		int leaf_num_object_threshold, int max_num_objects_per_leaf, float intersection_cost, 
		const BVHBuilderTri* triangles,
		const int num_objects
	);
	~EmbreeBVHBuilder();

	void doBuild(
		Indigo::TaskManager& task_manager,
		ShouldCancelCallback& should_cancel_callback,
		PrintOutput& print_output,
		bool verbose,
		js::Vector<ResultInteriorNode, 64>& result_nodes_out
	);

	//virtual const js::AABBox getRootAABB() const { assert(0); return root_aabb; } // root AABB will have been computed after build() has been called. 

	virtual const BVHBuilder::ResultObIndicesVec& getResultObjectIndices() const { return result_indices; }

	int getMaxLeafDepth() const { return max_leaf_depth; } // Root node is considered to have depth 0.

	static void test();
//private:
	//js::AABBox root_aabb;
public:
	const BVHBuilderTri* triangles;
	int m_num_objects;
private:

	bool do_high_quality_build;
	int max_leaf_depth;
	int leaf_num_object_threshold; 
	int max_num_objects_per_leaf;
	float intersection_cost; // Relative to BVH node traversal cost.
	float recip_root_node_aabb_area;

	js::Vector<uint32, 16> result_indices;

	ShouldCancelCallback* should_cancel_callback;

	RTCDeviceTy* embree_device;
public:
	IndigoAtomic embree_mem_usage;
	IndigoAtomic embree_max_mem_usage;
};
