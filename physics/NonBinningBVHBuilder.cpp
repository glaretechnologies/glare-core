/*=====================================================================
NonBinningBVHBuilder.cpp
-------------------
Copyright Glare Technologies Limited 2017 -
Generated at Tue Apr 27 15:25:47 +1200 2010
=====================================================================*/
#include "NonBinningBVHBuilder.h"


#include "jscol_aabbox.h"
#include <algorithm>
#include "../utils/Exception.h"
#include "../utils/Sort.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/TaskManager.h"
#include "../utils/Lock.h"
#include "../utils/Timer.h"
#include "../utils/ProfilerStore.h"


// For some reason, using incorrect SAH (not tight bounds on each child, but just using splitting plane to compute bounds) results in better trees.
// So leave this code for 4.0.x until we figure this out.
//#define USE_INCORRECT_SAH 1


NonBinningBVHBuilder::NonBinningBVHBuilder(int leaf_num_object_threshold_, int max_num_objects_per_leaf_, float intersection_cost_, const js::AABBox* aabbs_,
	const int num_objects_
)
:	leaf_num_object_threshold(leaf_num_object_threshold_),
	max_num_objects_per_leaf(max_num_objects_per_leaf_),
	intersection_cost(intersection_cost_),
	local_task_manager(NULL)
{
	assert(intersection_cost > 0.f);

	aabbs = aabbs_;
	num_objects = num_objects_;

	// See /wiki/index.php?title=BVH_Building for results on varying these settings.
	axis_parallel_num_ob_threshold = 1 << 20;
	new_task_num_ob_threshold = 1 << 9;

	static_assert(sizeof(Ob) == 32, "sizeof(Ob) == 32");
	static_assert(sizeof(ResultNode) == 48, "sizeof(ResultNode) == 48");
}


NonBinningBVHBuilder::~NonBinningBVHBuilder()
{
	delete local_task_manager;
}


NonBinningBVHBuildStats::NonBinningBVHBuildStats()
{
	num_maxdepth_leaves = 0;
	num_under_thresh_leaves = 0;
	num_cheaper_nosplit_leaves = 0;
	num_could_not_split_leaves = 0;
	num_leaves = 0;
	max_num_tris_per_leaf = 0;
	leaf_depth_sum = 0;
	max_leaf_depth = 0;
	num_interior_nodes = 0;
	num_arbitrary_split_leaves = 0;
}


void NonBinningBVHBuildStats::accumStats(const NonBinningBVHBuildStats& other)
{
	num_maxdepth_leaves				+= other.num_maxdepth_leaves;
	num_under_thresh_leaves			+= other.num_under_thresh_leaves;
	num_cheaper_nosplit_leaves		+= other.num_cheaper_nosplit_leaves;
	num_could_not_split_leaves		+= other.num_could_not_split_leaves;
	num_leaves						+= other.num_leaves;
	max_num_tris_per_leaf			= myMax(max_num_tris_per_leaf, other.max_num_tris_per_leaf);
	leaf_depth_sum					+= other.leaf_depth_sum;
	max_leaf_depth					= myMax(max_leaf_depth, other.max_leaf_depth);
	num_interior_nodes				+= other.num_interior_nodes;
	num_arbitrary_split_leaves		+= other.num_arbitrary_split_leaves;
}


class CenterItem
{
public:
	int i;
	float f;
	
};


class CenterItemPredicate
{
public:
	inline bool operator() (const CenterItem& a, const CenterItem& b)
	{
		return a.f < b.f;
	}
};


class CenterItemKey
{
public:
	inline float operator() (const CenterItem& item)
	{
		return item.f;
	}
};


/*class ObPredicate
{
public:
	inline bool operator() (const Ob& a, const Ob& b)
	{
		return a.centre[axis] < b.centre[axis];
	}
	int axis;
};


class ObKey
{
public:
	inline float operator() (const Ob& item)
	{
		return item.centre[axis];
	}
	int axis;
};*/


// Construct builder.objects[axis], along the given axis
class ConstructAxisObjectsTask : public Indigo::Task
{
public:
	ConstructAxisObjectsTask(NonBinningBVHBuilder& builder_, int num_objects_, int axis_, js::Vector<CenterItem, 16>* axis_centres_) : builder(builder_), num_objects(num_objects_), axis(axis_), axis_centres(axis_centres_) {}
	
	virtual void run(size_t thread_index)
	{
		const js::Vector<CenterItem, 16>& centres = *axis_centres; // Sorted CenterItems
		js::Vector<Ob, 64>& axis_obs = builder.objects_a[axis]; // Array we will be writing to.
		const js::AABBox* const aabbs = builder.aabbs; // Unsorted AABBs

		//Timer timer2;
		for(int i=0; i<num_objects; ++i)
		{
			const int src_i = centres[i].i;
			axis_obs[i].aabb = aabbs[src_i];
			axis_obs[i].setIndex(src_i);

			assert(axis_obs[i].getIndex() == src_i);

			//axis_obs[i].centre = toVec3f(aabbs[src_i].centroid());
			//axis_obs[i].index = src_i;
		}
		//conPrint("post-sort ob copy: " + timer2.elapsedString());
	}

	NonBinningBVHBuilder& builder;
	int num_objects;
	int axis;
	js::Vector<CenterItem, 16>* axis_centres;
};


/*
Builds a subtree with the objects[begin] ... objects[end-1]
Writes its results to the per-thread buffer 'per_thread_temp_info[thread_index].result_buf', and describes the result in result_chunk,
which is added to the global result_chunks list.

May spawn new BuildSubtreeTasks.
*/
class BuildSubtreeTask : public Indigo::Task
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	BuildSubtreeTask(NonBinningBVHBuilder& builder_) : builder(builder_) {}
	
	virtual void run(size_t thread_index)
	{
		ScopeProfiler _scope("BuildSubtreeTask", (int)thread_index);

		// Create subtree root node
		const int root_node_index = (int)builder.per_thread_temp_info[thread_index].result_buf.size();
		builder.per_thread_temp_info[thread_index].result_buf.push_back_uninitialised();
		builder.per_thread_temp_info[thread_index].result_buf[root_node_index].right_child_chunk_index = -666;

		builder.doBuild(
			builder.per_thread_temp_info[thread_index],
			node_aabb, 
			root_node_index, // node index
			begin, end, depth, sort_key,
			cur_objects,
			other_objects,
			*result_chunk
		);

		result_chunk->thread_index = (int)thread_index;
		result_chunk->offset = root_node_index;
		result_chunk->size = (int)builder.per_thread_temp_info[thread_index].result_buf.size() - root_node_index;
		result_chunk->sort_key = this->sort_key;
	}

	NonBinningBVHBuilder& builder;
	js::Vector<Ob, 64>* cur_objects;
	js::Vector<Ob, 64>* other_objects;
	ResultChunk* result_chunk;
	js::AABBox node_aabb;
	int depth;
	int begin;
	int end;
	uint64 sort_key;
};


