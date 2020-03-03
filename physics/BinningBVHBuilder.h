/*=====================================================================
BinningBVHBuilder.h
-------------------
Copyright Glare Technologies Limited 2018 -
Generated at Tue Apr 27 15:25:47 +1200 2010
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


struct BinningBVHBuildStats
{
	BinningBVHBuildStats();
	void accumStats(const BinningBVHBuildStats& other);

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


struct BinningOb
{
	// Index of the source object/aabb is stored in aabb.min_[3].
	js::AABBox aabb;

	int getIndex() const
	{
		return bitcastToVec4i(aabb.min_)[3];
	}

	void setIndex(int index)
	{
		std::memcpy(&aabb.min_.x[3], &index, 4);
	}
};


struct BinningResultChunk
{
	GLARE_ALIGNED_16_NEW_DELETE

	const static size_t MAX_RESULT_CHUNK_SIZE = ((1 << 16) / sizeof(ResultNode)) - 1; // We want the size of BinningResultChunk to be just under 1 << 16.  (Set nodes size to leave room other member vars)
	
	ResultNode nodes[MAX_RESULT_CHUNK_SIZE];

	size_t size; // Number of ResultNodes written to nodes array.
	size_t chunk_offset; // Number of previous result chunks * MAX_RESULT_CHUNK_SIZE.
};


static_assert(sizeof(BinningResultChunk) <= (1 << 16), "sizeof(BinningResultChunk) <= (1 << 16)");


struct BinningPerThreadTempInfo
{
	BinningBVHBuildStats stats;

	BinningResultChunk* result_chunk;
};


/*=====================================================================
BinningBVHBuilder
-------------------
Multi-threaded SAH BVH builder.
=====================================================================*/
class BinningBVHBuilder : public BVHBuilder
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	// leaf_num_object_threshold - if there are <= leaf_num_object_threshold objects assigned to a subtree, a leaf will be made out of them.  Should be >= 1.
	// max_num_objects_per_leaf - maximum num objects per leaf node.  Should be >= leaf_num_object_threshold.
	// intersection_cost - cost of ray-object intersection for SAH computation.  Relative to traversal cost which is assumed to be 1.
	BinningBVHBuilder(int leaf_num_object_threshold, int max_num_objects_per_leaf, float intersection_cost,
		const int num_objects
	);
	~BinningBVHBuilder();

	inline void setObjectAABB(size_t ob_i, const js::AABBox& aabb); // ob_i should be < num_objects constructor arg.

	virtual void build(
		Indigo::TaskManager& task_manager,
		PrintOutput& print_output, 
		bool verbose, 
		js::Vector<ResultNode, 64>& result_nodes_out
	);

	virtual const js::AABBox getRootAABB() const { return root_aabb; }

	const BVHBuilder::ResultObIndicesVec& getResultObjectIndices() const { return result_indices; }

	int getMaxLeafDepth() const { return stats.max_leaf_depth; } // Root node is considered to have depth 0.

	static void printResultNode(const ResultNode& result_node);
	static void printResultNodes(const js::Vector<ResultNode, 64>& result_nodes);

	friend class BinningBuildSubtreeTask;

private:
	BinningResultChunk* allocNewResultChunk();

	// Assumptions: root node for subtree is already created and is at node_index
	void doBuild(
		BinningPerThreadTempInfo& thread_temp_info,
		const js::AABBox& aabb,
		const js::AABBox& centroid_aabb,
		uint32 node_index,
		int left,
		int right,
		int depth,
		BinningResultChunk* result_chunk
	);

	js::AABBox root_aabb;
	js::AABBox root_centroid_aabb;

	js::Vector<BinningOb, 64> objects;
	int m_num_objects;
	std::vector<BinningPerThreadTempInfo> per_thread_temp_info;

	std::vector<BinningResultChunk*> result_chunks;
	Mutex result_chunks_mutex;

	Indigo::TaskManager* local_task_manager;
	Indigo::TaskManager* task_manager;
	int leaf_num_object_threshold; 
	int max_num_objects_per_leaf;
	float intersection_cost; // Relative to BVH node traversal cost.

	js::Vector<uint32, 16> result_indices;
public:
	int new_task_num_ob_threshold;

	BinningBVHBuildStats stats;

	double split_search_time;
	double partition_time;
};


void BinningBVHBuilder::setObjectAABB(size_t ob_i, const js::AABBox& aabb)
{
	root_aabb.enlargeToHoldAABBox(aabb);
	root_centroid_aabb.enlargeToHoldPoint(aabb.centroid());

	objects[ob_i].aabb = aabb;
	objects[ob_i].setIndex((int)ob_i);
}
