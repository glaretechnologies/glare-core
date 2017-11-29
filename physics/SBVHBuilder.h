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
	int num_leaves;
	int max_num_tris_per_leaf;
	int leaf_depth_sum;
	int max_leaf_depth;
	int num_interior_nodes;
	int num_arbitrary_split_leaves;
	int num_object_splits;
	int num_spatial_splits;
};


struct SBVHOb
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


struct SBVHResultChunk
{
	INDIGO_ALIGNED_NEW_DELETE

	const static size_t MAX_RESULT_CHUNK_SIZE = 600;

	ResultNode nodes[MAX_RESULT_CHUNK_SIZE];

	size_t size;
	size_t chunk_offset;
};


struct SBVHLeafResultChunk
{
	//int thread_index; // The index of the thread that computed this chunk.
	//int offset; // Offset in thread's result_buf
	//int size; // num nodes created by the task in thread's result_buf;
	//int original_index; // Index of this chunk in result_chunks, before sorting.

	//uint64 sort_key;

	INDIGO_ALIGNED_NEW_DELETE

	const static size_t MAX_RESULT_CHUNK_SIZE = 600;

	int leaf_obs[MAX_RESULT_CHUNK_SIZE];

	size_t size;
	size_t chunk_offset;
};


struct SBVHPerThreadTempInfo
{
	SBVHBuildStats stats;

	size_t dataSizeBytes() const { return 0; }// result_buf.dataSizeBytes();

	std::vector<std::vector<SBVHOb> > left_obs;
	std::vector<std::vector<SBVHOb> > right_obs;

	std::vector<int> unsplit;

	SBVHResultChunk* result_chunk;
	SBVHLeafResultChunk* leaf_result_chunk;
};


struct SBVHTri
{
	Vec4f v[3];
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
	// leaf_num_object_threshold - if there are <= leaf_num_object_threshold objects assigned to a subtree, a leaf will be made out of them.  Should be >= 1.
	// max_num_objects_per_leaf - maximum num objects per leaf node.  Should be >= leaf_num_object_threshold.
	// intersection_cost - cost of ray-object intersection for SAH computation.  Relative to traversal cost which is assumed to be 1.
	SBVHBuilder(int leaf_num_object_threshold, int max_num_objects_per_leaf, float intersection_cost, 
		const js::AABBox* aabbs,
		const SBVHTri* triangles,
		const int num_objects
	);
	~SBVHBuilder();

	virtual void build(
		Indigo::TaskManager& task_manager,
		PrintOutput& print_output, 
		bool verbose, 
		js::Vector<ResultNode, 64>& result_nodes_out
	);

	virtual const BVHBuilder::ResultObIndicesVec& getResultObjectIndices() const { return result_indices; }

	int getMaxLeafDepth() const { return stats.max_leaf_depth; } // Root node is considered to have depth 0.

	static void printResultNode(const ResultNode& result_node);
	static void printResultNodes(const js::Vector<ResultNode, 64>& result_nodes);

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
		const ObjectVecType& obs,
		int depth,
		uint64 sort_key,
		SBVHResultChunk* result_chunk
	);

	inline void markNodeAsLeaf(SBVHPerThreadTempInfo& thread_temp_info, const js::AABBox& aabb, uint32 node_index, const std::vector<SBVHOb>& obs, int depth, SBVHResultChunk* node_result_chunk);


	const js::AABBox* aabbs;
	const SBVHTri* triangles;
	int m_num_objects;
	ObjectVecType top_level_objects;
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
public:
	int axis_parallel_num_ob_threshold;
	int new_task_num_ob_threshold;

	SBVHBuildStats stats;

	double split_search_time;
	double partition_time;
};
