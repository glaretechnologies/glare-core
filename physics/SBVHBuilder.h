/*=====================================================================
SBVHBuilder.h
-------------
Copyright Glare Technologies Limited 2017 -
=====================================================================*/
#pragma once


#include "BVHBuilder.h"
#include "jscol_aabbox.h"
#include "../maths/vec3.h"
#include "../utils/Platform.h"
#include "../utils/Vector.h"
#include "../utils/Mutex.h"
#include "../utils/IndigoAtomic.h"
#include "../utils/BitVector.h"
#include <vector>
namespace js { class AABBox; }
namespace Indigo { class TaskManager; }
class PrintOutput;
class RayMeshTriangle;


struct SBVHBuildStats
{
	SBVHBuildStats();
	void accumStats(const SBVHBuildStats& other);

	int num_maxdepth_leaves;
	int num_under_thresh_leaves;
	int num_cheaper_nosplit_leaves;
	int num_could_not_split_leaves;
	int num_tris_in_leaves;
	int num_leaves;
	int max_num_tris_per_leaf;
	int leaf_depth_sum;
	int max_leaf_depth;
	int num_interior_nodes;
	int num_arbitrary_split_leaves;
	int num_object_splits;
	int num_spatial_splits;
	int num_hit_capacity_splits;
};


struct SBVHOb
{
	GLARE_ALIGNED_16_NEW_DELETE

	// Index of the source object/aabb is stored in aabb.min_[3].
	js::AABBox aabb;

	int getIndex() const
	{
		return bitcastToVec4i(aabb.min_)[3]; // This form seems slightly faster.
		//return elem<3>(bitcastToVec4i(aabb.min_));
	}

	void setIndex(int index)
	{
		std::memcpy(&aabb.min_.x[3], &index, 4);
	}

	uint32 getUnsplit() const
	{
		return bitcastToVec4i(aabb.max_)[3];
		//return elem<3>(bitcastToVec4i(aabb.max_));
	}

	void setUnsplit(uint32 val)
	{
		std::memcpy(&aabb.max_.x[3], &val, 4);
	}
};


struct SBVHResultChunk
{
	GLARE_ALIGNED_16_NEW_DELETE

	const static size_t MAX_RESULT_CHUNK_SIZE = 600;

	ResultNode nodes[MAX_RESULT_CHUNK_SIZE];

	size_t size; // Number of valid elements in 'nodes'.
	size_t chunk_offset; // Global offset of nodes[0] amongst all result chunks.
};


struct SBVHLeafResultChunk
{
	GLARE_ALIGNED_16_NEW_DELETE

	const static size_t MAX_RESULT_CHUNK_SIZE = 600;

	int leaf_obs[MAX_RESULT_CHUNK_SIZE];

	size_t size; // Number of valid elements in 'nodes'.
	size_t chunk_offset;// Global offset of leaf_obs[0] amongst all leaf result chunks.
};


struct SBVHPerThreadTempInfo
{
	SBVHBuildStats stats;

	size_t dataSizeBytes() const;

	SBVHResultChunk* result_chunk;
	SBVHLeafResultChunk* leaf_result_chunk;
};


/*=====================================================================
SBVHBuilder
-------------------
See "Spatial Splits in Bounding Volume Hierarchies"
http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.550.6560&rep=rep1&type=pdf
Also
https://github.com/embree/embree/blob/master/kernels/builders/heuristic_spatial.h
=====================================================================*/
class SBVHBuilder : public BVHBuilder
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	typedef BVHBuilderTri SBVHTri;

	// leaf_num_object_threshold - if there are <= leaf_num_object_threshold objects assigned to a subtree, a leaf will be made out of them.  Should be >= 1.
	// max_num_objects_per_leaf - maximum num objects per leaf node.  Should be >= leaf_num_object_threshold.
	// intersection_cost - cost of ray-object intersection for SAH computation.  Relative to traversal cost which is assumed to be 1.
	SBVHBuilder(int leaf_num_object_threshold, int max_num_objects_per_leaf, float intersection_cost, 
		const SBVHTri* triangles,
		const int num_objects
	);
	~SBVHBuilder();

	virtual void build(
		Indigo::TaskManager& task_manager,
		ShouldCancelCallback& should_cancel_callback,
		PrintOutput& print_output, 
		bool verbose, 
		js::Vector<ResultNode, 64>& result_nodes_out
	);

	virtual const js::AABBox getRootAABB() const { return root_aabb; } // root AABB will have been computed after build() has been called. 

	virtual const BVHBuilder::ResultObIndicesVec& getResultObjectIndices() const { return result_indices; }

	int getMaxLeafDepth() const { return stats.max_leaf_depth; } // Root node is considered to have depth 0.

	friend class SBVHBuildSubtreeTask;

	static void test();

	typedef std::vector<SBVHOb> ObjectVecType;
private:
	SBVHResultChunk* allocNewResultChunk();
	SBVHLeafResultChunk* allocNewLeafResultChunk();

	// Assumptions: root node for subtree is already created and is at node_index
	void doBuild(
		SBVHPerThreadTempInfo& thread_temp_info,
		const js::AABBox& aabb,
		const js::AABBox& centroid_aabb,
		uint32 node_index,
		int begin,
		int end,
		int capacity,
		int depth,
		SBVHResultChunk* result_chunk
	);

	inline void markNodeAsLeaf(SBVHPerThreadTempInfo& thread_temp_info, const js::AABBox& aabb, uint32 node_index, /*const std::vector<SBVHOb>& obs, */int begin, int end, int depth, SBVHResultChunk* node_result_chunk);

	js::AABBox root_aabb;
	const BVHBuilderTri* triangles;
	int m_num_objects;
	ObjectVecType top_level_objects;
	ObjectVecType temp_obs;
	std::vector<SBVHPerThreadTempInfo> per_thread_temp_info;

	js::Vector<SBVHResultChunk*, 16> result_chunks;
	Mutex result_chunks_mutex;

	js::Vector<SBVHLeafResultChunk*, 16> leaf_result_chunks;
	Mutex leaf_result_chunks_mutex;

	Indigo::TaskManager* task_manager;
	int leaf_num_object_threshold; 
	int max_num_objects_per_leaf;
	float intersection_cost; // Relative to BVH node traversal cost.
	float recip_root_node_aabb_area;

	js::Vector<uint32, 16> result_indices;

	ShouldCancelCallback* should_cancel_callback;
public:
	int axis_parallel_num_ob_threshold;
	int new_task_num_ob_threshold;

	SBVHBuildStats stats;

	double split_search_time;
	double partition_time;
};