struct ResultChunkPred
{
	bool operator () (const ResultChunk& a, const ResultChunk& b)
	{
		return a.sort_key < b.sort_key;
	}
};


// top-level build method
void NonBinningBVHBuilder::build(
		   Indigo::TaskManager& task_manager_,
		   //const js::AABBox* aabbs_,
		  // const int num_objects,
		   PrintOutput& print_output, 
		   bool verbose, 
		   js::Vector<ResultNode, 64>& result_nodes_out
		   )
{
	Timer build_timer;
	ScopeProfiler _scope("NonBinningBVHBuilder::build");
	js::AABBox root_aabb;
	{
	ScopeProfiler _scope2("initial init");

	//Timer timer;
	split_search_time = 0;
	partition_time = 0;

	this->task_manager = &task_manager_;
	//this->aabbs = aabbs_;

	// Create a new task manager for the BestSplitSearchTask tasks, if needed.
	if(num_objects >= axis_parallel_num_ob_threshold)
	{
		delete local_task_manager;
		local_task_manager = new Indigo::TaskManager("NonBinningBVHBuilder local task manager", 3);
	}


	if(num_objects <= 0)
	{
		// Create root node, and mark it as a leaf.
		result_nodes_out.push_back_uninitialised();
		result_nodes_out[0].aabb = js::AABBox::emptyAABBox();
		result_nodes_out[0].interior = false;
		result_nodes_out[0].left = 0;
		result_nodes_out[0].right = 0;
		result_nodes_out[0].depth = 0;


		stats.num_under_thresh_leaves = 1;
		//leaf_depth_sum += 0;
		stats.max_leaf_depth = 0;
		stats.num_leaves = 1;
		return;
	}

	// Build overall AABB
	root_aabb = aabbs[0];
	for(size_t i = 0; i < num_objects; ++i)
		root_aabb.enlargeToHoldAABBox(aabbs[i]);

	// Alloc space for objects for each axis
	//timer.reset();
	for(int axis=0; axis<3; ++axis)
	{
		this->objects_a[axis].resizeNoCopy(num_objects);
		this->objects_b[axis].resizeNoCopy(num_objects);
	}
	//conPrint("building this->objects elapsed: " + timer.elapsedString());

	// Sort indices based on center position along the axes.
	// We will sort CenterItems.
	// The general approach here, taken for efficiency reasons, is to sort small objects that contain an index.
	// Then we will use the index to copy the larger objects into the correct place later.
	//timer.reset();
	{
		js::Vector<CenterItem, 16> all_axis_centres[3]; // We will sort CenterItems.
		js::Vector<CenterItem, 16> working_space(num_objects); // Working space for sort algorithm.

		for(int axis=0; axis<3; ++axis)
		{
			js::Vector<CenterItem, 16>& axis_centres = all_axis_centres[axis];
			axis_centres.resizeNoCopy(num_objects);

			//Timer timer2;
			for(int i=0; i<num_objects; ++i)
			{
				axis_centres[i].f = aabbs[i].centroid()[axis];
				axis_centres[i].i = i;
			}
			//conPrint("building axis_centres[]: " + timer2.elapsedString());

			// Sort axis_centres
			//timer2.reset();
			Sort::radixSortWithParallelPartition<CenterItem, CenterItemKey>(*task_manager, axis_centres.data(), num_objects, CenterItemKey(), working_space.data(), false);
			//conPrint("sort: " + timer2.elapsedString());
		}

		for(int axis=0; axis<3; ++axis)
			task_manager->addTask(new ConstructAxisObjectsTask(*this, num_objects, axis, &all_axis_centres[axis]));
		task_manager->waitForTasksToComplete();
	}
	//conPrint("SortAxisTasks : " + timer.elapsedString());

	// Check we sorted objects along each axis properly:
#ifndef NDEBUG
	for(int axis=0; axis<3; ++axis)
		for(int i=1; i<num_objects; ++i)
		{
			const Ob& ob   = objects_a[axis][i];
			const Ob& prev = objects_a[axis][i-1];
			assert(ob.aabb.centroid()[axis] >= prev.aabb.centroid()[axis]);
		}
#endif


	split_left_half_area.resizeNoCopy(num_objects);

	// Reserve working space for each thread.
	const int initial_result_buf_reserve_cap = 2 * num_objects / (int)task_manager->getNumThreads();
	per_thread_temp_info.resize(task_manager->getNumThreads());
	for(size_t i=0; i<per_thread_temp_info.size(); ++i)
		per_thread_temp_info[i].result_buf.reserve(initial_result_buf_reserve_cap);

	// Alloc per_local_thread_temp_info.  This is used for per-axis computations, so there will be a max of 3 threads.
	if(num_objects >= axis_parallel_num_ob_threshold)
	{
		per_axis_thread_temp_info.resize(3);
		for(size_t i=0; i<per_axis_thread_temp_info.size(); ++i)
			per_axis_thread_temp_info[i].split_left_half_area.resizeNoCopy(num_objects);
	}

	/*
	Consider the tree of chunk splits.
	Each leaf (apart from if the root is a leaf) will have >= c objects, where c is the split threshold (as each child of a split must have >= c objects).
	Each interior node will have >= m*c objects, where m is the number of child leaves.
	Therefore the root node will have >= t*c objects, where t is the total number of leaves.

	    num objects at each node (of the split tree:)
	                        
	                    * >= 3c
	                   / \
	                  /   \
	            >= c *     * >= 2c
	                      / \
	                     /   \
	               >= c *     * >= c
	e.g.
	n >= t * c
	n/c >= t
	t <= n/c
	e.g. num interior nodes <= num objects / split threshold

	*/
	result_chunks.resizeNoCopy(myMax(1, num_objects / new_task_num_ob_threshold));


	} // End scope for initial init


	result_chunks[0].original_index = 0;
	next_result_chunk++;

	Reference<BuildSubtreeTask> task = new BuildSubtreeTask(*this);
	task->begin = 0;
	task->end = num_objects;
	task->depth = 0;
	task->sort_key = 1;
	task->node_aabb = root_aabb;
	task->result_chunk = &result_chunks[0];
	task->cur_objects = objects_a;
	task->other_objects = objects_b;
	task_manager->addTask(task);

	task_manager->waitForTasksToComplete();


	/*conPrint("initial_result_buf_reserve_cap: " + toString(initial_result_buf_reserve_cap));
	for(int i=0; i<(int)per_thread_temp_info.size(); ++i)
	{
		conPrint("thread " + toString(i));
		conPrint("	result_buf.size(): " + toString(per_thread_temp_info[i].result_buf.size()));
		conPrint("	result_buf.capacity(): " + toString(per_thread_temp_info[i].result_buf.capacity()));
	}*/


	// Now we need to combine all the result chunks into a single array.

	//timer.reset();
	//Timer timer;

	// Do a pass over the chunks to build chunk_root_node_indices and get the total number of nodes.
	const int num_result_chunks = (int)next_result_chunk;
	js::Vector<int, 16> final_chunk_offset(num_result_chunks); // Index (in combined/final array) of first node in chunk, for each chunk.
	js::Vector<int, 16> final_chunk_offset_original(num_result_chunks); // Index (in combined/final array) of first node in chunk, for each original (pre-sorted) chunk index.
	size_t total_num_nodes = 0;

	// Sort chunks by sort_key.  TODO: work out the best ordering first
	//std::sort(result_chunks.begin(), result_chunks.begin() + num_result_chunks, ResultChunkPred());

	for(size_t c=0; c<num_result_chunks; ++c)
	{
		//conPrint("Chunk " + toString(c) + ": sort_key=" + toString(result_chunks[c].sort_key) + ", offset=" + toString(result_chunks[c].offset) + 
		//	", size=" + toString(result_chunks[c].size) + ", thread_index=" + toString(result_chunks[c].thread_index));
		final_chunk_offset[c] = (int)total_num_nodes;
		final_chunk_offset_original[result_chunks[c].original_index] = (int)total_num_nodes;
		total_num_nodes += result_chunks[c].size;
		//conPrint("-------pre-merge Chunk " + toString(c) + "--------");
		//printResultNodes(per_thread_temp_info[result_chunks[c]->thread_index].result_buf);
	}

	result_nodes_out.resizeNoCopy(total_num_nodes);
	int write_index = 0;

	for(size_t c=0; c<num_result_chunks; ++c)
	{
		const ResultChunk& chunk = result_chunks[c];
		const int chunk_node_offset = final_chunk_offset[c] - chunk.offset; // delta for references internal to chunk.

		const js::Vector<ResultNode, 64>& chunk_nodes = per_thread_temp_info[chunk.thread_index].result_buf; // Get the per-thread result node buffer that this chunk is in.

		for(size_t i=0; i<chunk.size; ++i)
		{
			ResultNode chunk_node = chunk_nodes[chunk.offset + i];

			// If this is an interior node, we need to fix up some links.
			if(chunk_node.interior)
			{
				// Offset the child node indices by the offset of this chunk
				chunk_node.left += chunk_node_offset;

				const int chunk_index = chunk_node.right_child_chunk_index;

				assert(chunk_index == -1 || (chunk_index >= 0 && chunk_index < num_result_chunks));

				if(chunk_node.right_child_chunk_index != -1) // If the right child index refers to another chunk:
				{
					chunk_node.right = final_chunk_offset_original[chunk_index]; // Update it with the final node index.
				}
				else
					chunk_node.right += chunk_node_offset;

				assert(chunk_node.left < total_num_nodes);
				assert(chunk_node.right < total_num_nodes);
			}

			result_nodes_out[write_index] = chunk_node; // Copy node to final array
			write_index++;
		}
	}

	//conPrint("Final merge elapsed: " + timer.elapsedString());

	result_indices.resizeNoCopy(num_objects);
	const js::Vector<Ob, 64>& objects_a_0 = objects_a[0];
	for(int i=0; i<num_objects; ++i)
		result_indices[i] = objects_a_0[i].getIndex();


	// Combine stats from each thread
	for(size_t i=0; i<per_thread_temp_info.size(); ++i)
		this->stats.accumStats(per_thread_temp_info[i].stats);

	// Dump some mem usage stats
	if(false)
	{
		const double build_time = build_timer.elapsed();

		conPrint("----------- NonBinningBVHBuilder Build Stats ----------");
		conPrint("Mem usage:");
		conPrint("objects:                    " + getNiceByteSize(objects_a[0].dataSizeBytes() * 6));

		size_t total_per_thread_size = 0;
		for(size_t i=0; i<per_thread_temp_info.size(); ++i)
			total_per_thread_size += per_thread_temp_info[i].dataSizeBytes();
		conPrint("per_thread_temp_info:       " + getNiceByteSize(total_per_thread_size));

		size_t total_per_axis_size = 0;
		for(size_t i=0; i<per_axis_thread_temp_info.size(); ++i)
			total_per_axis_size += per_axis_thread_temp_info[i].dataSizeBytes();
		conPrint("per_axis_thread_temp_info:  " + getNiceByteSize(total_per_axis_size));

		conPrint("result_chunks:              " + getNiceByteSize(result_chunks.dataSizeBytes()));
		conPrint("split_left_half_area:       " + getNiceByteSize(split_left_half_area.dataSizeBytes()));
		conPrint("result_nodes_out:           " + toString(result_nodes_out.size()) + " nodes * " + toString(sizeof(ResultNode)) + "B = " + getNiceByteSize(result_nodes_out.dataSizeBytes()));
		
		const size_t total_size = objects_a[0].dataSizeBytes() * 6 + total_per_thread_size + total_per_axis_size + result_chunks.dataSizeBytes() + split_left_half_area.dataSizeBytes() +
			result_nodes_out.dataSizeBytes();
		
		conPrint("total:                      " + getNiceByteSize(total_size));
		conPrint("");

		//conPrint("split_search_time: " + toString(split_search_time) + " s");
		//conPrint("partition_time:    " + toString(partition_time) + " s");

		conPrint("Num triangles:              " + toString(objects_a[0].size()));
		conPrint("num interior nodes:         " + toString(stats.num_interior_nodes));
		conPrint("num leaves:                 " + toString(stats.num_leaves));
		conPrint("num maxdepth leaves:        " + toString(stats.num_maxdepth_leaves));
		conPrint("num under_thresh leaves:    " + toString(stats.num_under_thresh_leaves));
		conPrint("num cheaper nosplit leaves: " + toString(stats.num_cheaper_nosplit_leaves));
		conPrint("num could not split leaves: " + toString(stats.num_could_not_split_leaves));
		conPrint("num arbitrary split leaves: " + toString(stats.num_arbitrary_split_leaves));
		conPrint("av num tris per leaf:       " + toString((float)objects_a[0].size() / stats.num_leaves));
		conPrint("max num tris per leaf:      " + toString(stats.max_num_tris_per_leaf));
		conPrint("av leaf depth:              " + toString((float)stats.leaf_depth_sum / stats.num_leaves));
		conPrint("max leaf depth:             " + toString(stats.max_leaf_depth));
		conPrint("Build took " + doubleToStringNDecimalPlaces(build_time, 4) + " s");
		conPrint("---------------------");
	}
}


