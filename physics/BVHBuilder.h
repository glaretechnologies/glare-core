/*=====================================================================
BVHBuilder.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Tue Apr 27 15:25:47 +1200 2010
=====================================================================*/
#pragma once


#include <vector>
#include "../utils/platform.h"
#include "../maths/vec3.h"
namespace js { class AABBox; }
class PrintOutput;


class BVHBuilderCallBacks
{
public:
	virtual ~BVHBuilderCallBacks(){}

	virtual uint32 createNode() = 0;
	virtual void markAsInteriorNode(int node_index, int left_child_index, int right_child_index, const js::AABBox& left_aabb, const js::AABBox& right_aabb, int parent_index, bool is_left_child) = 0;
	virtual void markAsLeafNode(int node_index, const std::vector<uint32>& objects, int begin, int end, int parent_index, bool is_left_child) = 0;
};


/*=====================================================================
BVHBuilder
-------------------

=====================================================================*/
class BVHBuilder
{
public:
	BVHBuilder();
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
	/*
	Assumptions: root node for subtree is already created and is at node_index
	*/
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

	const js::AABBox* aabbs;
	std::vector<Vec3f> centers;
	std::vector<uint32> objects[3];
	std::vector<uint32> temp[2];
	std::vector<float> object_max;

	/// build stats ///
	int num_maxdepth_leaves;
	int num_under_thresh_leaves;
	int num_cheaper_nosplit_leaves;
	int num_could_not_split_leaves;
	int num_leaves;
	int max_num_tris_per_leaf;
	int leaf_depth_sum;
	int max_leaf_depth;
};
