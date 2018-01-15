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


BinningBVHBuilder::BinningBVHBuilder(int leaf_num_object_threshold_, int max_num_objects_per_leaf_, float intersection_cost_,
	const js::AABBox* aabbs_,
	const int num_objects_
)
:	leaf_num_object_threshold(leaf_num_object_threshold_),
	max_num_objects_per_leaf(max_num_objects_per_leaf_),
	intersection_cost(intersection_cost_),
	aabbs(aabbs_),
	m_num_objects(num_objects_)
{
	assert(intersection_cost > 0.f);

	// See /wiki/index.php?title=BVH_Building for results on varying these settings.
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

		// Alloc space for node in already existing chunk, or allocate a new chunk if needed.
		int node_index;
		if(builder.per_thread_temp_info[thread_index].result_chunk == NULL)
		{
			builder.per_thread_temp_info[thread_index].result_chunk = builder.allocNewResultChunk();
			builder.per_thread_temp_info[thread_index].result_chunk->size = 1;
			node_index = 0;
		}
		else
		{
			if(builder.per_thread_temp_info[thread_index].result_chunk->size >= BinningResultChunk::MAX_RESULT_CHUNK_SIZE) // If the current chunk is full:
				builder.per_thread_temp_info[thread_index].result_chunk = builder.allocNewResultChunk();

			node_index = (int)(builder.per_thread_temp_info[thread_index].result_chunk->size++);
		}

		// Fix up reference from parent node to this node.
		if(parent_node_index != -1)
			parent_chunk->nodes[parent_node_index].right = (int)(node_index + builder.per_thread_temp_info[thread_index].result_chunk->chunk_offset);

		builder.doBuild(
			builder.per_thread_temp_info[thread_index],
			node_aabb,
			centroid_aabb,
			node_index,
			begin, end, depth,
			builder.per_thread_temp_info[thread_index].result_chunk
		);
	}

	
	BinningBVHBuilder& builder;
	js::AABBox node_aabb;
	js::AABBox centroid_aabb;
	int depth;
	int begin;
	int end;
	BinningResultChunk* parent_chunk;
	int parent_node_index;
};


BinningResultChunk* BinningBVHBuilder::allocNewResultChunk()
{
	BinningResultChunk* chunk = new BinningResultChunk();
	chunk->size = 0;

	Lock lock(result_chunks_mutex);
	chunk->chunk_offset = result_chunks.size() * BinningResultChunk::MAX_RESULT_CHUNK_SIZE;
	result_chunks.push_back(chunk);
	return chunk;
}