/*
Search along a given axis for the best coordinate at which to split.
We use the standard SAH estimates for the costs:

Cost estimate for traversing a split node and its children:

C_split = N_l * C_inter * P_left   +   N_r * C_inter * P_right   +   C_trav

where 
P_left  = A_left  / A
P_right = A_right / A

so
C_split = (N_l * P_left   +   N_r * P_right) * C_inter   + C_trav
= (N_l * A_left   +   N_r * A_right) * C_inter / A   + C_trav                           (1)

Note that only the expression in the parentheses varies with split position, so that's what we will compute in the inner loop.
Actully we will compute half that value.  (due to using getHalfSurfaceArea())

Cost estimate for intersecting a ray against an unsplit node:

C_nosplit = N * C_inter                                                                 (2)
*/
static void searchAxisForBestSplit(const js::AABBox& aabb, const js::Vector<Ob, 64>* cur_objects, js::Vector<float, 16>& split_left_half_area, int axis, int left, int right,
	float& smallest_split_cost_factor_in_out, int& best_N_L_in_out, int& best_axis_in_out, float& best_div_val_in_out, js::AABBox& best_right_aabb_in_out)
{
	const js::Vector<Ob, 64>& axis_obs = cur_objects[axis];
	float* const use_split_left_half_area = split_left_half_area.data();

#if USE_INCORRECT_SAH
	float cap_area, cap_half_circum;
	Vec4f aabb_diff(aabb.max_ - aabb.min_);
	if(axis == 0)
	{
		cap_area        = aabb_diff[1] * aabb_diff[2];
		cap_half_circum = aabb_diff[1] + aabb_diff[2];
	}
	else if(axis == 1)
	{
		cap_area        = aabb_diff[0] * aabb_diff[2];
		cap_half_circum = aabb_diff[0] + aabb_diff[2];
	}
	else
	{
		cap_area        = aabb_diff[0] * aabb_diff[1];
		cap_half_circum = aabb_diff[0] + aabb_diff[1];
	}
#endif

	// Do a forwards sweep over all the objects for this subtree, computing half surface area for the union AABB for the prefix along this axis
	Vec4f prefix_min(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), 1.0f);
	Vec4f prefix_max(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1.0f);
	for(int i=left; i<right; ++i)
	{
		prefix_min = min(prefix_min, axis_obs[i].aabb.min_);
		prefix_max = max(prefix_max, axis_obs[i].aabb.max_);

		// Compute half surface area (see AABBox::getHalfSurfaceArea())
#if USE_INCORRECT_SAH
		const Vec4f diff(prefix_max - aabb.min_);
		const float half_area = cap_area + cap_half_circum * diff[axis];
#else
		const Vec4f diff(prefix_max - prefix_min);
		const Vec4f res = mul(diff, swizzle<2, 0, 1, 3>(diff)); // = diff.x[0]*diff.x[2] + diff.x[1]*diff.x[0] + diff.x[2]*diff.x[1]
		const float half_area = res[0] + res[1] + res[2];
#endif
		use_split_left_half_area[i] = half_area;
	}

	/*
	  |---------0--------|
	|-----------1----------|
	                   |---------------2---------------|
	                           |-------3-------|
	                                                 |----4----| 

	left prefix aabbs:
	  |------------------|                                          0
	|----------------------|                                        1
	|--------------------------------------------------|            2
	|--------------------------------------------------|            3
	|----------------------------------------------------------|    4


	If we split at the midpoint coordinate of objects 0 and 1, then 0 and 1 are assigned to the left child node, and 2, 3, 4 are assigned to the right.
	The left aabb therefore is union(aabb_0, aabb_1) = use_split_left_aabb[1], and the right aabb is union(aabb_2, aabb_3, aabb_4).
	This means that for objects with identical centre coordinates, it's the largest union prefix we should use (e.g. use_split_left_aabb[1] not use_split_left_aabb[0]).
	We can achieve this by checking split coordinates, and only considering the split coordinate if the previous split coordinate was different.
	This means that the use_split_left_aabb with largest index is used.
	*/

	float smallest_split_cost_factor = smallest_split_cost_factor_in_out;
	int best_N_L = best_N_L_in_out;
	int best_axis = best_axis_in_out;
	float best_div_val = best_div_val_in_out;
	js::AABBox best_right_aabb = best_right_aabb_in_out;

	// For Each object centroid:
	js::AABBox right_aabb = axis_obs[right-1].aabb;
	float last_split_val = axis_obs[right-1].aabb.centroid()[axis];//centre[axis];
	int N_L = right - left - 1;
	int N_R = 1;
	for(int i=right-2; i>=left; --i) // i = index of last object assigned to the left.  Start with 1 object assigned to right child node, go down to 1 object assigned to left child node.
	{
		const float left_aabb_half_area = use_split_left_half_area[i];
		const float splitval = axis_obs[i].aabb.centroid()[axis]; // centre[axis];

#ifndef NDEBUG
		// Check that our AABB calculations are correct:
		// NOTE: this check is disabled because it is quite slow.
		/*js::AABBox ref_left_aabb = js::AABBox::emptyAABBox();
		for(int t=left; t<=i; ++t)
			ref_left_aabb.enlargeToHoldAABBox(axis_obs[t].aabb);

		js::AABBox ref_right_aabb = js::AABBox::emptyAABBox();
		for(int t=i+1; t<right; ++t)
			ref_right_aabb.enlargeToHoldAABBox(axis_obs[t].aabb);

		assert(ref_left_aabb.getHalfSurfaceArea() == left_aabb_half_area);
		assert(ref_right_aabb == right_aabb);*/
#endif

		assert(N_L == (i - left) + 1 && N_R == (right - left) - N_L);

		float right_half_area;
#if USE_INCORRECT_SAH
		const Vec4f diff(aabb.max_ - right_aabb.min_);
		right_half_area = cap_area + cap_half_circum * diff[axis];
#else
		right_half_area = right_aabb.getHalfSurfaceArea();
#endif
		const float cost_factor = N_L * left_aabb_half_area + N_R * right_half_area; // Compute SAH cost factor

		if(cost_factor < smallest_split_cost_factor && splitval != last_split_val) // If this is the smallest cost so far, and this is the first such split position seen:
		{
			best_N_L = N_L;
			best_axis = axis;
			smallest_split_cost_factor = cost_factor;
			best_div_val = splitval;
			best_right_aabb = right_aabb;
		}

		last_split_val = splitval;
		right_aabb.enlargeToHoldAABBox(axis_obs[i].aabb);
		N_L--;
		N_R++;
	}

