/*=====================================================================
BinningBVHBuilder.cpp
---------------------
Copyright Glare Technologies Limited 2020 -
Generated at Tue Apr 27 15:25:47 +1200 2010
=====================================================================*/
#include "BinningBVHBuilder.h"


/*
Dev notes
---------
I tried parallelising the partition function. Results:

With MT'd partition
==================

BVHBuilderTests perf tests:
---------------------------
av_time:  0.13871507287141413 s
min_time: 0.1354363030463901 s

bedroom:
--------
tritree->build: 0.6952134609564382 s
OpenCLPTSceneBuilder::buildMeshBVH(): BVH build took 0.4618 s

Without MT'd partition
======================

BVHBuilderTests perf tests:
---------------------------
av_time:  0.1436770330129184 s
min_time: 0.13933144048314716 s

bedroom:
--------
tritree->build: 0.6083853368400014 s
OpenCLPTSceneBuilder::buildMeshBVH(): BVH build took 0.3879 s




So the MT'd partition, while faster on the BVHBuilderTests perf tests, is quite a bit slower on the bedroom scene.  So we won't use that code.
MT'd partition is probably slower on the bedroom scene due to the increased memory usage, as the partition cannot be in-place any more.

See 'physics\experiments\BinningBVHBuilder with parallel partition.cpp' for the code.

*/

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
#include "../utils/ShouldCancelCallback.h"
#include "../utils/CountCondition.h"


static const js::AABBox empty_aabb = js::AABBox::emptyAABBox();


BinningBVHBuilder::BinningBVHBuilder(int leaf_num_object_threshold_, int max_num_objects_per_leaf_, int max_depth_, float intersection_cost_,
	const int num_objects_
)
:	leaf_num_object_threshold(leaf_num_object_threshold_),
	max_num_objects_per_leaf(max_num_objects_per_leaf_),
	max_depth(max_depth_),
	intersection_cost(intersection_cost_),
	m_num_objects(num_objects_),
	should_cancel_callback(NULL)
{
	assert(max_depth >= 0);
	assert(intersection_cost > 0.f);

	// See /wiki/index.php?title=BVH_Building, in particular wiki/index.php?title=Task_size_experiments for results on varying these settings.
	new_task_num_ob_threshold = 1 << 9;

	static_assert(sizeof(ResultNode) == 48, "sizeof(ResultNode) == 48");

	this->objects.resizeNoCopy(m_num_objects);
	this->result_indices.resizeNoCopy(m_num_objects);
	root_aabb = empty_aabb;
	root_centroid_aabb = empty_aabb;
}


