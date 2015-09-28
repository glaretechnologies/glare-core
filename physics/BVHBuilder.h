/*=====================================================================
BVHBuilder.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Tue Apr 27 15:25:47 +1200 2010
=====================================================================*/
#pragma once


#include "../utils/Platform.h"
#include "../maths/vec3.h"
#include <vector>
namespace js { class AABBox; }
class PrintOutput;


class BVHBuilderCallBacks
{
public:
	virtual ~BVHBuilderCallBacks(){}

	// Create a node, then return the index of the node.
	virtual uint32 createNode() = 0;

	// Mark the node as indexed by 'node_index' as an interior node.
	// Also return the possibly new index, if the node was not actually created yet (as it may have been a leaf)
	virtual int markAsInteriorNode(int node_index, int left_child_index, int right_child_index, const js::AABBox& left_aabb, const js::AABBox& right_aabb, int parent_index, bool is_left_child) = 0;

	virtual void markAsLeafNode(int node_index, const std::vector<uint32>& objects, int begin, int end, int parent_index, bool is_left_child) = 0;
};


/*=====================================================================
BVHBuilder
-------------------

=====================================================================*/
class BVHBuilder
{
public:
	friend class SortAxisTask;
	friend class CentroidTask;

	// leaf_num_object_threshold - if there are <= leaf_num_object_threshold objects assigned to a subtree, a leaf will be made out of them.
	// max_num_objects_per_leaf - maximum num objects per leaf node.
	// intersection_cost - cost of ray-object intersection for SAH computation.
	BVHBuilder(int leaf_num_object_threshold, int max_num_objects_per_leaf, float intersection_cost);
	~BVHBuilder();

	void build(
		const js::AABBox* aabbs,
		const int num_objects,
		PrintOutput& print_output, 
		bool verbose, 
		BVHBuilderCallBacks& callback
	);


	int getMaxLeafDepth() const { return max_leaf_depth; }

private:
	
	// Assumptions: root node for subtree is already created and is at node_index
	void doBuild(
		const js::AABBox& aabb,
		uint32 node_index, 
		int left, 
		int right, 
		int depth,
		uint32 parent_index,
		bool is_left_child,
		BVHBuilderCallBacks& callback
	);


	// We may get multiple objects with the same bounding box.
	// These objects can't be split in the usual way.
	// Also the number of such objects assigned to a subtree may be > max_num_objects_per_leaf.
	// In this case we will just divide the object list into two until the num per subtree is <= max_num_objects_per_leaf.
	void doArbitrarySplits(
		const js::AABBox& aabb,
		uint32 node_index,
		int left, 
		int right, 
		int depth,
		uint32 parent_index,
		bool is_left_child,
		BVHBuilderCallBacks& callback
	);

	const js::AABBox* aabbs;
	std::vector<Vec3f> centers;
	std::vector<uint32> objects[3];
	std::vector<uint32> temp[2];
	std::vector<float> object_max;

	int leaf_num_object_threshold; 
	int max_num_objects_per_leaf;
	float intersection_cost; // Relative to BVH node traversal cost.
public:
	/// build stats ///
	int num_maxdepth_leaves;
	int num_under_thresh_leaves;
	int num_cheaper_nosplit_leaves;
	int num_could_not_split_leaves;
	int num_leaves;
	int max_num_tris_per_leaf;
	int leaf_depth_sum;
	int max_leaf_depth;
	int num_interior_nodes;
	int num_arbitrary_split_leaves;
};