#ifndef NDEBUG
	// Check that we computed the number of objects assigned to the left child correctly.
	if(best_axis == axis && best_N_L != -1)
	{
		int ref_N_L = 0;
		for(int i=left; i<right; ++i)
			if(axis_obs[i].aabb.centroid()[best_axis]/*centre[best_axis]*/ <= best_div_val)
				ref_N_L++;
		assert(ref_N_L == best_N_L);
	}
#endif

	best_N_L_in_out = best_N_L;
	best_axis_in_out = best_axis;
	smallest_split_cost_factor_in_out = smallest_split_cost_factor;
	best_div_val_in_out = best_div_val;
	best_right_aabb_in_out = best_right_aabb;
}


// Search along a given axis for the best split location, according to the SAH.
class BestSplitSearchTask : public Indigo::Task
{
public:
	BestSplitSearchTask(NonBinningBVHBuilder& builder_) : builder(builder_) {}
	
	virtual void run(size_t thread_index)
	{
		smallest_split_cost_factor = std::numeric_limits<float>::infinity();
		best_N_L = -1;
		best_div_val = 0;
		int best_axis = 0; // not used

		searchAxisForBestSplit(aabb, cur_objects, builder.per_axis_thread_temp_info[thread_index].split_left_half_area, axis, left, right,
			smallest_split_cost_factor, best_N_L, best_axis, best_div_val, best_right_aabb);
	}