BinningBVHBuilder::~BinningBVHBuilder()
{
	// This might have non-zero size if the build was cancelled.
	for(size_t i=0; i<result_chunks.size(); ++i)
		delete result_chunks[i];
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


// Returns node index in thread_temp_info.result_chunk
// May update thread_temp_info.result_chunk to point to a new chunk (if we ran out of space in the last chunk)
uint32 BinningBVHBuilder::allocNode(BinningPerThreadTempInfo& thread_temp_info)
{
	if(thread_temp_info.result_chunk->size >= BinningResultChunk::MAX_RESULT_CHUNK_SIZE) // If the current chunk is full:
		thread_temp_info.result_chunk = allocNewResultChunk();

	const size_t index = thread_temp_info.result_chunk->size++;
	return (uint32)index;
}


/*
Builds a subtree with the objects[begin] ... objects[end-1]
Writes its results to the per-thread buffer 'per_thread_temp_info[thread_index].result_buf', and describes the result in result_chunk,
which is added to the global result_chunks list.

May spawn new BuildSubtreeTasks.
*/
class BinningBuildSubtreeTask : public glare::Task
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	BinningBuildSubtreeTask(BinningBVHBuilder& builder_) : builder(builder_) {}
	
	virtual void run(size_t thread_index)
	{
		ScopeProfiler _scope("BinningBuildSubtreeTask", (int)thread_index);

		// Flush denormals to zero.  This is important otherwise w values storing low integer values get interpreted as denormals, which drastically reduces performance.
		_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
		_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

		// Allocate a new chunk if needed.
		if(builder.per_thread_temp_info[thread_index].result_chunk == NULL)
			builder.per_thread_temp_info[thread_index].result_chunk = builder.allocNewResultChunk();

		const uint32 node_index = builder.allocNode(builder.per_thread_temp_info[thread_index]);

		// Fix up reference from parent node to this node.
		if(parent_node_index != -1)
			parent_chunk->nodes[parent_node_index].right = (int)(node_index + builder.per_thread_temp_info[thread_index].result_chunk->chunk_offset);

		try
		{
			builder.doBuild(
				builder.per_thread_temp_info[thread_index],
				node_aabb,
				centroid_aabb,
				node_index,
				begin, end, depth,
				builder.per_thread_temp_info[thread_index].result_chunk
			);
		}
		catch(glare::CancelledException&)
		{}
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


// Top-level build method
// Throws glare::CancelledException if cancelled.
void BinningBVHBuilder::build(
		glare::TaskManager& task_manager_,
		ShouldCancelCallback& should_cancel_callback_,
		PrintOutput& print_output, 
		js::Vector<ResultNode, 64>& result_nodes_out
		)
{
	Timer build_timer;
	ScopeProfiler _scope("BVHBuilder::build");

	// Flush denormals to zero.  This is important otherwise w values storing low integer values get interpreted as denormals, which drastically reduces performance.
	SetFlushDenormsMode flusher;

	result_nodes_out.clear();
	//------------ Reset builder state --------------
	// Not clearing objects and result_indices as are alloced in constructor
	per_thread_temp_info.clear();
	for(size_t i=0; i<result_chunks.size(); ++i)
		delete result_chunks[i];
	result_chunks.clear();
	stats = BinningBVHBuildStats();
	//------------ End reset builder state --------------
	
	//Timer timer;
	split_search_time = 0;
	partition_time = 0;

	this->task_manager = &task_manager_;
	this->should_cancel_callback = &should_cancel_callback_;
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
		stats.max_leaf_depth = 0;
		stats.num_leaves = 1;
		return;
	}

	// Build max_obs_at_depth
	{
		max_obs_at_depth.resize(max_depth + 1);
		uint64 m = max_num_objects_per_leaf;
		for(int d=max_depth; d>=0; --d)
		{
			max_obs_at_depth[d] = m;
			if(m <= std::numeric_limits<uint64>::max() / 2) // Avoid overflow
				m *= 2;
		}

		//for(int d=0; d<max_obs_at_depth.size(); ++d)
		//	conPrint("Max obs at depth " + toString(d) + ": " + toString(max_obs_at_depth[d]));
	}


	
	per_thread_temp_info.resize(task_manager->getNumThreads());
	for(size_t i = 0; i < per_thread_temp_info.size(); ++i)
	{
		per_thread_temp_info[i].result_chunk = NULL;
		per_thread_temp_info[i].build_failed = false;
	}

	Reference<BinningBuildSubtreeTask> task = new BinningBuildSubtreeTask(*this);
	task->begin = 0;
	task->end = m_num_objects;
	task->depth = 0;
	task->node_aabb = root_aabb;
	task->centroid_aabb = root_centroid_aabb;
	task->parent_node_index = -1;
	task_manager->addTask(task);

	task_manager->waitForTasksToComplete();

	if(should_cancel_callback->shouldCancel()) 
		throw glare::CancelledException();

	// See if the build failed:
	for(size_t i = 0; i < per_thread_temp_info.size(); ++i)
		if(per_thread_temp_info[i].build_failed)
			throw glare::Exception("Build failed.");

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

#ifndef NDEBUG
	const int total_num_nodes = write_i;
#endif
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
	result_chunks.clear();


	//conPrint("Final merge elapsed: " + timer.elapsedString());


	// Combine stats from each thread
	for(size_t i=0; i<per_thread_temp_info.size(); ++i)
		this->stats.accumStats(per_thread_temp_info[i].stats);

	// Dump some stats
	if(false)
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


static const int max_B = 32;

static inline int numBucketsForNumObs(int num_obs)
{
	return myMin(max_B, (int)(4 + 0.05f * num_obs));
}


/*
In-place partitioning:

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


// Partition objects in cur_objects for the given axis, based on best_bucket of best_axis.
struct PartitionRes
{
	js::AABBox left_aabb, left_centroid_aabb;
	js::AABBox right_aabb, right_centroid_aabb;
	int split_i; // Index of first object that was partitioned to the right.
};

static void partition(js::Vector<BinningOb, 64>& objects_, const js::AABBox& centroid_aabb, int begin, int end, int best_axis, int best_bucket, PartitionRes& res_out)
{
	BinningOb* const objects = objects_.data();

	js::AABBox left_aabb = empty_aabb;
	js::AABBox left_centroid_aabb = empty_aabb;
	js::AABBox right_aabb = empty_aabb;
	js::AABBox right_centroid_aabb = empty_aabb;
	int cur = begin;
	int other = end - 1;

	/*for(int i=0; i<8; ++i)
	{
		_mm_prefetch((const char*)(objects + left  + i), _MM_HINT_T0);
		_mm_prefetch((const char*)(objects + right - i), _MM_HINT_T0);
	}*/

	const int num_buckets = numBucketsForNumObs(end - begin); // buckets per axis
	const Vec4f scale = div(Vec4f((float)num_buckets), (centroid_aabb.max_ - centroid_aabb.min_));

	while(cur <= other)
	{
		//_mm_prefetch((const char*)(objects + cur   + 32), _MM_HINT_T0);
		//_mm_prefetch((const char*)(objects + other - 32), _MM_HINT_T0);

		const js::AABBox aabb = objects[cur].aabb;
		const Vec4f centroid = aabb.centroid();

		//const Vec4i bucket_i = clamp(truncateToVec4i((centroid - centroid_aabb.min_) * scale), Vec4i(0), Vec4i(num_buckets-1));
		const Vec4i bucket_i = truncateToVec4i((centroid - centroid_aabb.min_) * scale); // Clamping shouldn't be needed here
		if(bucket_i[best_axis] <= best_bucket)
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
	res_out.split_i = cur;
}


// Partition half the list left and half right.
static void arbitraryPartition(const js::Vector<BinningOb, 64>& objects_, int begin, int end, PartitionRes& res_out)
{
	const BinningOb* const objects = objects_.data();

	js::AABBox left_aabb = empty_aabb;
	js::AABBox left_centroid_aabb = empty_aabb;
	js::AABBox right_aabb = empty_aabb;
	js::AABBox right_centroid_aabb = empty_aabb;

	const int split_i = begin + (end - begin)/2;

	for(int i=begin; i<split_i; ++i)
	{
		const js::AABBox aabb = objects[i].aabb;
		const Vec4f centroid = aabb.centroid();
		left_aabb.enlargeToHoldAABBox(aabb);
		left_centroid_aabb.enlargeToHoldPoint(centroid);
	}
	
	for(int i=split_i; i<end; ++i)
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
	res_out.split_i = split_i;
}


// Since we store the object index in the AABB.min_.w, we want to restore this value to 1 before we return it to the client code.
static inline void setAABBWToOneInPlace(js::AABBox& aabb)
{
	aabb.min_ = setWToOne(aabb.min_);
	aabb.max_ = setWToOne(aabb.max_);
}


class BinTask : public glare::Task
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	virtual void run(size_t thread_index)
	{
		if(started.increment() > 0) // If another thread has started this task (e.g. if old value is > 0), skip it.
			return;

		// Flush denormals to zero.  This is important otherwise w values storing low integer values get interpreted as denormals, which drastically reduces performance.
		_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
		_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

		const js::AABBox centroid_aabb = this->centroid_aabb_;
		const BinningOb* const objects = objects_->data();

		const int num_buckets = numBucketsForNumObs(end - begin); // buckets per axis

		js::AABBox* const bucket_aabbs = this->bucket_aabbs_;
		Vec4i* const counts = this->counts_;

		// Zero our local AABBs and counts
		for(int i=0; i<num_buckets; ++i)
		{
			for(int z=0; z<3; ++z)
				bucket_aabbs[z*max_B + i] = empty_aabb;
			counts[i] = Vec4i(0);
		}

		const Vec4f scale = div(Vec4f((float)num_buckets), (centroid_aabb.max_ - centroid_aabb.min_));
		for(int i=task_begin; i<task_end; ++i)
		{
			const js::AABBox ob_aabb = objects[i].aabb;
			const Vec4f centroid = ob_aabb.centroid();
			assert(centroid_aabb.contains(Vec4f(centroid[0], centroid[1], centroid[2], 1.f)));
			const Vec4i bucket_i = clamp(truncateToVec4i((centroid - centroid_aabb.min_) * scale), Vec4i(0), Vec4i(num_buckets-1));

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

		num_done_condition->increment();
	}

	js::AABBox bucket_aabbs_[max_B * 3];
	Vec4i counts_[max_B];

	js::AABBox centroid_aabb_;
	const js::Vector<BinningOb, 64>* objects_;
	int begin, end, task_begin, task_end;

	IndigoAtomic started;
	CountCondition* num_done_condition;

	char padding[64]; // to avoid false sharing
};


// inline static float costEstimate(/*float intersection_cost, */int num_obs) // Without surface area / P_A
// {
// 	const float intersection_cost = 1.f;
// 	const float av_P_A_plus_B = 1.45f; // 1.26778367717;
// 	const float av_num_obs_per_leaf = 2.5096484422267937f;
// 	const float better_est_cost = pow(av_P_A_plus_B, logBase2<float>((float)num_obs)) * (/*traversal cost*/1.f + intersection_cost * av_num_obs_per_leaf);
// 	return better_est_cost;
// }


static void searchForBestSplit(glare::TaskManager& task_manager, const js::AABBox& centroid_aabb_, const std::vector<uint64>& max_obs_at_depth, int depth, js::Vector<BinningOb, 64>& objects_, int begin, int end,
	int& best_axis_out, float& smallest_split_cost_factor_out, int& best_bucket_out)
{
	const js::AABBox centroid_aabb = centroid_aabb_;
	const BinningOb* const objects = objects_.data();

	float smallest_split_cost_factor = std::numeric_limits<float>::infinity(); // Smallest (N_L * half_left_surface_area + N_R * half_right_surface_area) found.
	int best_axis = -1;
	int best_bucket = -1;

	const int num_buckets = numBucketsForNumObs(end - begin); // buckets per axis
	js::AABBox bucket_aabbs[max_B * 3];
	Vec4i counts[max_B];
	for(int i=0; i<num_buckets; ++i)
	{
		for(int z=0; z<3; ++z)
			bucket_aabbs[z*max_B + i] = empty_aabb;
		counts[i] = Vec4i(0);
	}

	const int N = end - begin;
	const int task_N = 1 << 16;
	if(N >= task_N) // If there are enough objects, do parallel binning:
	{
		const int MAX_NUM_TASKS = 64;
		const int num_tasks = myClamp((int)task_manager.getNumThreads(), 1, MAX_NUM_TASKS);

		Reference<BinTask> tasks[MAX_NUM_TASKS];
		
		const int num_per_task = Maths::roundedUpDivide(N, num_tasks);

		CountCondition num_done_condition(num_tasks);

		for(int i=0; i<num_tasks; ++i)
		{
			tasks[i] = new BinTask();
			BinTask* task = tasks[i].ptr();
			task->objects_ = &objects_;
			task->centroid_aabb_ = centroid_aabb;
			task->begin = begin;
			task->end = end;
			task->task_begin = myMin(begin + i       * num_per_task, end);
			task->task_end   = myMin(begin + (i + 1) * num_per_task, end);
			task->num_done_condition = &num_done_condition;

			assert(task->task_begin >= begin && task->task_begin <= end && task->task_end >= task->task_begin && task->task_end <= end);
		}
		task_manager.addTasks(ArrayRef<glare::TaskRef>((Reference<glare::Task>*)tasks, num_tasks));
		
		// Try and execute the tasks in this thread, so that we know we will make progress on these tasks.
		for(int i=0; i<num_tasks; ++i)
			tasks[i]->run(/*thread index (not used)=*/0);

		// Wait until all tasks are done.
		num_done_condition.wait();


		// Merge bucket AABBs and counts from each task
		for(int t=0; t<num_tasks; ++t)
		{
			for(int i=0; i<num_buckets; ++i)
			{
				for(int z=0; z<3; ++z)
					bucket_aabbs[z*max_B + i].enlargeToHoldAABBox(tasks[t]->bucket_aabbs_[z*max_B + i]);
				counts[i] = counts[i] + tasks[t]->counts_[i];
			}
		}

		// Check against single-threaded reference counts and AABBs:
#ifndef NDEBUG
		js::AABBox ref_bucket_aabbs[max_B * 3];
		Vec4i ref_counts[max_B];
		for(int i=0; i<num_buckets; ++i)
		{
			for(int z=0; z<3; ++z)
				ref_bucket_aabbs[z*max_B + i] = empty_aabb;
			ref_counts[i] = Vec4i(0);
		}

		const Vec4f scale = div(Vec4f((float)num_buckets), (centroid_aabb.max_ - centroid_aabb.min_));
		for(int i=begin; i<end; ++i)
		{
			const js::AABBox ob_aabb = objects[i].aabb;
			const Vec4f centroid = ob_aabb.centroid();
			assert(centroid_aabb.contains(Vec4f(centroid[0], centroid[1], centroid[2], 1.f)));
			const Vec4i bucket_i = clamp(truncateToVec4i((centroid - centroid_aabb.min_) * scale), Vec4i(0), Vec4i(num_buckets-1));

			assert(bucket_i[0] >= 0 && bucket_i[0] < num_buckets);
			assert(bucket_i[1] >= 0 && bucket_i[1] < num_buckets);
			assert(bucket_i[2] >= 0 && bucket_i[2] < num_buckets);

			// X axis:
			const int b_x = elem<0>(bucket_i);
			ref_bucket_aabbs[b_x].enlargeToHoldAABBox(ob_aabb);
			(ref_counts[b_x])[0]++;

			// Y axis:
			const int b_y = elem<1>(bucket_i);
			ref_bucket_aabbs[max_B + b_y].enlargeToHoldAABBox(ob_aabb);
			(ref_counts[b_y])[1]++;

			// Z axis:
			const int b_z = elem<2>(bucket_i);
			ref_bucket_aabbs[max_B*2 + b_z].enlargeToHoldAABBox(ob_aabb);
			(ref_counts[b_z])[2]++;
		}

		for(int i=0; i<num_buckets; ++i)
		{
			for(int z=0; z<3; ++z)
				assert(bucket_aabbs[z*max_B + i] == ref_bucket_aabbs[z*max_B + i]);
			assert(counts[i] == ref_counts[i]);
		}
#endif
	}
	else // Else do serial binning:
	{
		const Vec4f scale = div(Vec4f((float)num_buckets), (centroid_aabb.max_ - centroid_aabb.min_));
		for(int i=begin; i<end; ++i)
		{
			const js::AABBox ob_aabb = objects[i].aabb;
			const Vec4f centroid = ob_aabb.centroid();
			assert(centroid_aabb.contains(Vec4f(centroid[0], centroid[1], centroid[2], 1.f)));
			const Vec4i bucket_i = clamp(truncateToVec4i((centroid - centroid_aabb.min_) * scale), Vec4i(0), Vec4i(num_buckets-1)); // We could maybe remove the max part of the clamp, is dangerous tho, and int max is fast.

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
	}

	const uint64 max_num_obs_at_depth = max_obs_at_depth[depth];
	if((uint64)N * 2 >= max_num_obs_at_depth) // If we are getting close to the max number of objects allowed at this depth:
	{
		// Choose bucket so it splits objects as evenly as possible in terms of numbers, left and right
		int max_min_count = -1;
		Vec4i left_count(0);
		for(int b=0; b<num_buckets-1; ++b)
		{
			left_count = left_count + counts[b];
			const Vec4i right_count = Vec4i(N) - Vec4i(left_count);
			const Vec4i min_counts = min(left_count, right_count);
			if(min_counts[0] > max_min_count)
			{
				max_min_count = min_counts[0];
				best_bucket = b;
				best_axis = 0;
			}
			if(min_counts[1] > max_min_count)
			{
				max_min_count = min_counts[1];
				best_bucket = b;
				best_axis = 1;
			}
			if(min_counts[2] > max_min_count)
			{
				max_min_count = min_counts[2];
				best_bucket = b;
				best_axis = 2;
			}
		}

		smallest_split_cost_factor = 0; // What should this be set to? Set to zero for now to avoid creating leaves instead
	}
	else
	{
		// Sweep right to left computing exclusive prefix surface areas
		Vec4f right_area[max_B];
		js::AABBox right_aabb[3];
		for(int i=0; i<3; ++i)
			right_aabb[i] = empty_aabb;

		for(int b=num_buckets-1; b>=0; --b)
		{
			const float A0 = right_aabb[0].getHalfSurfaceArea(); // AABB w coord will be garbage due to index being stored, but getHalfSurfaceArea() ignores w coord.
			const float A1 = right_aabb[1].getHalfSurfaceArea();
			const float A2 = right_aabb[2].getHalfSurfaceArea();
			right_area[b] = Vec4f(A0, A1, A2, A2);

			right_aabb[0].enlargeToHoldAABBox(bucket_aabbs[b]);
			right_aabb[1].enlargeToHoldAABBox(bucket_aabbs[b +   max_B]);
			right_aabb[2].enlargeToHoldAABBox(bucket_aabbs[b + 2*max_B]);
		}

		// Sweep left to right, computing inclusive left prefix surface area and counts, and computing overall cost factor
		Vec4i count(0);
		js::AABBox left_aabb[3];
		for(int i=0; i<3; ++i)
			left_aabb[i] = empty_aabb;

		for(int b=0; b<num_buckets-1; ++b)
		{
			count = count + counts[b];

			left_aabb[0].enlargeToHoldAABBox(bucket_aabbs[b]);
			left_aabb[1].enlargeToHoldAABBox(bucket_aabbs[b +   max_B]);
			left_aabb[2].enlargeToHoldAABBox(bucket_aabbs[b + 2*max_B]);

			const float A0 = left_aabb[0].getHalfSurfaceArea();
			const float A1 = left_aabb[1].getHalfSurfaceArea();
			const float A2 = left_aabb[2].getHalfSurfaceArea();

			const Vec4i right_count = Vec4i(N) - Vec4i(count);
			const Vec4f cost = toVec4f(count) * Vec4f(A0, A1, A2, A2) + toVec4f(right_count) * right_area[b]; // Compute SAH cost factor

			/*if(depth < 0)
			{
				Vec4f left_cost = Vec4f(A0, A1, A2, A2) * Vec4f(
					costEstimate(count[0]),
					costEstimate(count[1]),
					costEstimate(count[2]),
					0
				);

				Vec4f right_cost = right_area[b] * Vec4f(
					costEstimate(right_count[0]),
					costEstimate(right_count[1]),
					costEstimate(right_count[2]),
					0
				);

				cost = left_cost + right_cost; // Compute SAH cost factor
			}*/

			//if(depth < 2)
			//	conPrint("bucket: " + toString(b) + ", cost: " + cost.toString());

			if(smallest_split_cost_factor > elem<0>(cost))
			{
				smallest_split_cost_factor = elem<0>(cost);
				best_axis = 0;
				best_bucket = b;
			}

			if(smallest_split_cost_factor > elem<1>(cost))
			{
				smallest_split_cost_factor = elem<1>(cost);
				best_axis = 1;
				best_bucket = b;
			}

			if(smallest_split_cost_factor > elem<2>(cost))
			{
				smallest_split_cost_factor = elem<2>(cost);
				best_axis = 2;
				best_bucket = b;
			}
		}
	}
	//if(depth < 2)
	//	conPrint("search result for begin: " + toString(begin) + ", end: " + toString(end) + ", cost: " + toString(smallest_split_cost_factor) + ", Best bucket: " + toString(best_bucket) + " / " + toString(num_buckets) + ", best axis: " + toString(best_axis));

	best_axis_out = best_axis;
	smallest_split_cost_factor_out = smallest_split_cost_factor;
	best_bucket_out = best_bucket;
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
			int begin, 
			int end, 
			int depth,
			BinningResultChunk* node_result_chunk
			)
{
	if(depth <= 5)
	{
		// conPrint("BinningBVHBuilder(): Checking for cancel at depth " + toString(depth));
		if(should_cancel_callback->shouldCancel())
		{
			// conPrint("BinningBVHBuilder(): Cancelling!");
			throw glare::CancelledException();
		}
	}

	assert(node_index < BinningResultChunk::MAX_RESULT_CHUNK_SIZE);

	const int N = end - begin;
	if(N <= leaf_num_object_threshold || depth >= max_depth)
	{
		if(N > max_num_objects_per_leaf)
		{
			// We hit max depth but there are too many objects for a leaf.  So the build has failed.
			thread_temp_info.build_failed = true;
			return;
		}

		// Make this a leaf node
		node_result_chunk->nodes[node_index].interior = false;
		node_result_chunk->nodes[node_index].aabb = aabb;
		node_result_chunk->nodes[node_index].left = begin;
		node_result_chunk->nodes[node_index].right = end;
		node_result_chunk->nodes[node_index].depth = (uint8)depth;

		for(int i=begin; i<end; ++i)
			result_indices[i] = objects[i].getIndex();

		// Update build stats
		if(depth >= max_depth)
			thread_temp_info.stats.num_maxdepth_leaves++;
		else
			thread_temp_info.stats.num_under_thresh_leaves++;
		thread_temp_info.stats.leaf_depth_sum += depth;
		thread_temp_info.stats.max_leaf_depth = myMax(thread_temp_info.stats.max_leaf_depth, depth);
		thread_temp_info.stats.max_num_tris_per_leaf = myMax(thread_temp_info.stats.max_num_tris_per_leaf, N);
		thread_temp_info.stats.num_leaves++;
		return;
	}
	
	float smallest_split_cost_factor; // Smallest (N_L * half_left_surface_area + N_R * half_right_surface_area) found.
	int best_axis;
	int best_bucket;

	//conPrint("Looking for best split...");
	//Timer timer;
	searchForBestSplit(*task_manager, centroid_aabb, max_obs_at_depth, depth, objects, begin, end, best_axis, smallest_split_cost_factor, best_bucket);
	//conPrint("Looking for best split done.  elapsed: " + timer.elapsedString());
	//split_search_time += timer.elapsed();

	PartitionRes res;
	if(best_axis == -1) // This can happen when all object centre coordinates along the axis are the same.
	{
		arbitraryPartition(objects, begin, end, res);
	}
	else
	{
		const float traversal_cost = 1.0f;
		const float non_split_cost = N * intersection_cost;

		// C_split = P_L N_L C_i + P_R N_R C_i + C_t
		//         = A_L/A N_L C_i + A_R/A N_R C_i + C_t
		//         = C_i (A_L N_L + A_R N_R) / A + C_t
		//         = C_i (A_L/2 N_L + A_R/2 N_R) / (A/2) + C_t
		const float smallest_split_cost = intersection_cost * smallest_split_cost_factor / aabb.getHalfSurfaceArea() + traversal_cost;

		// If it is not advantageous to split, and the number of objects is <= the max num per leaf, then form a leaf:
		if((smallest_split_cost >= non_split_cost) && (N <= max_num_objects_per_leaf))
		{
			node_result_chunk->nodes[node_index].interior = false;
			node_result_chunk->nodes[node_index].aabb = aabb;
			node_result_chunk->nodes[node_index].left = begin;
			node_result_chunk->nodes[node_index].right = end;
			node_result_chunk->nodes[node_index].depth = (uint8)depth;

			for(int i=begin; i<end; ++i)
				result_indices[i] = objects[i].getIndex();

			// Update build stats
			thread_temp_info.stats.num_cheaper_nosplit_leaves++;
			thread_temp_info.stats.leaf_depth_sum += depth;
			thread_temp_info.stats.max_leaf_depth = myMax(thread_temp_info.stats.max_leaf_depth, depth);
			thread_temp_info.stats.max_num_tris_per_leaf = myMax(thread_temp_info.stats.max_num_tris_per_leaf, end - begin);
			thread_temp_info.stats.num_leaves++;
			return;
		}

		partition(objects, centroid_aabb, begin, end, best_axis, best_bucket, res);
	}

	int num_left_tris = res.split_i - begin;
	int num_right_tris = end - res.split_i;

	const uint64 max_num_obs_at_depth_plus_1 = max_obs_at_depth[myMin((int)max_obs_at_depth.size() - 1, depth + 1)];
	if((uint64)num_left_tris > max_num_obs_at_depth_plus_1 || (uint64)num_right_tris > max_num_obs_at_depth_plus_1) // If we have exceeded the max number of objects allowed at a given depth:
	{
		// Re-partition, splitting as evenly as possible in terms of numbers left and right.
		arbitraryPartition(objects, begin, end, res);

		num_left_tris = res.split_i - begin;
		num_right_tris = end - res.split_i;
	}

	setAABBWToOneInPlace(res.left_aabb);
	setAABBWToOneInPlace(res.left_centroid_aabb);
	setAABBWToOneInPlace(res.right_aabb);
	setAABBWToOneInPlace(res.right_centroid_aabb);

	assert(aabb.containsAABBox(res.left_aabb));
	assert(res.left_aabb.containsAABBox(res.left_centroid_aabb));
	assert(centroid_aabb.containsAABBox(res.left_centroid_aabb));
	assert(aabb.containsAABBox(res.right_aabb));
	assert(res.right_aabb.containsAABBox(res.right_centroid_aabb));
	assert(centroid_aabb.containsAABBox(res.right_centroid_aabb));


	// Allocate left child from the thread's current result chunk
	const uint32 left_child = allocNode(thread_temp_info);
	BinningResultChunk* left_child_chunk = thread_temp_info.result_chunk; // Record result_chunk used for left child now, as thread_temp_info.result_chunk may change below.

	const bool do_right_child_in_new_task = (num_left_tris >= new_task_num_ob_threshold) && (num_right_tris >= new_task_num_ob_threshold);// && !task_manager->areAllThreadsBusy();

	//if(do_right_child_in_new_task)
	//	conPrint("Splitting task: num left=" + toString(split_i - left) + ", num right=" + toString(right - split_i));
	//else
	//	conPrint("not splitting.");

	// Allocate right child
	BinningResultChunk* right_child_chunk;
	uint32 right_child;
	if(!do_right_child_in_new_task)
	{
		right_child = allocNode(thread_temp_info);
		right_child_chunk = thread_temp_info.result_chunk;
	}
	else
	{
		right_child_chunk = NULL; // not used
		right_child = 0; // not used
	}

	// Mark this node as an interior node.
	node_result_chunk->nodes[node_index].interior = true;
	node_result_chunk->nodes[node_index].aabb = aabb;
	node_result_chunk->nodes[node_index].left  = (int)(left_child  + left_child_chunk->chunk_offset);
	node_result_chunk->nodes[node_index].right = do_right_child_in_new_task ? -1 : (int)(right_child + right_child_chunk->chunk_offset); // will be fixed-up later
	node_result_chunk->nodes[node_index].right_child_chunk_index = -1; // not used
	node_result_chunk->nodes[node_index].depth = (uint8)depth;

	thread_temp_info.stats.num_interior_nodes++;

	if(do_right_child_in_new_task) // NOTE: make sure to do this after we create the current node, as the current node right index will be fixed up by this task.
	{
		// Put this subtree into a task
		Reference<BinningBuildSubtreeTask> subtree_task = new BinningBuildSubtreeTask(*this);
		subtree_task->node_aabb = res.right_aabb;
		subtree_task->centroid_aabb = res.right_centroid_aabb;
		subtree_task->depth = depth + 1;
		subtree_task->begin = res.split_i;
		subtree_task->end = end;
		subtree_task->parent_node_index = (int)(node_index);
		subtree_task->parent_chunk = node_result_chunk;

		task_manager->addTask(subtree_task);
	}

	// Build left child
	doBuild(
		thread_temp_info,
		res.left_aabb, // aabb
		res.left_centroid_aabb,
		left_child, // node index
		begin, // begin
		res.split_i, // end
		depth + 1, // depth
		left_child_chunk
	);

	if(!do_right_child_in_new_task) // If we haven't launched a task to build the right subtree already:
	{
		// Build right child
		doBuild(
			thread_temp_info,
			res.right_aabb, // aabb
			res.right_centroid_aabb,
			right_child, // node index
			res.split_i, // begin
			end, // end
			depth + 1, // depth
			right_child_chunk
		);
	}
}


#if BUILD_TESTS


#include "BVHBuilderTests.h"
#include "../dll/include/IndigoMesh.h"
#include "../dll/include/IndigoException.h"
#include "../dll/IndigoStringUtils.h"
#include "../utils/TestUtils.h"
#include "../utils/StandardPrintOutput.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Vector.h"
#include "../maths/PCG32.h"
#include "../utils/TaskManager.h"
#include "../utils/Timer.h"
#include "../utils/FileUtils.h"


static void testOnAllIGMeshes(bool comprehensive_tests, bool test_near_build_failure)
{
	PCG32 rng(1);
	glare::TaskManager task_manager;// (1);
	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;

	Timer timer;
	std::vector<std::string> files = FileUtils::getFilesInDirWithExtensionFullPathsRecursive(TestUtils::getIndigoTestReposDir(), "igmesh");
	std::sort(files.begin(), files.end());

	const size_t num_to_test = comprehensive_tests ? files.size() : 100;
	for(size_t i=0; i<num_to_test; ++i)
	{
		Indigo::Mesh mesh;
		try
		{
			//if(i < 188)
			//	continue;

			Indigo::Mesh::readFromFile(toIndigoString(files[i]), mesh);
			js::Vector<BVHBuilderTri, 16> tris(mesh.triangles.size() + mesh.quads.size() * 2);

			if(tris.size() > 500000)
			{
				conPrint("Skipping mesh with " + toString(tris.size()) + " tris.");
				continue;
			}

			for(size_t t=0; t<mesh.triangles.size(); ++t)
			{
				tris[t].v[0] = Vec4f(mesh.vert_positions[mesh.triangles[t].vertex_indices[0]].x, mesh.vert_positions[mesh.triangles[t].vertex_indices[0]].y, mesh.vert_positions[mesh.triangles[t].vertex_indices[0]].z, 1.);
				tris[t].v[1] = Vec4f(mesh.vert_positions[mesh.triangles[t].vertex_indices[1]].x, mesh.vert_positions[mesh.triangles[t].vertex_indices[1]].y, mesh.vert_positions[mesh.triangles[t].vertex_indices[1]].z, 1.);
				tris[t].v[2] = Vec4f(mesh.vert_positions[mesh.triangles[t].vertex_indices[2]].x, mesh.vert_positions[mesh.triangles[t].vertex_indices[2]].y, mesh.vert_positions[mesh.triangles[t].vertex_indices[2]].z, 1.);
			}
			for(size_t q=0; q<mesh.quads.size(); ++q)
			{
				Vec4f v0(mesh.vert_positions[mesh.quads[q].vertex_indices[0]].x, mesh.vert_positions[mesh.quads[q].vertex_indices[0]].y, mesh.vert_positions[mesh.quads[q].vertex_indices[0]].z, 1.);
				Vec4f v1(mesh.vert_positions[mesh.quads[q].vertex_indices[1]].x, mesh.vert_positions[mesh.quads[q].vertex_indices[1]].y, mesh.vert_positions[mesh.quads[q].vertex_indices[1]].z, 1.);
				Vec4f v2(mesh.vert_positions[mesh.quads[q].vertex_indices[2]].x, mesh.vert_positions[mesh.quads[q].vertex_indices[2]].y, mesh.vert_positions[mesh.quads[q].vertex_indices[2]].z, 1.);
				Vec4f v3(mesh.vert_positions[mesh.quads[q].vertex_indices[3]].x, mesh.vert_positions[mesh.quads[q].vertex_indices[3]].y, mesh.vert_positions[mesh.quads[q].vertex_indices[3]].z, 1.);

				tris[mesh.triangles.size() + q * 2 + 0].v[0] = v0;
				tris[mesh.triangles.size() + q * 2 + 0].v[1] = v1;
				tris[mesh.triangles.size() + q * 2 + 0].v[2] = v2;

				tris[mesh.triangles.size() + q * 2 + 1].v[0] = v0;
				tris[mesh.triangles.size() + q * 2 + 1].v[1] = v2;
				tris[mesh.triangles.size() + q * 2 + 1].v[2] = v3;
			}

			conPrint(toString(i) + "/" + toString(files.size()) + ": Building '" + files[i] + "'... (tris: " + toString(tris.size()) + ")");

			const int max_num_objects_per_leaf = 31;
			const float intersection_cost = 1.f;
			const int max_depth = test_near_build_failure ? myMax(0, ((int)logBase2<double>((double)tris.size() / max_num_objects_per_leaf) + 2)) : 60;
			BinningBVHBuilder builder(/*leaf_num_object_threshold=*/1, max_num_objects_per_leaf, max_depth, intersection_cost, 
				(int)tris.size()
			);

			for(size_t z=0; z<tris.size(); ++z)
			{
				js::AABBox aabb = js::AABBox::emptyAABBox();
				aabb.enlargeToHoldPoint(tris[z].v[0]);
				aabb.enlargeToHoldPoint(tris[z].v[1]);
				aabb.enlargeToHoldPoint(tris[z].v[2]);
				builder.setObjectAABB((int)z, aabb);
			}

			js::Vector<ResultNode, 64> result_nodes;
			builder.build(task_manager,
				should_cancel_callback,
				print_output,
				result_nodes
			);

			BVHBuilderTests::testResultsValid(builder.getResultObjectIndices(), result_nodes, tris.size(), /*duplicate_prims_allowed=*/true);
		}
		catch(Indigo::IndigoException& e)
		{
			// Error reading mesh file (maybe invalid)
			conPrint("Skipping mesh failed to read (" + files[i] + "): " + toStdString(e.what()));
		}
	}
	conPrint("Finished building all meshes.  Elapsed: " + timer.elapsedStringNSigFigs(3));
}


static void testWithNumObsAndMaxDepth(int num_objects, int max_depth, int max_num_objects_per_leaf, bool failure_expected)
{
	glare::TaskManager task_manager;
	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;

	PCG32 rng_(1);
	js::Vector<js::AABBox, 16> aabbs(num_objects);
	for(int z=0; z<num_objects; ++z)
	{
		const Vec4f v0(rng_.unitRandom() * 0.8f, rng_.unitRandom() * 0.8f, rng_.unitRandom() * 0.8f, 1);
		const Vec4f v1 = v0 + Vec4f(rng_.unitRandom(), rng_.unitRandom(), rng_.unitRandom(), 0) * 0.02f;
		const Vec4f v2 = v0 + Vec4f(rng_.unitRandom(), rng_.unitRandom(), rng_.unitRandom(), 0) * 0.02f;
		aabbs[z] = js::AABBox::emptyAABBox();
		aabbs[z].enlargeToHoldPoint(v0);
		aabbs[z].enlargeToHoldPoint(v1);
		aabbs[z].enlargeToHoldPoint(v2);
	}

	try
	{
		Timer timer;

		const float intersection_cost = 1.f;
		BinningBVHBuilder builder(/*leaf_num_object_threshold=*/1, max_num_objects_per_leaf, max_depth, intersection_cost,
			num_objects
		);

		for(int z=0; z<num_objects; ++z)
			builder.setObjectAABB(z, aabbs[z]);

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			should_cancel_callback,
			print_output,
			result_nodes
		);

		if(failure_expected)
			failTest("Expected failure.");

		const double elapsed = timer.elapsed();
		conPrint("BinningBVHBuilder: BVH building for " + toString(num_objects) + " objects took " + toString(elapsed) + " s");

		BVHBuilderTests::testResultsValid(builder.getResultObjectIndices(), result_nodes, aabbs.size(), /*duplicate_prims_allowed=*/false);

		const float SAH_cost = BVHBuilder::getSAHCost(result_nodes, intersection_cost);
		conPrint("SAH_cost: " + toString(SAH_cost));
	}
	catch(glare::Exception& e)
	{
		if(!failure_expected)
			failTest(e.what());
	}
}


void BinningBVHBuilder::test(bool comprehensive_tests)
{
	conPrint("BinningBVHBuilder::test()");

	PCG32 rng(1);
	glare::TaskManager task_manager;// (1);
	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;

	//==================== Test building on every igmesh we can find ====================
	testOnAllIGMeshes(comprehensive_tests, /*test_build_failure_recovery=*/false);
	testOnAllIGMeshes(comprehensive_tests, /*test_build_failure_recovery=*/true);

	//==================== Test Builds very close to max depth and num obs constraints ====================
	conPrint("Testing near build failure...");
	testWithNumObsAndMaxDepth(/*num obs=*/1, /*max depth=*/0, /*max_num_objects_per_leaf=*/1, /*failure_expected=*/false);
	testWithNumObsAndMaxDepth(/*num obs=*/2, /*max depth=*/1, /*max_num_objects_per_leaf=*/1, /*failure_expected=*/false);
	testWithNumObsAndMaxDepth(/*num obs=*/4, /*max depth=*/2, /*max_num_objects_per_leaf=*/1, /*failure_expected=*/false);
	testWithNumObsAndMaxDepth(/*num obs=*/8, /*max depth=*/3, /*max_num_objects_per_leaf=*/1, /*failure_expected=*/false);
	testWithNumObsAndMaxDepth(/*num obs=*/24, /*max depth=*/3, /*max_num_objects_per_leaf=*/3, /*failure_expected=*/false);

	testWithNumObsAndMaxDepth(/*num obs=*/48, /*max depth=*/4, /*max_num_objects_per_leaf=*/3, /*failure_expected=*/false);
	testWithNumObsAndMaxDepth(/*num obs=*/47, /*max depth=*/4, /*max_num_objects_per_leaf=*/3, /*failure_expected=*/false);
	testWithNumObsAndMaxDepth(/*num obs=*/20, /*max depth=*/4, /*max_num_objects_per_leaf=*/3, /*failure_expected=*/false);

	conPrint("Testing expected build failures...");
	testWithNumObsAndMaxDepth(/*num obs=*/9,    /*max depth=*/3,  /*max_num_objects_per_leaf=*/1, /*failure_expected=*/true);
	testWithNumObsAndMaxDepth(/*num obs=*/1025, /*max depth=*/10, /*max_num_objects_per_leaf=*/1, /*failure_expected=*/true);

	//==================== Perf test ====================
	const bool DO_PERF_TESTS = false;
	if(DO_PERF_TESTS)
	{
		conPrint("Doing perf test...");

		const int num_objects = 1000000;

		PCG32 rng_(1);
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		for(int z=0; z<num_objects; ++z)
		{
			const Vec4f v0(rng_.unitRandom() * 0.8f, rng_.unitRandom() * 0.8f, 0/*rng_.unitRandom()*/ * 0.8f, 1);
			const Vec4f v1 = v0 + Vec4f(rng_.unitRandom(), rng_.unitRandom(), /*rng_.unitRandom()*/0, 0) * 0.02f;
			const Vec4f v2 = v0 + Vec4f(rng_.unitRandom(), rng_.unitRandom(), /*rng_.unitRandom()*/0, 0) * 0.02f;
			aabbs[z] = js::AABBox::emptyAABBox();
			aabbs[z].enlargeToHoldPoint(v0);
			aabbs[z].enlargeToHoldPoint(v1);
			aabbs[z].enlargeToHoldPoint(v2);
		}

		double sum_time = 0;
		double min_time = 1.0e100;
		const int NUM_ITERS = 10;
		for(int q=0; q<NUM_ITERS; ++q)
		{
			Timer timer;

			const int max_num_objects_per_leaf = 16;
			const float intersection_cost = 1.f;

			BinningBVHBuilder builder(/*leaf_num_object_threshold=*/1, max_num_objects_per_leaf, /*max_depth=*/60, intersection_cost,
				num_objects
			);

			for(int z=0; z<num_objects; ++z)
				builder.setObjectAABB(z, aabbs[z]);

			js::Vector<ResultNode, 64> result_nodes;
			builder.build(task_manager,
				should_cancel_callback,
				print_output,
				result_nodes
			);

			const double elapsed = timer.elapsed();
			sum_time += elapsed;
			min_time = myMin(min_time, elapsed);
			conPrint("BinningBVHBuilder: BVH building for " + toString(num_objects) + " objects took " + toString(elapsed) + " s");

			BVHBuilderTests::testResultsValid(builder.getResultObjectIndices(), result_nodes, aabbs.size(), /*duplicate_prims_allowed=*/false);

			const float SAH_cost = BVHBuilder::getSAHCost(result_nodes, intersection_cost);
			conPrint("SAH_cost: " + toString(SAH_cost));
			//--------------------------------------
		}

		const double av_time = sum_time / NUM_ITERS;
		conPrint("av_time:  " + toString(av_time) + " s");
		conPrint("min_time: " + toString(min_time) + " s");
	}

	conPrint("BinningBVHBuilder::test() done");
}


#endif // BUILD_TESTS
