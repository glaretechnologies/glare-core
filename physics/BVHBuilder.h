/*=====================================================================
BVHBuilder.h
-------------------
Copyright Glare Technologies Limited 2017 -
Generated at Tue Apr 27 15:25:47 +1200 2010
=====================================================================*/
#pragma once


#include "../physics/jscol_aabbox.h"
#include "../maths/vec3.h"
#include "../utils/Platform.h"
#include "../utils/Vector.h"
#include "../utils/Mutex.h"
#include "../utils/IndigoAtomic.h"
#include <vector>
namespace js { class AABBox; }
namespace Indigo { class TaskManager; }
class PrintOutput;


struct ResultNode
{
	js::AABBox left_aabb;  // if interior
	js::AABBox right_aabb;  // if interior
	int left;  // if interior, this is the index of the left child node.  Will be zero if this is the root node of a different chunk.  If Leaf, this is the object begin index.
	int right; // if interior, this is the index of the right child node.  Will be zero if this is the root node of a different chunk.  If Leaf, this is the object end index.
	
	int right_child_chunk_index; // if interior: -1 if right child is in the same chunk as this node.
	bool interior;


	inline void operator = (const ResultNode& other)
	{
		left_aabb = other.left_aabb;
		right_aabb = other.right_aabb;
		_mm_store_ps((float*)this + 16, _mm_load_ps((float*)&other + 16));
	}
};


struct Ob
{
	// Index of the source object/aabb is stored in aabb.min_[3].
	js::AABBox aabb;
	//Vec3f centre;
	//int index;

	int getIndex() const
	{
		return bitcastToVec4i(aabb.min_)[3];
	}

	void setIndex(int index)
	{
		std::memcpy(&aabb.min_.x[3], &index, 4);
	}

	inline void operator = (const Ob& other)
	{
		aabb = other.aabb;
		//_mm_store_ps((float*)this + 8, _mm_load_ps((float*)&other + 8));
	}
};


struct PerThreadTempInfo
{
	js::Vector<ResultNode, 64> result_buf;

	size_t dataSizeBytes() const { return result_buf.dataSizeBytes(); }
};


struct PerAxisThreadTempInfo
{
	js::Vector<js::AABBox, 16> split_left_aabb;

	size_t dataSizeBytes() const { return split_left_aabb.dataSizeBytes(); }
};


struct ResultChunk
{
	int thread_index; // The index of the thread that computed this chunk.
	int offset; // Offset in thread's result_buf
	int size; // num nodes created by the task in thread's result_buf;
};


/*=====================================================================
BVHBuilder
-------------------
Multi-threaded SAH BVH builder.
=====================================================================*/
class BVHBuilder
{
public:
	// leaf_num_object_threshold - if there are <= leaf_num_object_threshold objects assigned to a subtree, a leaf will be made out of them.  Should be >= 1.
	// max_num_objects_per_leaf - maximum num objects per leaf node.
	// intersection_cost - cost of ray-object intersection for SAH computation.
	BVHBuilder(int leaf_num_object_threshold, int max_num_objects_per_leaf, float intersection_cost);
	~BVHBuilder();

	void build(
		Indigo::TaskManager& task_manager,
		const js::AABBox* aabbs,
		const int num_objects,
		PrintOutput& print_output, 
		bool verbose, 
		js::Vector<ResultNode, 64>& result_nodes_out
	);

	typedef js::Vector<uint32, 16> ResultObIndicesVec;
	const ResultObIndicesVec& getResultObjectIndices() const { return result_indices; }// { return objects[0]; }

	int getMaxLeafDepth() const { return max_leaf_depth; } // Root node is considered to have depth 0.

	static void printResultNodes(const js::Vector<ResultNode, 64>& result_nodes);

	friend class CentroidTask;
	friend class BuildSubtreeTask;
	friend class BestSplitSearchTask;
	friend class PartitionTask;
	friend class ConstructAxisObjectsTask;

private:
	// Assumptions: root node for subtree is already created and is at node_index
	void doBuild(
		PerThreadTempInfo& thread_temp_info,
		const js::AABBox& aabb,
		uint32 node_index,
		int left,
		int right,
		int depth,
		js::Vector<Ob, 64>* cur_objects,
		js::Vector<Ob, 64>* other_objects,
		ResultChunk& result_chunk
	);

	// We may get multiple objects with the same bounding box.
	// These objects can't be split in the usual way.
	// Also the number of such objects assigned to a subtree may be > max_num_objects_per_leaf.
	// In this case we will just divide the object list into two until the num per subtree is <= max_num_objects_per_leaf.
	void doArbitrarySplits(
		PerThreadTempInfo& thread_temp_info,
		const js::AABBox& aabb,
		uint32 node_index,
		int left, 
		int right, 
		int depth,
		js::Vector<Ob, 64>* cur_objects,
		js::Vector<Ob, 64>* other_objects,
		ResultChunk& result_chunk
	);



	const js::AABBox* aabbs;
	js::Vector<Ob, 64> objects_a[3];
	js::Vector<Ob, 64> objects_b[3];
	std::vector<PerThreadTempInfo> per_thread_temp_info;
	std::vector<PerAxisThreadTempInfo> per_axis_thread_temp_info;

	js::Vector<js::AABBox, 16> split_left_aabb;

	IndigoAtomic next_result_chunk; // Index of next free result chunk
	js::Vector<ResultChunk, 16> result_chunks;

	Indigo::TaskManager* task_manager;
	int leaf_num_object_threshold; 
	int max_num_objects_per_leaf;
	float intersection_cost; // Relative to BVH node traversal cost.

	Indigo::TaskManager* local_task_manager;

	js::Vector<uint32, 16> result_indices;
public:
	int axis_parallel_num_ob_threshold;
	int new_task_num_ob_threshold;

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

	double split_search_time;
	double partition_time;
};