	js::AABBox aabb;
	NonBinningBVHBuilder& builder;
	js::Vector<Ob, 64>* cur_objects;
	int axis; // Axis to search along.
	int left;
	int right;

	// Results:
	float smallest_split_cost_factor;
	int best_N_L;
	float best_div_val;
	js::AABBox best_right_aabb;
};


// Parition objects in cur_objects for the given axis, based on best_div_val of best_axis.
// Places the paritioned objects in other_objects.
template <int best_axis>
static inline void partitionAxis(const js::Vector<Ob, 64>* cur_objects, js::Vector<Ob, 64>* other_objects, int left, int right, int num_left_tris, float best_div_val, int axis)
{
	const js::Vector<Ob, 64>& axis_obs = cur_objects[axis];
	Ob* const axis_other_obs = other_objects[axis].data();

	int left_write_pos = left;
	int right_write_pos = left + num_left_tris;
	for(int i=left; i<right; ++i)
	{
		const Vec4f min = axis_obs[i].aabb.min_;
		const Vec4f max = axis_obs[i].aabb.max_;
		//const Vec4f centre = _mm_load_ps((const float*)&axis_obs[i].centre);
		if(((min + max)*0.5f)[best_axis]/*centre[best_axis]*/ <= best_div_val) // If on Left side:
		{
			axis_other_obs[left_write_pos].aabb.min_ = min;
			axis_other_obs[left_write_pos].aabb.max_ = max;
			//_mm_store_ps((float*)&axis_other_obs[left_write_pos].centre, centre.v);
			left_write_pos++;
		}
		else // else if on Right side:
		{
			axis_other_obs[right_write_pos].aabb.min_ = min;
			axis_other_obs[right_write_pos].aabb.max_ = max;
			//_mm_store_ps((float*)&axis_other_obs[right_write_pos].centre, centre.v);
			right_write_pos++;
		}
	}
}


// Parition objects in cur_objects for all 3 axes, based on best_div_val of best_axis.
// Places the paritioned objects in other_objects.
template <int best_axis>
static inline void partitionAxes(const js::Vector<Ob, 64>* cur_objects, js::Vector<Ob, 64>* other_objects, int left, int right, int num_left_tris, float best_div_val)
{
	for(int axis=0; axis<3; ++axis)
		partitionAxis<best_axis>(cur_objects, other_objects, left, right, num_left_tris, best_div_val, axis);
}



// Partition the object list for the given axis ('axis').
// This needs to be a stable partition. (preserve ordering)
class PartitionTask : public Indigo::Task
{
public:
	PartitionTask(NonBinningBVHBuilder& builder_) : builder(builder_) {}
	
	virtual void run(size_t thread_index)
	{
		if(best_axis == 0)
			partitionAxis<0>(cur_objects, other_objects, left, right, num_left_tris, best_div_val, axis);
		else if(best_axis == 1)
			partitionAxis<1>(cur_objects, other_objects, left, right, num_left_tris, best_div_val, axis);
		else if(best_axis == 2)
			partitionAxis<2>(cur_objects, other_objects, left, right, num_left_tris, best_div_val, axis);
	}