// top-level build method
void BinningBVHBuilder::build(
		   Indigo::TaskManager& task_manager_,
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
	const int num_objects = this->m_num_objects;

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
	
	
	per_thread_temp_info.resize(task_manager->getNumThreads());
	for(size_t i = 0; i < per_thread_temp_info.size(); ++i)
		per_thread_temp_info[i].result_chunk = NULL;

	} // End scope for initial init


	Reference<BinningBuildSubtreeTask> task = new BinningBuildSubtreeTask(*this);
	task->begin = 0;
	task->end = m_num_objects;
	task->depth = 0;
	task->node_aabb = root_aabb;
	task->centroid_aabb = centroid_aabb;
	task->parent_node_index = -1;
	task_manager->addTask(task);

	task_manager->waitForTasksToComplete();


	// Now we need to combine all the result chunks into a single array.

	//timer.reset();
	//Timer timer;

	js::Vector<uint32, 16> final_node_indices(result_chunks.size() * BinningResultChunk::MAX_RESULT_CHUNK_SIZE);
	uint32 write_i = 0;
	for(size_t c=0; c<result_chunks.size(); ++c)
	{
		for(size_t i=0; i<result_chunks[c]->size; ++i)
		{
			final_node_indices[c * BinningResultChunk::MAX_RESULT_CHUNK_SIZE + i] = write_i++;
		}
	}

	const int total_num_nodes = write_i;
	result_nodes_out.resizeNoCopy(write_i);

	for(size_t c=0; c<result_chunks.size(); ++c)
	{
		const BinningResultChunk& chunk = *result_chunks[c];
		size_t src_node_i = c * BinningResultChunk::MAX_RESULT_CHUNK_SIZE;

		for(size_t i=0; i<chunk.size; ++i)
		{
			ResultNode chunk_node = chunk.nodes[i];

			// If this is an interior node, we need to fix up some links.
			if(chunk_node.interior)
			{
				chunk_node.left  = final_node_indices[chunk_node.left];
				chunk_node.right = final_node_indices[chunk_node.right];

				assert(chunk_node.left  >= 0 && chunk_node.left  < total_num_nodes);
				assert(chunk_node.right >= 0 && chunk_node.right < total_num_nodes);
			}

			const uint32 final_pos = final_node_indices[src_node_i];
			result_nodes_out[final_pos] = chunk_node; // Copy node to final array
			src_node_i++;
		}

		delete result_chunks[c];
	}


	//conPrint("Final merge elapsed: " + timer.elapsedString());

	result_indices.resizeNoCopy(m_num_objects);
	for(int i=0; i<m_num_objects; ++i)
		result_indices[i] = objects[i].getIndex();


	// Combine stats from each thread
	for(size_t i=0; i<per_thread_temp_info.size(); ++i)
		this->stats.accumStats(per_thread_temp_info[i].stats);

	// Dump some mem usage stats
	if(true)
	{
		const double build_time = build_timer.elapsed();

		conPrint("----------- BinningBVHBuilder Build Stats ----------");
		conPrint("Mem usage:");
		conPrint("objects:                    " + getNiceByteSize(objects.dataSizeBytes()));

		conPrint("result_chunks:              " + getNiceByteSize(result_chunks.size() * sizeof(BinningResultChunk)));
		conPrint("result_nodes_out:           " + toString(result_nodes_out.size()) + " nodes * " + toString(sizeof(ResultNode)) + "B = " + getNiceByteSize(result_nodes_out.dataSizeBytes()));
		
		const size_t total_size = objects.dataSizeBytes() + (result_chunks.size() * sizeof(BinningResultChunk)) + result_nodes_out.dataSizeBytes();
		
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


// Partition half the list left and half right.
static void arbitraryPartition(js::Vector<BinningOb, 64>& objects_, int left, int right, PartitionRes& res_out)
{
	BinningOb* const objects = objects_.data();

	js::AABBox left_aabb = empty_aabb;
	js::AABBox left_centroid_aabb = empty_aabb;
	js::AABBox right_aabb = empty_aabb;
	js::AABBox right_centroid_aabb = empty_aabb;

	const int split_i = left + (right - left)/2;

	for(int i=left; i<split_i; ++i)
	{
		const js::AABBox aabb = objects[i].aabb;
		const Vec4f centroid = aabb.centroid();
		left_aabb.enlargeToHoldAABBox(aabb);
		left_centroid_aabb.enlargeToHoldPoint(centroid);
	}
	
	for(int i=split_i; i<right; ++i)
	{
		const js::AABBox aabb = objects[i].aabb;
		const Vec4f centroid = aabb.centroid();
		right_aabb.enlargeToHoldAABBox(aabb);
		right_centroid_aabb.enlargeToHoldPoint(centroid);
	}

	res_out.left_aabb = left_aabb;
	res_out.left_centroid_aabb = left_centroid_aabb;
	res_out.right_aabb = right_aabb;
	res_out.right_centroid_aabb = right_centroid_aabb;
	res_out.cur = split_i;
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
			uint32 node_index, // index in chunk nodes
			int left, 
			int right, 
			int depth,
			BinningResultChunk* node_result_chunk
			)
{
	const int MAX_DEPTH = 60;

	assert(node_index < BinningResultChunk::MAX_RESULT_CHUNK_SIZE);

	if(right - left <= leaf_num_object_threshold || depth >= MAX_DEPTH)
	{
		assert(right - left <= max_num_objects_per_leaf);

		// Make this a leaf node
		node_result_chunk->nodes[node_index].interior = false;
		node_result_chunk->nodes[node_index].aabb = aabb;
		node_result_chunk->nodes[node_index].left = left;
		node_result_chunk->nodes[node_index].right = right;
		node_result_chunk->nodes[node_index].depth = (uint8)depth;

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

	PartitionRes res;
	if(best_axis == -1) // This can happen when all object centres coordinates along the axis are the same.
	{
		arbitraryPartition(objects, left, right, res);
	}
	else
	{
		// NOTE: the factor of 2 compensates for the surface area vars being half the areas.
		const float smallest_split_cost = 2 * intersection_cost * smallest_split_cost_factor / aabb.getSurfaceArea() + traversal_cost; // Eqn 1.

		// If it is not advantageous to split, and the number of objects is <= the max num per leaf, then form a leaf:
		if((smallest_split_cost >= non_split_cost) && ((right - left) <= max_num_objects_per_leaf))
		{
			node_result_chunk->nodes[node_index].interior = false;
			node_result_chunk->nodes[node_index].aabb = aabb;
			node_result_chunk->nodes[node_index].left = left;
			node_result_chunk->nodes[node_index].right = right;
			node_result_chunk->nodes[node_index].depth = (uint8)depth;

			// Update build stats
			thread_temp_info.stats.num_cheaper_nosplit_leaves++;
			thread_temp_info.stats.leaf_depth_sum += depth;
			thread_temp_info.stats.max_leaf_depth = myMax(thread_temp_info.stats.max_leaf_depth, depth);
			thread_temp_info.stats.max_num_tris_per_leaf = myMax(thread_temp_info.stats.max_num_tris_per_leaf, right - left);
			thread_temp_info.stats.num_leaves++;
			return;
		}

		partition(objects, left, right, best_div_val, best_axis, res);

		// Check partitioning
#ifndef NDEBUG
		const int num_left_tris = res.cur - left;
		for(int i=left; i<left + num_left_tris; ++i)
			assert(objects[i].aabb.centroid()[best_axis] <= best_div_val);

		for(int i=left + num_left_tris; i<right; ++i)
			assert(objects[i].aabb.centroid()[best_axis] > best_div_val);
#endif
	}

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

	int num_left_tris = res.cur - left;

	if(num_left_tris == 0 || num_left_tris == right - left)
	{
		arbitraryPartition(objects, left, right, res);
		num_left_tris = res.cur - left;
	}

	//timer.reset();
	//partition_time += timer.elapsed();


	// Allocate left child from the thread's current result chunk
	if(thread_temp_info.result_chunk->size >= BinningResultChunk::MAX_RESULT_CHUNK_SIZE) // If the current chunk is full:
		thread_temp_info.result_chunk = allocNewResultChunk();

	const size_t left_child = thread_temp_info.result_chunk->size++;
	BinningResultChunk* left_child_chunk = thread_temp_info.result_chunk;


	const int split_i = left + num_left_tris;
	const bool do_right_child_in_new_task = (num_left_tris >= new_task_num_ob_threshold) && ((right - split_i) >= new_task_num_ob_threshold);// && !task_manager->areAllThreadsBusy();

	//if(do_right_child_in_new_task)
	//	conPrint("Splitting task: num left=" + toString(split_i - left) + ", num right=" + toString(right - split_i));
	//else
	//	conPrint("not splitting.");

	// Allocate right child
	BinningResultChunk* right_child_chunk;
	uint32 right_child;
	if(!do_right_child_in_new_task)
	{
		if(thread_temp_info.result_chunk->size >= BinningResultChunk::MAX_RESULT_CHUNK_SIZE) // If the current chunk is full:
			thread_temp_info.result_chunk = allocNewResultChunk();

		right_child = (uint32)(thread_temp_info.result_chunk->size++);
		right_child_chunk = thread_temp_info.result_chunk;
	}
	else
	{
		right_child_chunk = NULL; // not used
		right_child = 0; // not used
	}

	// Mark this node as an interior node.
	//setAABBWToOne(best_right_aabb);
	node_result_chunk->nodes[node_index].interior = true;
	node_result_chunk->nodes[node_index].aabb = aabb;
	node_result_chunk->nodes[node_index].left  = (int)(left_child  + left_child_chunk->chunk_offset);
	node_result_chunk->nodes[node_index].right = do_right_child_in_new_task ? -1 : (int)(right_child + right_child_chunk->chunk_offset); // will be fixed-up later
	node_result_chunk->nodes[node_index].right_child_chunk_index = -1;
	node_result_chunk->nodes[node_index].depth = (uint8)depth;

	thread_temp_info.stats.num_interior_nodes++;

	// Build left child
	doBuild(
		thread_temp_info,
		res.left_aabb, // aabb
		res.left_centroid_aabb,
		(int)left_child, // node index
		left, // left
		split_i, // right
		depth + 1, // depth
		left_child_chunk
	);

	if(do_right_child_in_new_task)
	{
		// Put this subtree into a task
		Reference<BinningBuildSubtreeTask> subtree_task = new BinningBuildSubtreeTask(*this);
		subtree_task->node_aabb = res.right_aabb;
		subtree_task->centroid_aabb = res.right_centroid_aabb;
		subtree_task->depth = depth + 1;
		subtree_task->begin = split_i;
		subtree_task->end = right;
		subtree_task->parent_node_index = (int)(node_index);
		subtree_task->parent_chunk = node_result_chunk;

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
			right_child_chunk
		);
	}
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
