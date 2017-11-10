/*=====================================================================
BinningBVHBuilder.cpp
-------------------
Copyright Glare Technologies Limited 2017 -
Generated at Tue Apr 27 15:25:47 +1200 2010
=====================================================================*/
#include "BinningBVHBuilder.h"


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


static const js::AABBox empty_aabb = js::AABBox::emptyAABBox();


BinningBVHBuilder::BinningBVHBuilder(int leaf_num_object_threshold_, int max_num_objects_per_leaf_, float intersection_cost_)
:	leaf_num_object_threshold(leaf_num_object_threshold_),
	max_num_objects_per_leaf(max_num_objects_per_leaf_),
	intersection_cost(intersection_cost_)
{
	assert(intersection_cost > 0.f);

	aabbs = NULL;

	// See /wiki/index.php?title=BVH_Building for results on varying these settings.
	axis_parallel_num_ob_threshold = 1 << 20;
	new_task_num_ob_threshold = 1 << 9;

	static_assert(sizeof(ResultNode) == 48, "sizeof(ResultNode) == 48");
}


BinningBVHBuilder::~BinningBVHBuilder()
{
}


BinningBVHBuildStats::BinningBVHBuildStats()
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


void BinningBVHBuildStats::accumStats(const BinningBVHBuildStats& other)
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


/*
Builds a subtree with the objects[begin] ... objects[end-1]
Writes its results to the per-thread buffer 'per_thread_temp_info[thread_index].result_buf', and describes the result in result_chunk,
which is added to the global result_chunks list.

May spawn new BuildSubtreeTasks.
*/
class BinningBuildSubtreeTask : public Indigo::Task
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	BinningBuildSubtreeTask(BinningBVHBuilder& builder_) : builder(builder_) {}
	
	virtual void run(size_t thread_index)
	{
		ScopeProfiler _scope("BinningBuildSubtreeTask", (int)thread_index);

		// Create subtree root node
		const int root_node_index = (int)builder.per_thread_temp_info[thread_index].result_buf.size();
		builder.per_thread_temp_info[thread_index].result_buf.push_back_uninitialised();
		builder.per_thread_temp_info[thread_index].result_buf[root_node_index].right_child_chunk_index = -666;

		builder.doBuild(
			builder.per_thread_temp_info[thread_index],
			node_aabb,
			centroid_aabb,
			root_node_index, // node index
			begin, end, depth, sort_key,
			*result_chunk
		);

		result_chunk->thread_index = (int)thread_index;
		result_chunk->offset = root_node_index;
		result_chunk->size = (int)builder.per_thread_temp_info[thread_index].result_buf.size() - root_node_index;
		result_chunk->sort_key = this->sort_key;
	}

	BinningBVHBuilder& builder;
	BinningResultChunk* result_chunk;
	js::AABBox node_aabb;
	js::AABBox centroid_aabb;
	int depth;
	int begin;
	int end;
	uint64 sort_key;
};


struct BinningResultChunkPred
{
	bool operator () (const BinningResultChunk& a, const BinningResultChunk& b)
	{
		return a.sort_key < b.sort_key;
	}
};