	NonBinningBVHBuilder& builder;
	js::Vector<Ob, 64>* cur_objects;
	js::Vector<Ob, 64>* other_objects;
	int axis;
	int best_axis;
	float best_div_val;
	int num_left_tris;
	int left;
	int right;
};


// Since we store the object index in the AABB.min_.w, we want to restore this value to 1 before we return it to the client code.
static inline void setAABBWToOne(js::AABBox& aabb)
{
	aabb.min_ = select(Vec4f(1.f), aabb.min_, bitcastToVec4f(Vec4i(0, 0, 0, 0xFFFFFFFF)));
	aabb.max_ = select(Vec4f(1.f), aabb.max_, bitcastToVec4f(Vec4i(0, 0, 0, 0xFFFFFFFF)));
}


/*class PartitionPred
{
public:
	bool operator () (const Ob& ob) const
	{
		return ob.centre[best_axis] <= best_div_val;
	}

	int best_axis;
	float best_div_val;
};*/


/*
Recursively build a subtree.
Assumptions: root node for subtree is already created and is at node_index
*/
void NonBinningBVHBuilder::doBuild(
			PerThreadTempInfo& thread_temp_info,
			const js::AABBox& aabb,
			uint32 node_index, 
			int left, 
			int right, 
			int depth,
			uint64 sort_key,
			js::Vector<Ob, 64>* cur_objects,
			js::Vector<Ob, 64>* other_objects,
			ResultChunk& result_chunk
			)
{
	const int MAX_DEPTH = 60;

	js::Vector<ResultNode, 64>& chunk_nodes = thread_temp_info.result_buf;

	if(right - left <= leaf_num_object_threshold || depth >= MAX_DEPTH)
	{
		assert(right - left <= max_num_objects_per_leaf);

		// Make this a leaf node
		chunk_nodes[node_index].interior = false;
		chunk_nodes[node_index].aabb = aabb;
		chunk_nodes[node_index].left = left;
		chunk_nodes[node_index].right = right;
		chunk_nodes[node_index].depth = depth;

		// if cur_objects is objects_b, we need to copy back into objects_a
		if(cur_objects == objects_b)
			for(int axis=0; axis<3; ++axis)
				std::memcpy(&(objects_a[axis])[left], &(objects_b[axis])[left], sizeof(Ob) * (right - left));

		// Update build stats
		if(depth >= MAX_DEPTH)
			thread_temp_info.stats.num_maxdepth_leaves++;
		else
			thread_temp_info.stats.num_under_thresh_leaves++;
		thread_temp_info.stats.leaf_depth_sum += depth;
		thread_temp_info.stats.max_leaf_depth = myMax(thread_temp_info.stats.max_leaf_depth, depth);
		thread_temp_info.stats.max_num_tris_per_leaf = myMax(thread_temp_info.stats.max_num_tris_per_leaf, right - left);
		thread_temp_info.stats.num_leaves++;
		return;
	}

	const float traversal_cost = 1.0f;
	const float non_split_cost = (right - left) * intersection_cost; // Eqn 2.
	float smallest_split_cost_factor = std::numeric_limits<float>::infinity(); // Smallest (N_L * half_left_surface_area + N_R * half_right_surface_area) found.
	int best_N_L = -1;
	int best_axis = -1;
	float best_div_val = 0;
	js::AABBox best_right_aabb;

	//conPrint("Looking for best split...");
	//Timer timer;

	// If there are enough objects to process, do in parallel, one task for each axis
	if((right - left) >= axis_parallel_num_ob_threshold/* && !task_manager->areAllThreadsBusy()*/)
	{
		// conPrint("doing BestSplitSearchTask in parallel with " + toString(right - left) + " objects");

		Reference<BestSplitSearchTask> tasks[3];
		for(int axis = 0; axis < 3; ++axis)
		{
			tasks[axis] = new BestSplitSearchTask(*this);
			tasks[axis]->cur_objects = cur_objects;
			tasks[axis]->axis = axis;
			tasks[axis]->left = left;
			tasks[axis]->right = right;
			tasks[axis]->aabb = aabb;
			local_task_manager->addTask(tasks[axis]);
		}

		local_task_manager->waitForTasksToComplete();

		for(int axis = 0; axis < 3; ++axis)
		{
			if(tasks[axis]->smallest_split_cost_factor < smallest_split_cost_factor)
			{
				best_N_L = tasks[axis]->best_N_L;
				smallest_split_cost_factor = tasks[axis]->smallest_split_cost_factor;
				best_axis = tasks[axis]->axis;
				best_div_val = tasks[axis]->best_div_val;
				best_right_aabb = tasks[axis]->best_right_aabb;
			}
		}
	}
	else
	{
		for(unsigned int axis=0; axis<3; ++axis)
		{
			searchAxisForBestSplit(aabb, cur_objects, this->split_left_half_area, axis, left, right,
				smallest_split_cost_factor, best_N_L, best_axis, best_div_val, best_right_aabb);
		}
	}

	//conPrint("Looking for best split done.  elapsed: " + timer.elapsedString());
	//split_search_time += timer.elapsed();

	if(best_axis == -1) // This can happen when all object centres coordinates along the axis are the same.
	{
		doArbitrarySplits(thread_temp_info, aabb, node_index, left, right, depth, cur_objects, other_objects, result_chunk);
		return;
	}

	assert(aabb.containsAABBox(best_right_aabb));

	// NOTE: the factor of 2 compensates for the surface area vars being half the areas.
	const float smallest_split_cost = 2 * intersection_cost * smallest_split_cost_factor / aabb.getSurfaceArea() + traversal_cost; // Eqn 1.

	// If it is not advantageous to split, and the number of objects is <= the max num per leaf, then form a leaf:
	if((smallest_split_cost >= non_split_cost) && ((right - left) <= max_num_objects_per_leaf))
	{
		chunk_nodes[node_index].interior = false;
		chunk_nodes[node_index].aabb = aabb;
		chunk_nodes[node_index].left = left;
		chunk_nodes[node_index].right = right;
		chunk_nodes[node_index].depth = depth;

		// if cur_objects is objects_b, we need to copy back into objects_a
		if(cur_objects == objects_b)
			for(int axis=0; axis<3; ++axis)
				std::memcpy(&(objects_a[axis])[left], &(objects_b[axis])[left], sizeof(Ob) * (right - left));

		// Update build stats
		thread_temp_info.stats.num_cheaper_nosplit_leaves++;
		thread_temp_info.stats.leaf_depth_sum += depth;
		thread_temp_info.stats.max_leaf_depth = myMax(thread_temp_info.stats.max_leaf_depth, depth);
		thread_temp_info.stats.max_num_tris_per_leaf = myMax(thread_temp_info.stats.max_num_tris_per_leaf, right - left);
		thread_temp_info.stats.num_leaves++;
		return;
	}
	
	const int num_left_tris = best_N_L;

	//Timer timer;
	//timer.reset();

	if((right - left) >= axis_parallel_num_ob_threshold)
	{
		//conPrint("Partitioning with task (right - left = " + toString(right - left) + ") ...");

		Reference<PartitionTask> tasks[3];
		for(int axis = 0; axis < 3; ++axis)
		{
			tasks[axis] = new PartitionTask(*this);
			tasks[axis]->cur_objects = cur_objects;
			tasks[axis]->other_objects = other_objects;
			tasks[axis]->axis = axis;
			tasks[axis]->best_axis = best_axis;
			tasks[axis]->num_left_tris = num_left_tris;
			tasks[axis]->best_div_val = best_div_val;
			tasks[axis]->left = left;
			tasks[axis]->right = right;
			local_task_manager->addTask(tasks[axis]);
		}

		local_task_manager->waitForTasksToComplete();
		
		/*for(int axis = 0; axis < 3; ++axis)
		{
			js::Vector<Ob, 64>& axis_obs = objects[axis];

			PartitionPred pred;
			pred.best_axis = best_axis;
			pred.best_div_val = best_div_val;

			// puts results in temp0
			const size_t actual_num_left = Sort::parallelStablePartition(*local_task_manager, axis_obs.data() + left, temp0.data(), right - left, pred);

			// Copy back from temp0 to axis_obs
			std::memcpy(axis_obs.data() + left, temp0.data(), (right - left)*sizeof(Ob));

			split_i = left + (int)actual_num_left;
		}*/

		//conPrint("   Partitioning done.  elapsed: " + timer.elapsedString());
	}
	else
	{
		//if((right - left) >= axis_parallel_num_ob_threshold)
		//	conPrint("Partitioning without task (right - left = " + toString(right - left) + ") ...");

		// Do a pass, copying all objects other', at the correct position
		if(best_axis == 0)
			partitionAxes<0>(cur_objects, other_objects, left, right, num_left_tris, best_div_val);
		else if(best_axis == 1)
			partitionAxes<1>(cur_objects, other_objects, left, right, num_left_tris, best_div_val);
		else if(best_axis == 2)
			partitionAxes<2>(cur_objects, other_objects, left, right, num_left_tris, best_div_val);
		
		/*PartitionPred pred;
		pred.best_axis = best_axis;
		pred.best_div_val = best_div_val;
		
		const size_t actual_num_left = Sort::stablePartition(axis_obs.data() + left, temp0.data(), right - left, pred);
		
		// Copy temp objects back to the object list
		std::memcpy(&axis_obs[left], &temp0[0], sizeof(Ob) * (right - left));*/
		

		//if((right - left) >= axis_parallel_num_ob_threshold)
		//	conPrint("   Partitioning done.  elapsed: " + timer.elapsedString());
	}

	if(num_left_tris == 0 || num_left_tris == right - left)
	{
		// Split objects arbitrarily until the number in each leaf is <= max_num_objects_per_leaf.
		doArbitrarySplits(thread_temp_info, aabb, node_index, left, right, depth, cur_objects, other_objects, result_chunk);
		return;
	}

	// Compute left child AABB
	Vec4f left_min( std::numeric_limits<float>::infinity(),  std::numeric_limits<float>::infinity(),  std::numeric_limits<float>::infinity(), 1.0f);
	Vec4f left_max(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1.0f);
	for(int i=left; i<left + best_N_L; ++i)
	{
		left_min = min(left_min, other_objects[0][i].aabb.min_);
		left_max = max(left_max, other_objects[0][i].aabb.max_);
	}
	js::AABBox left_aabb(left_min, left_max);
	setAABBWToOne(left_aabb);

	assert(aabb.containsAABBox(left_aabb));

	//conPrint("Partitioning done.  elapsed: " + timer.elapsedString());
	//timer.reset();
	//partition_time += timer.elapsed();


	// Create child nodes
	const uint32 left_child = (uint32)chunk_nodes.size();
	chunk_nodes.push_back_uninitialised();

	const int split_i = left + num_left_tris;
	const bool do_right_child_in_new_task = (num_left_tris >= new_task_num_ob_threshold) && ((right - split_i) >= new_task_num_ob_threshold);// && !task_manager->areAllThreadsBusy();

	//if(do_right_child_in_new_task)
	//	conPrint("Splitting task: num left=" + toString(split_i - left) + ", num right=" + toString(right - split_i));
	//else
	//	conPrint("not splitting.");

	uint32 right_child;
	if(!do_right_child_in_new_task)
	{
		right_child = (uint32)chunk_nodes.size();
		chunk_nodes.push_back_uninitialised();
	}
	else
	{
		right_child = 0; // Root node of new task subtree chunk.
	}


	// Mark this node as an interior node.
	setAABBWToOne(best_right_aabb);
	chunk_nodes[node_index].interior = true;
	chunk_nodes[node_index].aabb = aabb;
	chunk_nodes[node_index].left = left_child;
	chunk_nodes[node_index].right = right_child;
	chunk_nodes[node_index].right_child_chunk_index = -1;
	chunk_nodes[node_index].depth = depth;

	thread_temp_info.stats.num_interior_nodes++;

	// Build left child
	doBuild(
		thread_temp_info,
		left_aabb, // aabb
		left_child, // node index
		left, // left
		split_i, // right
		depth + 1, // depth
		sort_key << 1, // sort key
		other_objects, // cur_objects - we are swapping
		cur_objects, // other objects - we are swapping
		result_chunk
	);

	if(do_right_child_in_new_task)
	{
		// Add to chunk list
		const int chunk_index = (int)next_result_chunk++;
		chunk_nodes[node_index].right_child_chunk_index = chunk_index;
		assert((int)chunk_index < (int)result_chunks.size());
		result_chunks[chunk_index].original_index = chunk_index;

		// Put this subtree into a task
		//conPrint("Making new task...");
		Reference<BuildSubtreeTask> subtree_task = new BuildSubtreeTask(*this);
		subtree_task->result_chunk = &result_chunks[chunk_index];
		subtree_task->node_aabb = best_right_aabb;
		subtree_task->depth = depth + 1;
		subtree_task->sort_key = (sort_key << 1) | 1;
		subtree_task->begin = split_i;
		subtree_task->end = right;
		subtree_task->cur_objects = other_objects; //  we are swapping
		subtree_task->other_objects = cur_objects; //  we are swapping
		task_manager->addTask(subtree_task);
	}
	else
	{
		// Build right child
		doBuild(
			thread_temp_info,
			best_right_aabb, // aabb
			right_child, // node index
			split_i, // left
			right, // right
			depth + 1, // depth
			(sort_key << 1) | 1,
			other_objects, // cur_objects - we are swapping
			cur_objects, // other objects - we are swapping
			result_chunk
		);
	}
}


// We may get multiple objects with the same bounding box.
// These objects can't be split in the usual way.
// Also the number of such objects assigned to a subtree may be > max_num_objects_per_leaf.
// In this case we will just divide the object list into two until the num per subtree is <= max_num_objects_per_leaf.
void NonBinningBVHBuilder::doArbitrarySplits(
		PerThreadTempInfo& thread_temp_info,
		const js::AABBox& aabb,
		uint32 node_index, 
		int left, 
		int right, 
		int depth,
		js::Vector<Ob, 64>* cur_objects,
		js::Vector<Ob, 64>* other_objects,
		ResultChunk& result_chunk
	)
{
	/*conPrint("------------doArbitrarySplits()------------");
	for(int i=left; i<right; ++i)
	{
		const int aabb_index = objects[0][i];
		const js::AABBox ob_aabb = aabbs[aabb_index];
		conPrint("ob " + toString(aabb_index) + " min: " + ob_aabb.min_.toString());
		conPrint("ob " + toString(aabb_index) + " max: " + ob_aabb.max_.toString());
		conPrint("");
	}*/

	js::Vector<ResultNode, 64>& chunk_nodes = thread_temp_info.result_buf;

	if(right - left <= max_num_objects_per_leaf)
	{
		// Make this a leaf node
		chunk_nodes[node_index].interior = false;
		chunk_nodes[node_index].aabb = aabb;
		chunk_nodes[node_index].left = left;
		chunk_nodes[node_index].right = right;
		chunk_nodes[node_index].depth = depth;

		// if cur_objects is objects_b, we need to copy back into objects_a
		if(cur_objects == objects_b)
			for(int axis=0; axis<3; ++axis)
				std::memcpy(&(objects_a[axis])[left], &(objects_b[axis])[left], sizeof(Ob) * (right - left));
		
		// Update build stats
		thread_temp_info.stats.num_arbitrary_split_leaves++;
		thread_temp_info.stats.leaf_depth_sum += depth;
		thread_temp_info.stats.max_leaf_depth = myMax(thread_temp_info.stats.max_leaf_depth, depth);
		thread_temp_info.stats.max_num_tris_per_leaf = myMax(thread_temp_info.stats.max_num_tris_per_leaf, right - left);
		thread_temp_info.stats.num_leaves++;
		return;
	}

	const int split_i = left + (right - left)/2;


	// Compute AABBs for children
	js::AABBox left_aabb  = js::AABBox::emptyAABBox();
	js::AABBox right_aabb = js::AABBox::emptyAABBox();

	for(int i=left; i<split_i; ++i)
		left_aabb.enlargeToHoldAABBox(cur_objects[0][i].aabb);

	for(int i=split_i; i<right; ++i)
		right_aabb.enlargeToHoldAABBox(cur_objects[0][i].aabb);

	// Create child nodes
	const uint32 left_child  = (uint32)chunk_nodes.size();
	const uint32 right_child = (uint32)chunk_nodes.size() + 1;
	chunk_nodes.push_back_uninitialised();
	chunk_nodes.push_back_uninitialised();

	// Create interior node
	setAABBWToOne(left_aabb);
	setAABBWToOne(right_aabb);
	chunk_nodes[node_index].interior = true;
	chunk_nodes[node_index].aabb = aabb;
	chunk_nodes[node_index].left = left_child;
	chunk_nodes[node_index].right = right_child;
	chunk_nodes[node_index].right_child_chunk_index = -1;
	chunk_nodes[node_index].depth = depth;

	thread_temp_info.stats.num_interior_nodes++;

	// TODO: Task splitting here as well?

	// Build left child
	doArbitrarySplits(
		thread_temp_info,
		left_aabb, // aabb
		left_child, // node index
		left, // left
		split_i, // right
		depth + 1, // depth
		cur_objects, // Don't swap, objects are still in cur_objects
		other_objects,
		result_chunk
	);

	// Build right child
	doArbitrarySplits(
		thread_temp_info,
		right_aabb, // aabb
		right_child, // node index
		split_i, // left
		right, // right
		depth + 1, // depth
		cur_objects, // Don't swap, objects are still in cur_objects
		other_objects,
		result_chunk
	);
}


void NonBinningBVHBuilder::printResultNode(const ResultNode& result_node)
{
	if(result_node.interior)
		conPrint(" Interior");
	else
		conPrint(" Leaf");

	conPrint("	AABB:  " + result_node.aabb.toStringNSigFigs(4));
	if(result_node.interior)
	{
		conPrint("	left_child_index:  " + toString(result_node.left));
		conPrint("	right_child_index: " + toString(result_node.right));
		conPrint("	right_child_chunk_index: " + toString(result_node.right_child_chunk_index));
	}
	else
	{
		conPrint("	objects_begin: " + toString(result_node.left));
		conPrint("	objects_end:   " + toString(result_node.right));
	}
}

void NonBinningBVHBuilder::printResultNodes(const js::Vector<ResultNode, 64>& result_nodes)
{
	for(size_t i=0; i<result_nodes.size(); ++i)
	{
		conPrintStr("node " + toString(i) + ": ");
		printResultNode(result_nodes[i]);
	}
}