// top-level build method
void BinningBVHBuilder::build(
		   Indigo::TaskManager& task_manager_,
		   const js::AABBox* aabbs_,
		   const int num_objects,
		   PrintOutput& print_output, 
		   bool verbose, 
		   js::Vector<ResultNode, 64>& result_nodes_out
		   )
{
	Timer build_timer;
	ScopeProfiler _scope("BVHBuilder::build");
	js::AABBox root_aabb;
	js::AABBox centroid_aabb;
	{
	ScopeProfiler _scope2("initial init");

	//Timer timer;
	split_search_time = 0;
	partition_time = 0;

	this->task_manager = &task_manager_;
	this->aabbs = aabbs_;

	if(num_objects <= 0)
	{
		// Create root node, and mark it as a leaf.
		result_nodes_out.push_back_uninitialised();
		result_nodes_out[0].aabb = empty_aabb;
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
	centroid_aabb = js::AABBox(aabbs[0].centroid(), aabbs[0].centroid());
	for(size_t i = 0; i < num_objects; ++i)
	{
		root_aabb.enlargeToHoldAABBox(aabbs[i]);
		centroid_aabb.enlargeToHoldPoint(aabbs[i].centroid());
	}

	// Alloc space for objects for each axis
	//timer.reset();
	this->objects.resizeNoCopy(num_objects);
	for(size_t i = 0; i < num_objects; ++i)
	{
		objects[i].aabb = aabbs[i];
		objects[i].setIndex((int)i);
	}
	
	
	// Reserve working space for each thread.
	const int initial_result_buf_reserve_cap = 2 * num_objects / (int)task_manager->getNumThreads();
	per_thread_temp_info.resize(task_manager->getNumThreads());
	for(size_t i=0; i<per_thread_temp_info.size(); ++i)
		per_thread_temp_info[i].result_buf.reserve(initial_result_buf_reserve_cap);


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

	Reference<BinningBuildSubtreeTask> task = new BinningBuildSubtreeTask(*this);
	task->begin = 0;
	task->end = num_objects;
	task->depth = 0;
	task->sort_key = 1;
	task->node_aabb = root_aabb;
	task->centroid_aabb = centroid_aabb;
	task->result_chunk = &result_chunks[0];
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
		const BinningResultChunk& chunk = result_chunks[c];
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
	for(int i=0; i<num_objects; ++i)
		result_indices[i] = objects[i].getIndex();


	// Combine stats from each thread
	for(size_t i=0; i<per_thread_temp_info.size(); ++i)
		this->stats.accumStats(per_thread_temp_info[i].stats);

	// Dump some mem usage stats
	if(true)
	{
		const double build_time = build_timer.elapsed();

		conPrint("---------------------");
		conPrint("Mem usage:");
		conPrint("objects:                    " + getNiceByteSize(objects.dataSizeBytes()));

		size_t total_per_thread_size = 0;
		for(size_t i=0; i<per_thread_temp_info.size(); ++i)
			total_per_thread_size += per_thread_temp_info[i].dataSizeBytes();
		conPrint("per_thread_temp_info:       " + getNiceByteSize(total_per_thread_size));

		conPrint("result_chunks:              " + getNiceByteSize(result_chunks.dataSizeBytes()));
		conPrint("result_nodes_out:           " + toString(result_nodes_out.size()) + " nodes * " + toString(sizeof(ResultNode)) + "B = " + getNiceByteSize(result_nodes_out.dataSizeBytes()));
		
		const size_t total_size = objects.dataSizeBytes() + total_per_thread_size + result_chunks.dataSizeBytes() + result_nodes_out.dataSizeBytes();
		
		conPrint("total:                      " + getNiceByteSize(total_size));
		conPrint("");

		//conPrint("split_search_time: " + toString(split_search_time) + " s");
		//conPrint("partition_time:    " + toString(partition_time) + " s");

		conPrint("Num triangles:              " + toString(objects.size()));
		conPrint("num interior nodes:         " + toString(stats.num_interior_nodes));
		conPrint("num leaves:                 " + toString(stats.num_leaves));
		conPrint("num maxdepth leaves:        " + toString(stats.num_maxdepth_leaves));
		conPrint("num under_thresh leaves:    " + toString(stats.num_under_thresh_leaves));
		conPrint("num cheaper nosplit leaves: " + toString(stats.num_cheaper_nosplit_leaves));
		conPrint("num could not split leaves: " + toString(stats.num_could_not_split_leaves));
		conPrint("num arbitrary split leaves: " + toString(stats.num_arbitrary_split_leaves));
		conPrint("av num tris per leaf:       " + toString((float)objects.size() / stats.num_leaves));
		conPrint("max num tris per leaf:      " + toString(stats.max_num_tris_per_leaf));
		conPrint("av leaf depth:              " + toString((float)stats.leaf_depth_sum / stats.num_leaves));
		conPrint("max leaf depth:             " + toString(stats.max_leaf_depth));
		conPrint("Build took " + doubleToStringNDecimalPlaces(build_time, 4) + " s");
		conPrint("---------------------");
	}
}


/*

1 5 8 4 3 9 7 6 2

Let's say <= 5 go to left

1 5 8 4 3 9 7 6 2
c               o

1 5 8 4 3 9 7 6 2
  c             o

1 5 8 4 3 9 7 6 2
    c           o

	[[swap]]

1 5 2 4 3 9 7 6 8
    c         o  

1 5 2 4 3 9 7 6 8
      c       o

1 5 2 4 3 9 7 6 8
          c   o

1 5 2 4 3 6 7 9 8
          c o

1 5 2 4 3 6 7 9 8
          c
          o

*/

// Parition objects in cur_objects for the given axis, based on best_div_val of best_axis.
// Places the paritioned objects in other_objects.
//template <int best_axis>
struct PartitionRes
{
	js::AABBox left_aabb, left_centroid_aabb;
	js::AABBox right_aabb, right_centroid_aabb;
	int cur;
};

static void partition(js::Vector<BinningOb, 64>& objects_, int left, int right, float best_div_val, int best_axis, PartitionRes& res_out)
{
	BinningOb* const objects = objects_.data();

	js::AABBox left_aabb = empty_aabb;
	js::AABBox left_centroid_aabb = empty_aabb;
	js::AABBox right_aabb = empty_aabb;
	js::AABBox right_centroid_aabb = empty_aabb;
	int cur = left;
	int other = right - 1;

	/*for(int i=0; i<8; ++i)
	{
		_mm_prefetch((const char*)(objects + left  + i), _MM_HINT_T0);
		_mm_prefetch((const char*)(objects + right - i), _MM_HINT_T0);
	}*/

	while(cur <= other)
	{
		//_mm_prefetch((const char*)(objects + cur   + 32), _MM_HINT_T0);
		//_mm_prefetch((const char*)(objects + other - 32), _MM_HINT_T0);

		const js::AABBox aabb = objects[cur].aabb;
		const Vec4f centroid = aabb.centroid();
		if(centroid[best_axis] <= best_div_val) // If object should go on Left side:
		{
			left_aabb.enlargeToHoldAABBox(aabb);
			left_centroid_aabb.enlargeToHoldPoint(centroid);
			cur++;
		}
		else // else if cur object should go on Right side:
		{
			std::swap(objects[cur], objects[other]); // Swap. After the swap we know obs[other] should be on right, so we can decr other.

			right_aabb.enlargeToHoldAABBox(aabb);
			right_centroid_aabb.enlargeToHoldPoint(centroid);

			other--;
		}
	}

	res_out.left_aabb = left_aabb;
	res_out.left_centroid_aabb = left_centroid_aabb;
	res_out.right_aabb = right_aabb;
	res_out.right_centroid_aabb = right_centroid_aabb;
	res_out.cur = cur;
}


// Since we store the object index in the AABB.min_.w, we want to restore this value to 1 before we return it to the client code.
static inline void setAABBWToOne(js::AABBox& aabb)
{
	aabb.min_ = select(Vec4f(1.f), aabb.min_, bitcastToVec4f(Vec4i(0, 0, 0, 0xFFFFFFFF)));
	aabb.max_ = select(Vec4f(1.f), aabb.max_, bitcastToVec4f(Vec4i(0, 0, 0, 0xFFFFFFFF)));
}


static void search(const js::AABBox& centroid_aabb_, js::Vector<BinningOb, 64>& objects_, int left, int right, int& best_axis_out, float& best_div_val_out, float& smallest_split_cost_factor_out)
{
	const js::AABBox& centroid_aabb = centroid_aabb_;
	BinningOb* const objects = objects_.data();

	float smallest_split_cost_factor = std::numeric_limits<float>::infinity(); // Smallest (N_L * half_left_surface_area + N_R * half_right_surface_area) found.
	//int best_N_L = -1;
	int best_axis = -1;
	float best_div_val = 0;

	
	const int max_B = 32;
	const int num_buckets = myMin(max_B, (int)(4 + 0.05f * (right - left))); // buckets per axis
	js::AABBox bucket_aabbs[max_B * 3];
	Vec4i counts[max_B];
	for(int i=0; i<num_buckets; ++i)
		for(int z=0; z<3; ++z)
		{
			bucket_aabbs[z*max_B + i] = empty_aabb;
			counts[i] = Vec4i(0);
		}

	const Vec4f scale = div(Vec4f((float)num_buckets), (centroid_aabb.max_ - centroid_aabb.min_));
	for(int i=left; i<right; ++i)
	{
		const js::AABBox ob_aabb = objects[i].aabb;
		const Vec4f centroid = ob_aabb.centroid();
		assert(centroid_aabb.contains(centroid));
		const Vec4i bucket_i = clamp(truncateToVec4i(mul((centroid - centroid_aabb.min_), scale)), Vec4i(0), Vec4i(num_buckets-1));

		assert(bucket_i[0] >= 0 && bucket_i[0] < num_buckets);
		assert(bucket_i[1] >= 0 && bucket_i[1] < num_buckets);
		assert(bucket_i[2] >= 0 && bucket_i[2] < num_buckets);

		// X axis:
		const int b_x = elem<0>(bucket_i);
		bucket_aabbs[b_x].enlargeToHoldAABBox(ob_aabb);
		(counts[b_x])[0]++;

		// Y axis:
		const int b_y = elem<1>(bucket_i);
		bucket_aabbs[max_B + b_y].enlargeToHoldAABBox(ob_aabb);
		(counts[b_y])[1]++;

		// Z axis:
		const int b_z = elem<2>(bucket_i);
		bucket_aabbs[max_B*2 + b_z].enlargeToHoldAABBox(ob_aabb);
		(counts[b_z])[2]++;
	}

	// Sweep right to left computing exclusive prefix surface areas and counts
	Vec4i right_prefix_counts[max_B];
	Vec4f right_area[max_B];
	Vec4i count(0);
	js::AABBox right_aabb[3];
	for(int i=0; i<3; ++i)
		right_aabb[i] = empty_aabb;

	for(int b=num_buckets-1; b>=0; --b)
	{
		right_prefix_counts[b] = count;
		count = count + counts[b];

		float A0 = right_aabb[0].getHalfSurfaceArea();
		float A1 = right_aabb[1].getHalfSurfaceArea();
		float A2 = right_aabb[2].getHalfSurfaceArea();
		right_area[b] = Vec4f(A0, A1, A2, A2);

		right_aabb[0].enlargeToHoldAABBox(bucket_aabbs[b]);
		right_aabb[1].enlargeToHoldAABBox(bucket_aabbs[b +   max_B]);
		right_aabb[2].enlargeToHoldAABBox(bucket_aabbs[b + 2*max_B]);
	}

	// Sweep left to right, computing inclusive left prefix surface area and counts, and computing overall cost factor
	count = Vec4i(0);
	js::AABBox left_aabb[3];
	for(int i=0; i<3; ++i)
		left_aabb[i] = empty_aabb;

	const Vec4f axis_len_over_num_buckets = div(centroid_aabb.max_ - centroid_aabb.min_, Vec4f((float)num_buckets));

	for(int b=0; b<num_buckets-1; ++b)
	{
		count = count + counts[b];

		left_aabb[0].enlargeToHoldAABBox(bucket_aabbs[b]);
		left_aabb[1].enlargeToHoldAABBox(bucket_aabbs[b +   max_B]);
		left_aabb[2].enlargeToHoldAABBox(bucket_aabbs[b + 2*max_B]);

		float A0 = left_aabb[0].getHalfSurfaceArea();
		float A1 = left_aabb[1].getHalfSurfaceArea();
		float A2 = left_aabb[2].getHalfSurfaceArea();

		const Vec4f cost = mul(toVec4f(count), Vec4f(A0, A1, A2, A2)) + mul(toVec4f(right_prefix_counts[b]), right_area[b]); // Compute SAH cost factor

		if(smallest_split_cost_factor > elem<0>(cost))
		{
			smallest_split_cost_factor = elem<0>(cost);
			//best_N_L = count[0];
			best_axis = 0;
			best_div_val = centroid_aabb.min_[0] + axis_len_over_num_buckets[0] * (b + 1); // centroid_aabb.axisLength(0) * (b + 1) / num_buckets;
			//best_left_aabb = left_aabb[0];
		}

		if(smallest_split_cost_factor > elem<1>(cost))
		{
			smallest_split_cost_factor = elem<1>(cost);
			//best_N_L = count[1];
			best_axis = 1;
			best_div_val = centroid_aabb.min_[1] + axis_len_over_num_buckets[1] * (b + 1); //  + centroid_aabb.axisLength(1) * (b + 1) / num_buckets;
			//best_left_aabb = left_aabb[1];
		}

		if(smallest_split_cost_factor > elem<2>(cost))
		{
			smallest_split_cost_factor = elem<2>(cost);
			//best_N_L = count[2];
			best_axis = 2;
			best_div_val = centroid_aabb.min_[2] + axis_len_over_num_buckets[2] * (b + 1); //  + centroid_aabb.axisLength(2) * (b + 1) / num_buckets;
			//best_left_aabb = left_aabb[2];
		}
	}

	best_axis_out = best_axis;
	best_div_val_out = best_div_val;
	smallest_split_cost_factor_out = smallest_split_cost_factor;
}


/*
Recursively build a subtree.
Assumptions: root node for subtree is already created and is at node_index
*/
void BinningBVHBuilder::doBuild(
			BinningPerThreadTempInfo& thread_temp_info,
			const js::AABBox& aabb,
			const js::AABBox& centroid_aabb,
			uint32 node_index, 
			int left, 
			int right, 
			int depth,
			uint64 sort_key,
			BinningResultChunk& result_chunk
			)
{
	const int MAX_DEPTH = 60;

	//if(left == 180155 && right == 180157)
	//	int a = 9;

	js::Vector<ResultNode, 64>& chunk_nodes = thread_temp_info.result_buf;

	if(right - left <= leaf_num_object_threshold || depth >= MAX_DEPTH)
	{
		assert(right - left <= max_num_objects_per_leaf);

		// Make this a leaf node
		chunk_nodes[node_index].interior = false;
		chunk_nodes[node_index].aabb = aabb;
		chunk_nodes[node_index].left = left;
		chunk_nodes[node_index].right = right;
		chunk_nodes[node_index].depth = (uint8)depth;

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
	float smallest_split_cost_factor;// = std::numeric_limits<float>::infinity(); // Smallest (N_L * half_left_surface_area + N_R * half_right_surface_area) found.
//	int best_N_L = -1;
	int best_axis;// = -1;
	float best_div_val;// = 0;
	//js::AABBox best_left_aabb;

	//conPrint("Looking for best split...");
	//Timer timer;

	search(centroid_aabb, objects, left, right, best_axis, best_div_val, smallest_split_cost_factor);
	
	//conPrint("Looking for best split done.  elapsed: " + timer.elapsedString());
	//split_search_time += timer.elapsed();

	if(best_axis == -1) // This can happen when all object centres coordinates along the axis are the same.
	{
		doArbitrarySplits(thread_temp_info, aabb, node_index, left, right, depth, result_chunk);
		return;
	}

	//setAABBWToOne(best_left_aabb);
	//assert(aabb.containsAABBox(best_left_aabb));

	// NOTE: the factor of 2 compensates for the surface area vars being half the areas.
	const float smallest_split_cost = 2 * intersection_cost * smallest_split_cost_factor / aabb.getSurfaceArea() + traversal_cost; // Eqn 1.

	// If it is not advantageous to split, and the number of objects is <= the max num per leaf, then form a leaf:
	if((smallest_split_cost >= non_split_cost) && ((right - left) <= max_num_objects_per_leaf))
	{
		chunk_nodes[node_index].interior = false;
		chunk_nodes[node_index].aabb = aabb;
		chunk_nodes[node_index].left = left;
		chunk_nodes[node_index].right = right;
		chunk_nodes[node_index].depth = (uint8)depth;

		// Update build stats
		thread_temp_info.stats.num_cheaper_nosplit_leaves++;
		thread_temp_info.stats.leaf_depth_sum += depth;
		thread_temp_info.stats.max_leaf_depth = myMax(thread_temp_info.stats.max_leaf_depth, depth);
		thread_temp_info.stats.max_num_tris_per_leaf = myMax(thread_temp_info.stats.max_num_tris_per_leaf, right - left);
		thread_temp_info.stats.num_leaves++;
		return;
	}
	
	//timer.reset();
	PartitionRes res;
	partition(objects, left, right, best_div_val, best_axis, res);
	//conPrint("Partition done.  elapsed: " + timer.elapsedString());

	const int num_left_tris = res.cur - left;

	setAABBWToOne(res.left_aabb);
	setAABBWToOne(res.left_centroid_aabb);
	setAABBWToOne(res.right_aabb);
	setAABBWToOne(res.right_centroid_aabb);

	assert(aabb.containsAABBox(res.left_aabb));
	assert(res.left_aabb.containsAABBox(res.left_centroid_aabb));
	assert(centroid_aabb.containsAABBox(res.left_centroid_aabb));
	assert(aabb.containsAABBox(res.right_aabb));
	assert(res.right_aabb.containsAABBox(res.right_centroid_aabb));
	assert(centroid_aabb.containsAABBox(res.right_centroid_aabb));

	// Check partitioning
#ifndef NDEBUG
	for(int i=left; i<left + num_left_tris; ++i)
		assert(objects[i].aabb.centroid()[best_axis] <= best_div_val);
	
	for(int i=left + num_left_tris; i<right; ++i)
		assert(objects[i].aabb.centroid()[best_axis] > best_div_val);
#endif

	if(num_left_tris == 0 || num_left_tris == right - left)
	{
		// Split objects arbitrarily until the number in each leaf is <= max_num_objects_per_leaf.
		doArbitrarySplits(thread_temp_info, aabb, node_index, left, right, depth, result_chunk);
		return;
	}

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
	//setAABBWToOne(best_right_aabb);
	chunk_nodes[node_index].interior = true;
	chunk_nodes[node_index].aabb = aabb;
	chunk_nodes[node_index].left = left_child;
	chunk_nodes[node_index].right = right_child;
	chunk_nodes[node_index].right_child_chunk_index = -1;
	chunk_nodes[node_index].depth = (uint8)depth;

	thread_temp_info.stats.num_interior_nodes++;

	// Build left child
	doBuild(
		thread_temp_info,
		res.left_aabb, // aabb
		res.left_centroid_aabb,
		left_child, // node index
		left, // left
		split_i, // right
		depth + 1, // depth
		sort_key << 1, // sort key
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
		Reference<BinningBuildSubtreeTask> subtree_task = new BinningBuildSubtreeTask(*this);
		subtree_task->result_chunk = &result_chunks[chunk_index];
		subtree_task->node_aabb = res.right_aabb;
		subtree_task->centroid_aabb = res.right_centroid_aabb;
		subtree_task->depth = depth + 1;
		subtree_task->sort_key = (sort_key << 1) | 1;
		subtree_task->begin = split_i;
		subtree_task->end = right;
		task_manager->addTask(subtree_task);
	}
	else
	{
		// Build right child
		doBuild(
			thread_temp_info,
			res.right_aabb, // aabb
			res.right_centroid_aabb,
			right_child, // node index
			split_i, // left
			right, // right
			depth + 1, // depth
			(sort_key << 1) | 1,
			result_chunk
		);
	}
}


// We may get multiple objects with the same bounding box.
// These objects can't be split in the usual way.
// Also the number of such objects assigned to a subtree may be > max_num_objects_per_leaf.
// In this case we will just divide the object list into two until the num per subtree is <= max_num_objects_per_leaf.
void BinningBVHBuilder::doArbitrarySplits(
		BinningPerThreadTempInfo& thread_temp_info,
		const js::AABBox& aabb,
		uint32 node_index, 
		int left, 
		int right, 
		int depth,
		BinningResultChunk& result_chunk
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
		chunk_nodes[node_index].depth = (uint8)depth;

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
	js::AABBox left_aabb  = empty_aabb;
	js::AABBox right_aabb = empty_aabb;

	for(int i=left; i<split_i; ++i)
		left_aabb.enlargeToHoldAABBox(objects[i].aabb);

	for(int i=split_i; i<right; ++i)
		right_aabb.enlargeToHoldAABBox(objects[i].aabb);

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
	chunk_nodes[node_index].depth = (uint8)depth;

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
		result_chunk
	);
}


void BinningBVHBuilder::printResultNode(const ResultNode& result_node)
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


void BinningBVHBuilder::printResultNodes(const js::Vector<ResultNode, 64>& result_nodes)
{
	for(size_t i=0; i<result_nodes.size(); ++i)
	{
		conPrintStr("node " + toString(i) + ": ");
		printResultNode(result_nodes[i]);
	}
}
