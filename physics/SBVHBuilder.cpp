/*=====================================================================
SBVHBuilder.cpp
---------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "SBVHBuilder.h"


#include "jscol_aabbox.h"
#include "../graphics/bitmap.h"
#include "../graphics/Drawing.h"
#include "../graphics/PNGDecoder.h"
#include "../utils/ShouldCancelCallback.h"
#include "../utils/Exception.h"
#include "../utils/Sort.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/TaskManager.h"
#include "../utils/Lock.h"
#include "../utils/Timer.h"
#include "../utils/ProfilerStore.h"
#include <algorithm>


/*
TODO
----
* Do actual in-place partitioning without the extra temp_obs array. (partition in-place then move)

* Of if still using temp_obs: ping-pong between buffers instead of copying back.

* Try precomputing reciprocal tri edge vectors - should make clipping faster.  (see edge recip distances precomputed.patch)

* Do binning in tasks in SBVHBuilder as well as in BinningBVHBuilder? (doesn't seem to measurably speed up)

* Can definitely get better builds lowering alpha. (threshold for spatial splits search)

* prefetch tri in partition as well as search? (doesn't seem to measurably speed up)

* const int num_spatial_buckets = 16;:   tweak this?

*/


/*

Partitioning
------------
We partition in-place, splitting spare capacity left and right.

-------------------------------------------------------------------------
|                                  |                                    |
begin                              end                                  capacity

partitioned:

-------------------------------------------------------------------------
|                   |                 |              |                  |
left begin    left end   left capacity, right begin  right end       right capacity


*/


static const js::AABBox empty_aabb = js::AABBox::emptyAABBox();


// Since we store the object index in the AABB.min_.w, we want to restore this value to 1 before we return it to the client code.
static inline void setAABBWToOneInPlace(js::AABBox& aabb)
{
	aabb.min_ = setWToOne(aabb.min_);
	aabb.max_ = setWToOne(aabb.max_);
}


#ifndef NDEBUG
static inline const js::AABBox setWCoordsToOne(const js::AABBox& box) { return js::AABBox(setWToOne(box.min_), setWToOne(box.max_)); }
#endif


//#define ALLOW_DEBUG_DRAWING 1
#if ALLOW_DEBUG_DRAWING
static const bool DEBUG_DRAW = false;
static const Vec2f lower(0, 0);
static const float draw_scale = 1;
static const int map_res = 1000;
static Bitmap main_map(map_res, map_res, 3, NULL);
#endif


#if ALLOW_DEBUG_DRAWING
static void drawLine(Bitmap& map, const Colour3f& col, const Vec2f& a, const Vec2f& b)
{
	Drawing::drawLine(map, col,
		Vec2f((a.x - lower.x) * draw_scale, 1 - (a.y - lower.y)*draw_scale) * (float)map.getHeight(),
		Vec2f((b.x - lower.x) * draw_scale, 1 - (b.y - lower.y)*draw_scale) * (float)map.getHeight());
}

static void drawTriangle(Bitmap& map, const Vec4f& v0, const Vec4f& v1, const Vec4f& v2, const Vec4f& centroid)
{
	drawLine(map, Colour3f(0.8f), Vec2f(v0[0], v0[1]), Vec2f(v1[0], v1[1]));
	drawLine(map, Colour3f(0.8f), Vec2f(v1[0], v1[1]), Vec2f(v2[0], v2[1]));
	drawLine(map, Colour3f(0.8f), Vec2f(v2[0], v2[1]), Vec2f(v0[0], v0[1]));

	// Draw centroid
	drawLine(map, Colour3f(0.8f), Vec2f(centroid[0] - 0.002f, centroid[1]), Vec2f(centroid[0] + 0.002f, centroid[1]));
	drawLine(map, Colour3f(0.8f), Vec2f(centroid[0], centroid[1] - 0.002f), Vec2f(centroid[0], centroid[1] + 0.002f));
}


static void drawTriangle(Bitmap& map, const SBVHTri& tri, const Vec4f& centroid)
{
	drawTriangle(map, tri.v[0], tri.v[1], tri.v[2], centroid);
}

static void drawAABB(Bitmap& map, const Colour3f& col, const js::AABBox& aabb)
{
	drawLine(map, col, Vec2f(aabb.min_[0], aabb.min_[1]), Vec2f(aabb.max_[0], aabb.min_[1]));
	drawLine(map, col, Vec2f(aabb.max_[0], aabb.min_[1]), Vec2f(aabb.max_[0], aabb.max_[1]));
	drawLine(map, col, Vec2f(aabb.max_[0], aabb.max_[1]), Vec2f(aabb.min_[0], aabb.max_[1]));
	drawLine(map, col, Vec2f(aabb.min_[0], aabb.max_[1]), Vec2f(aabb.min_[0], aabb.min_[1]));
}


static void drawPartitionLine(Bitmap& map, const js::AABBox& aabb, int best_axis, float best_div_val, bool best_split_was_spatial)
{
	const Colour3f col = best_split_was_spatial ? Colour3f(0.4f, 0.7f, 0.4f) : Colour3f(0.4f, 0.4f, 0.5f);
	if(best_axis == 0)
		drawLine(map, col, Vec2f(best_div_val, aabb.min_[1]), Vec2f(best_div_val, aabb.max_[1]));
	else if(best_axis == 1)
		drawLine(map, col, Vec2f(aabb.min_[0], best_div_val), Vec2f(aabb.max_[0], best_div_val));
}
#endif


size_t SBVHPerThreadTempInfo::dataSizeBytes() const
{
	size_t s = 0;
	return s;
}


SBVHBuilder::SBVHBuilder(int leaf_num_object_threshold_, int max_num_objects_per_leaf_, int max_depth_, float intersection_cost_,
	const SBVHTri* triangles_,
	const int num_objects_
)
:	leaf_num_object_threshold(leaf_num_object_threshold_),
	max_num_objects_per_leaf(max_num_objects_per_leaf_),
	max_depth(max_depth_),
	intersection_cost(intersection_cost_),
	should_cancel_callback(NULL)
{
	assert(max_depth >= 0);
	assert(intersection_cost > 0.f);

	triangles = triangles_;
	m_num_objects = num_objects_;

	// See /wiki/index.php?title=BVH_Building for results on varying this settings.
	new_task_num_ob_threshold = 1 << 9;

	static_assert(sizeof(ResultNode) == 48, "sizeof(ResultNode) == 48");
}


SBVHBuilder::~SBVHBuilder()
{
	// These might have non-zero size if the build was cancelled.
	for(size_t i=0; i<result_chunks.size(); ++i)
		delete result_chunks[i];
	
	for(size_t i=0; i<leaf_result_chunks.size(); ++i)
		delete leaf_result_chunks[i];
}


SBVHBuildStats::SBVHBuildStats()
{
	num_maxdepth_leaves = 0;
	num_under_thresh_leaves = 0;
	num_cheaper_nosplit_leaves = 0;
	num_could_not_split_leaves = 0;
	num_leaves = 0;
	num_tris_in_leaves = 0;
	max_num_tris_per_leaf = 0;
	leaf_depth_sum = 0;
	max_leaf_depth = 0;
	num_interior_nodes = 0;
	num_arbitrary_split_leaves = 0;
	num_object_splits = 0;
	num_spatial_splits = 0;
	num_hit_capacity_splits = 0;
}


void SBVHBuildStats::accumStats(const SBVHBuildStats& other)
{
	num_maxdepth_leaves				+= other.num_maxdepth_leaves;
	num_under_thresh_leaves			+= other.num_under_thresh_leaves;
	num_cheaper_nosplit_leaves		+= other.num_cheaper_nosplit_leaves;
	num_could_not_split_leaves		+= other.num_could_not_split_leaves;
	num_leaves						+= other.num_leaves;
	num_tris_in_leaves				+= other.num_tris_in_leaves;
	max_num_tris_per_leaf			= myMax(max_num_tris_per_leaf, other.max_num_tris_per_leaf);
	leaf_depth_sum					+= other.leaf_depth_sum;
	max_leaf_depth					= myMax(max_leaf_depth, other.max_leaf_depth);
	num_interior_nodes				+= other.num_interior_nodes;
	num_arbitrary_split_leaves		+= other.num_arbitrary_split_leaves;
	num_object_splits				+= other.num_object_splits;
	num_spatial_splits				+= other.num_spatial_splits;
	num_hit_capacity_splits			+= other.num_hit_capacity_splits;
}


// Returns node index in thread_temp_info.result_chunk
// May update thread_temp_info.result_chunk to point to a new chunk (if we ran out of space in the last chunk)
uint32 SBVHBuilder::allocNode(SBVHPerThreadTempInfo& thread_temp_info)
{
	if(thread_temp_info.result_chunk->size >= SBVHResultChunk::MAX_RESULT_CHUNK_SIZE) // If the current chunk is full:
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
class SBVHBuildSubtreeTask : public glare::Task
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	SBVHBuildSubtreeTask(SBVHBuilder& builder_) : builder(builder_) {}
	
	virtual void run(size_t thread_index)
	{
		// Flush denormals to zero.  This is important otherwise w values storing low integer values get interpreted as denormals, which drastically reduces performance.
		_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
		_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);

		ScopeProfiler _scope("SBVHBuildSubtreeTask", (int)thread_index);

		builder.per_thread_temp_info[thread_index].result_chunk = result_chunk;
		builder.per_thread_temp_info[thread_index].leaf_result_chunk = leaf_result_chunk;

		result_chunk->size = 1;

		try
		{
			builder.doBuild(
				builder.per_thread_temp_info[thread_index],
				node_aabb,
				centroid_aabb,
				0, // node index
				begin,
				end,
				capacity,
				depth, 
				result_chunk
			);
		}
		catch(glare::CancelledException&)
		{}
	}

	SBVHBuilder& builder;
	SBVHResultChunk* result_chunk;
	SBVHLeafResultChunk* leaf_result_chunk;
	js::AABBox node_aabb;
	js::AABBox centroid_aabb;
	int depth;
	int begin, end, capacity;
};


SBVHResultChunk* SBVHBuilder::allocNewResultChunk()
{
	SBVHResultChunk* chunk = new SBVHResultChunk();
	chunk->size = 0;

	Lock lock(result_chunks_mutex);
	chunk->chunk_offset = result_chunks.size() * SBVHResultChunk::MAX_RESULT_CHUNK_SIZE;
	result_chunks.push_back(chunk);
	return chunk;
}


SBVHLeafResultChunk* SBVHBuilder::allocNewLeafResultChunk()
{
	SBVHLeafResultChunk* chunk = new SBVHLeafResultChunk();
	chunk->size = 0;

	Lock lock(leaf_result_chunks_mutex);
	chunk->chunk_offset = leaf_result_chunks.size() * SBVHLeafResultChunk::MAX_RESULT_CHUNK_SIZE;
	leaf_result_chunks.push_back(chunk);
	return chunk;
}


struct SBVHStackEntry
{
	uint32 node_index;
	int parent_node_index; // -1 if no parent.
	bool is_left;
};


// Top-level build method
void SBVHBuilder::build(
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


	//------------ Reset builder state --------------
	per_thread_temp_info.clear();
	for(size_t i=0; i<result_chunks.size(); ++i)
		delete result_chunks[i];
	result_chunks.clear();
	for(size_t i=0; i<leaf_result_chunks.size(); ++i)
		delete leaf_result_chunks[i];
	leaf_result_chunks.clear();
	result_chunks.clear();
	stats = SBVHBuildStats();
	result_nodes_out.clear();
	//------------ End reset builder state --------------


	js::AABBox root_centroid_aabb;
	{
	ScopeProfiler _scope2("initial init");

	//Timer timer;
	split_search_time = 0;
	partition_time = 0;

	this->task_manager = &task_manager_;
	this->should_cancel_callback = &should_cancel_callback_;
	const int num_objects = m_num_objects;

#if ALLOW_DEBUG_DRAWING
	if(DEBUG_DRAW)
	{
		main_map.zero();

		// Draw faint background grid lines
		for(float x=0; x<2; x += 0.1f)
			drawLine(main_map, Colour3f(0.15f), Vec2f(x, 0), Vec2f(x, 1));
		for(float y=0; y<2; y += 0.1f)
			drawLine(main_map, Colour3f(0.15f), Vec2f(0, y), Vec2f(1, y));

		// Draw tris
		for(int i=0; i<num_objects; ++i)
			drawTriangle(main_map, triangles[i], aabbs[i].centroid());
	}
#endif

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

	root_aabb = empty_aabb;
	root_centroid_aabb = empty_aabb;

	// Allocate and build top_level_objects
	/*
	The larger the capacity factor here, the higher quality the build, but the more mem used, and the slower building is.
	bedroom with num_objects * 2:
	num hit capacity splits     58764
	Build took 1.7616 s
	4.911 M samples/s (GTX 1070)
	4.906 M samples/s (GTX 1070)

	bedroom with num_objects * 4:
	num hit capacity splits     516
	Build took 1.8272 s
	4.932 M samples/s (GTX 1070)
	4.935 M samples/s (GTX 1070)
	*/
	this->top_level_objects.resize(num_objects * 4);
	this->temp_obs         .resize(num_objects * 4);

	for(int i = 0; i < num_objects; ++i)
	{
		const SBVHTri& tri = triangles[i];

		js::AABBox tri_aabb(tri.v[0], tri.v[0]);
		tri_aabb.enlargeToHoldPoint(tri.v[1]);
		tri_aabb.enlargeToHoldPoint(tri.v[2]);

		top_level_objects[i].aabb.min_ = setW(tri_aabb.min_, (int)i); // Store ob index in min_.w
		top_level_objects[i].aabb.max_ = tri_aabb.max_;
		assert(top_level_objects[i].getIndex() == i);

		root_aabb.enlargeToHoldAABBox(tri_aabb);
		root_centroid_aabb.enlargeToHoldPoint(tri_aabb.centroid());
	}

	if(should_cancel_callback->shouldCancel())
		throw glare::CancelledException();

	this->recip_root_node_aabb_area = 1 / root_aabb.getSurfaceArea();
	
	
	// Reserve working space for each thread.
	per_thread_temp_info.resize(task_manager->getNumThreads());
	for(size_t i = 0; i < per_thread_temp_info.size(); ++i)
		per_thread_temp_info[i].build_failed = false;

	} // End scope for initial init


	SBVHResultChunk* root_chunk = allocNewResultChunk();
	SBVHLeafResultChunk* root_leaf_chunk = allocNewLeafResultChunk();

	Reference<SBVHBuildSubtreeTask> task = new SBVHBuildSubtreeTask(*this);
	task->depth = 0;
	task->begin = 0;
	task->end = m_num_objects;
	task->capacity = (int)top_level_objects.size();
	task->node_aabb = root_aabb;
	task->centroid_aabb = root_centroid_aabb;
	task->result_chunk = root_chunk;
	task->leaf_result_chunk = root_leaf_chunk;
	task_manager->addTask(task);

	task_manager->waitForTasksToComplete();

	if(should_cancel_callback->shouldCancel())
		throw glare::CancelledException();

	// See if the build failed:
	for(size_t i = 0; i < per_thread_temp_info.size(); ++i)
		if(per_thread_temp_info[i].build_failed)
			throw glare::Exception("Build failed.");


	/*conPrint("initial_result_buf_reserve_cap: " + toString(initial_result_buf_reserve_cap));
	for(int i=0; i<(int)per_thread_temp_info.size(); ++i)
	{
		conPrint("thread " + toString(i));
		conPrint("	result_buf.size(): " + toString(per_thread_temp_info[i].result_buf.size()));
		conPrint("	result_buf.capacity(): " + toString(per_thread_temp_info[i].result_buf.capacity()));
	}*/


	// Now we need to combine all the result chunks into a single array.


	// First combine leaf geom
	js::Vector<uint32, 16> final_leaf_geom_indices(leaf_result_chunks.size() * SBVHLeafResultChunk::MAX_RESULT_CHUNK_SIZE);
	uint32 write_i = 0;
	for(size_t c=0; c<leaf_result_chunks.size(); ++c)
		for(size_t i=0; i<leaf_result_chunks[c]->size; ++i)
			final_leaf_geom_indices[c * SBVHLeafResultChunk::MAX_RESULT_CHUNK_SIZE + i] = write_i++;

	result_indices.resizeNoCopy(write_i);

	for(size_t c=0; c<leaf_result_chunks.size(); ++c)
	{
		const SBVHLeafResultChunk& chunk = *leaf_result_chunks[c];
		size_t src_i = c * SBVHLeafResultChunk::MAX_RESULT_CHUNK_SIZE;

		for(size_t i=0; i<chunk.size; ++i)
		{
			const int leaf_geom = chunk.leaf_obs[i];

			const uint32 final_pos = final_leaf_geom_indices[src_i];
			result_indices[final_pos] = leaf_geom; // Copy leaf geom index to final array
			src_i++;
		}

		delete leaf_result_chunks[c];
	}
	leaf_result_chunks.clear();


	// Count number of result nodes
	size_t num_nodes = 0;
	for(size_t c=0; c<result_chunks.size(); ++c)
		num_nodes += result_chunks[c]->size;
	
	// Convert nodes in our SBVHResultChunks into a nice linear array of nodes, in either a depth-first or breadth-first traversal order.
	result_nodes_out.resizeNoCopy(num_nodes);

	CircularBuffer<SBVHStackEntry> stack; // Stack of references to interior nodes to process.

	// Push root node onto stack
	{
		SBVHStackEntry entry;
		entry.node_index = 0;
		entry.parent_node_index = -1; // TODO: could just decide on breadth first ordering, then don't need to do patch-ups.
		stack.push_back(entry);
	}

	int res_node_index = 0;
	while(!stack.empty())
	{
		/*
		Breadth-first traversal seems to be slightly faster:
		 
		// Popping off front of queue - this gives a breadth-first traversal
		14.747
		14.711
		14.716
		14.683
		14.690

		// Popping off back of stack - this gives a depth-first traversal
		14.658
		14.652
		*/

		// Popping off back of stack - this gives a depth-first traversal
		//const SBVHStackEntry entry = stack.back();
		//stack.pop_back();

		// Popping off front of queue - this gives a breadth-first traversal
		const SBVHStackEntry entry = stack.front();
		stack.pop_front();

		const uint32 chunk_index = entry.node_index / SBVHResultChunk::MAX_RESULT_CHUNK_SIZE;
		const SBVHResultChunk& chunk = *result_chunks[chunk_index];
		const ResultNode* src_node = &chunk.nodes[entry.node_index - chunk_index * SBVHResultChunk::MAX_RESULT_CHUNK_SIZE];
		
		const int dst_node_index = res_node_index++;

		// Fix up index in parent node to this node.
		if(entry.parent_node_index >= 0)
		{
			if(entry.is_left)
				result_nodes_out[entry.parent_node_index].left = dst_node_index;
			else
				result_nodes_out[entry.parent_node_index].right = dst_node_index;
		}

		if(src_node->interior)
		{
			// Push left
			{
				SBVHStackEntry new_entry;
				new_entry.is_left = true;
				new_entry.node_index = src_node->left;
				new_entry.parent_node_index = dst_node_index;
				stack.push_back(new_entry);
			}

			// Push right
			{
				SBVHStackEntry new_entry;
				new_entry.is_left = false;
				new_entry.node_index = src_node->right;
				new_entry.parent_node_index = dst_node_index;
				stack.push_back(new_entry);
			}
		}
		else
		{
			// This is a leaf node
			const int new_left = final_leaf_geom_indices[src_node->left];
			result_nodes_out[dst_node_index].left = new_left;
			result_nodes_out[dst_node_index].right = new_left + (src_node->right - src_node->left); // new prim end = new prim start + (old prim end - old prim start)
		}

		result_nodes_out[dst_node_index].aabb = src_node->aabb;
		result_nodes_out[dst_node_index].interior = src_node->interior;
	}

	assert(res_node_index == (int)result_nodes_out.size());

	for(size_t c=0; c<result_chunks.size(); ++c)
		delete result_chunks[c];
	result_chunks.clear();


	// Combine stats from each thread
	for(size_t i=0; i<per_thread_temp_info.size(); ++i)
		this->stats.accumStats(per_thread_temp_info[i].stats);

	// Dump some stats
	if(false)
	{
		const double build_time = build_timer.elapsed();

		conPrint("----------- SBVHBuilder Build Stats ----------");
		conPrint("Mem usage:");
		conPrint("top_level_objects:          " + getNiceByteSize(sizeof(SBVHOb) * top_level_objects.size()));
		conPrint("temp_obs:                   " + getNiceByteSize(sizeof(SBVHOb) * temp_obs.size()));

		size_t total_per_thread_size = 0;
		for(size_t i=0; i<per_thread_temp_info.size(); ++i)
			total_per_thread_size += per_thread_temp_info[i].dataSizeBytes();
		conPrint("per_thread_temp_info:       " + getNiceByteSize(total_per_thread_size));

		conPrint("result_chunks:              " + getNiceByteSize(result_chunks.dataSizeBytes()));
		conPrint("result_nodes_out:           " + toString(result_nodes_out.size()) + " nodes * " + toString(sizeof(ResultNode)) + "B = " + getNiceByteSize(result_nodes_out.dataSizeBytes()));
		
		const size_t total_size = (sizeof(SBVHOb) * top_level_objects.size()) + total_per_thread_size + result_chunks.dataSizeBytes() + result_nodes_out.dataSizeBytes();
		
		conPrint("total:                      " + getNiceByteSize(total_size));
		conPrint("");

		conPrint("split_search_time:          " + toString(split_search_time) + " s");
		conPrint("partition_time:             " + toString(partition_time) + " s");

		conPrint("Num triangles:              " + toString(m_num_objects));
		conPrint("num interior nodes:         " + toString(stats.num_interior_nodes));
		conPrint("num leaves:                 " + toString(stats.num_leaves));
		conPrint("num maxdepth leaves:        " + toString(stats.num_maxdepth_leaves));
		conPrint("num under_thresh leaves:    " + toString(stats.num_under_thresh_leaves));
		conPrint("num cheaper nosplit leaves: " + toString(stats.num_cheaper_nosplit_leaves));
		conPrint("num could not split leaves: " + toString(stats.num_could_not_split_leaves));
		conPrint("num arbitrary split leaves: " + toString(stats.num_arbitrary_split_leaves));
		conPrint("av num tris per leaf:       " + toString((float)stats.num_tris_in_leaves / stats.num_leaves));
		conPrint("max num tris per leaf:      " + toString(stats.max_num_tris_per_leaf));
		conPrint("av leaf depth:              " + toString((float)stats.leaf_depth_sum / stats.num_leaves));
		conPrint("max leaf depth:             " + toString(stats.max_leaf_depth));
		conPrint("num leaf primitives:        " + toString(result_indices.size()));
		conPrint("num object splits:          " + toString(stats.num_object_splits));
		conPrint("num spatial splits:         " + toString(stats.num_spatial_splits));
		conPrint("num hit capacity splits     " + toString(stats.num_hit_capacity_splits));
		conPrint("Build took " + doubleToStringNDecimalPlaces(build_time, 4) + " s");
		conPrint("---------------------");
	}

#if ALLOW_DEBUG_DRAWING
	if(DEBUG_DRAW)
		PNGDecoder::write(main_map, "sbvh.png");
#endif
}


// Parition objects in cur_objects for the given axis, based on best_div_val of best_axis.
// Places the paritioned objects in left_obs_out and right_obs_out.
struct PartitionRes
{
	// AABB and AABB of centroids of objects being partitioned left and right.
	// We use the centroid AABB when searching for object partitioning splits.
	js::AABBox left_aabb, left_centroid_aabb;
	js::AABBox right_aabb, right_centroid_aabb;

	int num_left; // num partitioned left
	int num_right; // num partitioned right
	int left_capacity; // capacity assigned left.
};


// Partition half the list left and half right.
static void arbitraryPartition(const std::vector<SBVHOb>& objects_, int begin, int end, int capacity, PartitionRes& res_out)
{
	assert(end - begin >= 2);
	const SBVHOb* const objects = objects_.data();

	js::AABBox left_aabb = empty_aabb;
	js::AABBox left_centroid_aabb = empty_aabb;
	js::AABBox right_aabb = empty_aabb;
	js::AABBox right_centroid_aabb = empty_aabb;

	const int half_num = (end - begin)/2;
	const int split_i = begin + half_num;

	// Compute AABBs
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

	res_out.num_left = half_num;
	res_out.num_right = (end - begin) - half_num;
	res_out.left_capacity = half_num;
}


static void partition(std::vector<SBVHOb>& objects_, std::vector<SBVHOb>& temp_obs, int begin, int end, int capacity, const BVHBuilderTri* triangles, const js::AABBox& parent_aabb, 
	const js::AABBox& centroid_aabb,
	float best_div_val, int best_axis, bool best_split_was_spatial, int best_bucket,
	int best_num_left, int best_num_right, 
	PartitionRes& res_out
	)
{
	SBVHOb* const objects = objects_.data();

	const int num_left  = best_num_left;
	const int num_right = best_num_right;

	// Split the capacity proportially to num_left and num_right
	const int max_left_capacity = capacity - num_right;
	const int left_capacity = myClamp((int)(0.5f + capacity * (float)num_left / (num_left + num_right)), num_left, max_left_capacity); // Add 0.5 to round-to-nearest int

	int left_write = begin;
	const int right_write_begin = begin + left_capacity;
	int right_write = right_write_begin;

	js::AABBox left_aabb = empty_aabb;
	js::AABBox left_centroid_aabb = empty_aabb;
	js::AABBox right_aabb = empty_aabb;
	js::AABBox right_centroid_aabb = empty_aabb;

	if(best_split_was_spatial)
	{
		const int num_spatial_buckets = 16;
		const Vec4f spatial_scale = div(Vec4f((float)num_spatial_buckets), (parent_aabb.max_ - parent_aabb.min_));

		for(int cur = begin; cur < end; ++cur)
		{
			const int ob_i = objects[cur].getIndex();
			const BVHBuilderTri& tri = triangles[ob_i];
			const js::AABBox aabb = objects[cur].aabb;
			assert(parent_aabb.containsAABBox(setWCoordsToOne(aabb)));
			const Vec4i entry_bucket_i = clamp(truncateToVec4i((aabb.min_ - parent_aabb.min_) * spatial_scale), Vec4i(0), Vec4i(num_spatial_buckets-1));
			const Vec4i exit_bucket_i  = clamp(truncateToVec4i((aabb.max_ - parent_aabb.min_) * spatial_scale), Vec4i(0), Vec4i(num_spatial_buckets-1));

			if(entry_bucket_i[best_axis] <= best_bucket && exit_bucket_i[best_axis] > best_bucket) // If triangle straddles split plane:
			{
				const uint32 unsplit_tri = objects[cur].getUnsplit();
				if(unsplit_tri == 1)
				{
					temp_obs[left_write++] = objects[cur];
					left_aabb.enlargeToHoldAABBox(aabb);
					left_centroid_aabb.enlargeToHoldPoint(aabb.centroid());
				}
				else if(unsplit_tri == 2)
				{
					temp_obs[right_write++] = objects[cur];
					right_aabb.enlargeToHoldAABBox(aabb);
					right_centroid_aabb.enlargeToHoldPoint(aabb.centroid());
				}
				else
				{
					// Clip triangle with splitting plane
					js::AABBox tri_left_aabb  = empty_aabb;
					js::AABBox tri_right_aabb = empty_aabb;
					Vec4f v = tri.v[2]; // current vert, start with v2 so first edge processed is v2 -> v0
					for(int i=0; i<3; ++i) // For each triangle vertex:
					{
						const Vec4f prev_v = v;
						v = tri.v[i];
						if((prev_v[best_axis] < best_div_val && v[best_axis] > best_div_val) || (prev_v[best_axis] > best_div_val && v[best_axis] < best_div_val)) // If edge from prev_v to v straddles clip plane:
						{
							const float t = (best_div_val - prev_v[best_axis]) / (v[best_axis] - prev_v[best_axis]); // Solve for fraction along edge of position on split plane.
							const Vec4f p = prev_v * (1 - t) + v * t;
							tri_left_aabb .enlargeToHoldPoint(p);
							tri_right_aabb.enlargeToHoldPoint(p);
						}

						if(v[best_axis] <= best_div_val) tri_left_aabb .enlargeToHoldPoint(v);
						if(v[best_axis] >= best_div_val) tri_right_aabb.enlargeToHoldPoint(v);
					}

					assert(tri_left_aabb.invariant());
					assert(tri_right_aabb.invariant());

					// Make sure AABBs of clipped triangle don't extend past the bounds of the triangle clipped to the current AABB.
					tri_left_aabb  = intersection(tri_left_aabb,  aabb);
					tri_right_aabb = intersection(tri_right_aabb, aabb);

					// Force AABB to be valid.
					// Fixes partitioning failing due to max of AABB being < min of AABB, resuling in entry bin being > exit bin.
					tri_left_aabb.min_  = min(tri_left_aabb.min_,  tri_left_aabb.max_);
					tri_right_aabb.min_ = min(tri_right_aabb.min_, tri_right_aabb.max_);

					// Check tri_left_aabb, tri_right_aabb
				//	assert(parent_aabb.containsAABBox(tri_left_aabb));
				//	assert(parent_aabb.containsAABBox(tri_right_aabb));
					assert(tri_left_aabb .max_[best_axis] <= best_div_val + 1.0e-5f);
					assert(tri_right_aabb.min_[best_axis] >= best_div_val - 1.0e-5f);

					assert(tri_left_aabb.invariant());
					assert(tri_right_aabb.invariant());

					SBVHOb left_ob;
					left_ob.aabb.min_ = setW(tri_left_aabb.min_, ob_i); // Store ob index in min_.w
					left_ob.aabb.max_ = tri_left_aabb.max_;
					assert(left_ob.getIndex() == ob_i);
					temp_obs[left_write++] = left_ob;

					SBVHOb right_ob;
					right_ob.aabb.min_ = setW(tri_right_aabb.min_, ob_i); // Store ob index in min_.w
					right_ob.aabb.max_ = tri_right_aabb.max_;
					assert(right_ob.getIndex() == ob_i);
					temp_obs[right_write++] = right_ob;

					left_centroid_aabb .enlargeToHoldPoint(tri_left_aabb.centroid()); // Get centroid of the part of the triangle clipped to left child volume, add to left centroid aabb.
					right_centroid_aabb.enlargeToHoldPoint(tri_right_aabb.centroid());

					left_aabb .enlargeToHoldAABBox(tri_left_aabb);
					right_aabb.enlargeToHoldAABBox(tri_right_aabb);
				}
			}
			else // Else if triangle does not straddle clip plane:  then triangle bounds are completely in left or right side. Send triangle left or right based on triangle centroid.
			{
				const Vec4f centroid = aabb.centroid();
				if(exit_bucket_i[best_axis] <= best_bucket) // If object should go on Left side:
				{
					temp_obs[left_write++] = objects[cur];
					left_aabb.enlargeToHoldAABBox(aabb);
					left_centroid_aabb.enlargeToHoldPoint(centroid);
				}
				else // else if cur object should go on Right side:
				{
					temp_obs[right_write++] = objects[cur];
					right_aabb.enlargeToHoldAABBox(aabb);
					right_centroid_aabb.enlargeToHoldPoint(centroid);
				}
			}
		}
	}
	else // Else if best split was just an object split:
	{
		// Code to do the partition exactly how the search was done:
		const int max_B = 32;
		const int num_buckets = myMin(max_B, (int)(4 + 0.05f * (end - begin))); // buckets per axis
		const Vec4f scale = div(Vec4f((float)num_buckets), (centroid_aabb.max_ - centroid_aabb.min_));

		for(int cur = begin; cur < end; ++cur)
		{
			const js::AABBox aabb = objects[cur].aabb;
			const Vec4f centroid = aabb.centroid();

			const Vec4i bucket_i = clamp(truncateToVec4i((centroid - centroid_aabb.min_) * scale), Vec4i(0), Vec4i(num_buckets-1));
			if(bucket_i[best_axis] <= best_bucket) // If object should go on left side:
			{
				left_aabb.enlargeToHoldAABBox(aabb);
				left_centroid_aabb.enlargeToHoldPoint(centroid);

				assert(left_write < begin + num_left);
				assert(left_write < right_write_begin);
				temp_obs[left_write++] = objects[cur];
			}
			else // else if cur object should go on right side:
			{
				right_aabb.enlargeToHoldAABBox(aabb);
				right_centroid_aabb.enlargeToHoldPoint(centroid);

				assert(right_write < right_write_begin + num_right);
				assert(right_write < begin + capacity);
				temp_obs[right_write++] = objects[cur];
			}
		}
	}

	assert(left_write - begin              == num_left ); // Check the number of objects we partitioned left was equal to the num left computed by the search.
	assert(right_write - right_write_begin == num_right); // Check the number of objects we partitioned right was equal to the num left computed by the search.

	// Copy from temp_obs back to obs
	for(int i = begin; i != left_write; ++i)
		objects[i] = temp_obs[i];

	for(int i = begin + left_capacity; i != right_write; ++i)
		objects[i] = temp_obs[i];

	res_out.left_aabb = left_aabb;
	res_out.left_centroid_aabb = left_centroid_aabb;
	res_out.right_aabb = right_aabb;
	res_out.right_centroid_aabb = right_centroid_aabb;
	res_out.num_left = num_left;
	res_out.num_right = num_right;
	res_out.left_capacity = left_capacity;
}


// Compute AABBs of objects partitioned left and right, and the number partioned left and right, for a spatial split with unsplitting, without doing any actual partitioning.
static void spatialPartitionResultWithUnsplitting(const std::vector<SBVHOb>& objects_, const BVHBuilderTri* triangles, const js::AABBox& parent_aabb, int begin, int end, 
	float best_div_val, int best_axis, int best_bucket,
	PartitionRes& res_out)
{
	const SBVHOb* const objects = objects_.data();

	const int num_spatial_buckets = 16;
	const Vec4f spatial_scale = div(Vec4f((float)num_spatial_buckets), (parent_aabb.max_ - parent_aabb.min_));

	int num_left  = 0;
	int num_right = 0;
	js::AABBox left_aabb  = empty_aabb;
	js::AABBox right_aabb = empty_aabb;

	for(int cur = begin; cur < end; ++cur)
	{
		const int ob_i = objects[cur].getIndex();
		const BVHBuilderTri& tri = triangles[ob_i];
		const js::AABBox aabb = objects[cur].aabb;
		assert(parent_aabb.containsAABBox(setWCoordsToOne(aabb)));

		const Vec4i entry_bucket_i = clamp(truncateToVec4i((aabb.min_ - parent_aabb.min_) * spatial_scale), Vec4i(0), Vec4i(num_spatial_buckets-1));
		const Vec4i exit_bucket_i  = clamp(truncateToVec4i((aabb.max_ - parent_aabb.min_) * spatial_scale), Vec4i(0), Vec4i(num_spatial_buckets-1));

		if(entry_bucket_i[best_axis] <= best_bucket && exit_bucket_i[best_axis] > best_bucket) // If triangle straddles split plane:
		{
			const uint32 unsplit_tri = objects[cur].getUnsplit();
			if(unsplit_tri == 1)
			{
				left_aabb.enlargeToHoldAABBox(aabb);
				num_left++;
			}
			else if(unsplit_tri == 2)
			{
				right_aabb.enlargeToHoldAABBox(aabb);
				num_right++;
			}
			else
			{
				// Clip
				js::AABBox tri_left_aabb  = empty_aabb;
				js::AABBox tri_right_aabb = empty_aabb;
				Vec4f v = tri.v[2]; // current vert
				for(int i=0; i<3; ++i) // For each triangle vertex:
				{
					const Vec4f prev_v = v;
					v = tri.v[i];
					if((prev_v[best_axis] < best_div_val && v[best_axis] > best_div_val) || (prev_v[best_axis] > best_div_val && v[best_axis] < best_div_val)) // If edge from prev_v to v straddles clip plane:
					{
						const float t = (best_div_val - prev_v[best_axis]) / (v[best_axis] - prev_v[best_axis]); // Solve for fraction along edge of position on split plane.
						const Vec4f p = prev_v * (1 - t) + v * t;
						tri_left_aabb.enlargeToHoldPoint(p);
						tri_right_aabb.enlargeToHoldPoint(p);
					}

					if(v[best_axis] <= best_div_val) tri_left_aabb.enlargeToHoldPoint(v);
					if(v[best_axis] >= best_div_val) tri_right_aabb.enlargeToHoldPoint(v);
				}

				// Make sure AABBs of clipped triangles don't extend past the current node AABB.
				tri_left_aabb  = intersection(tri_left_aabb, aabb);
				tri_right_aabb = intersection(tri_right_aabb, aabb);

				// Check tri_left_aabb, tri_right_aabb
				assert(parent_aabb.containsAABBox(tri_left_aabb));
				assert(parent_aabb.containsAABBox(tri_right_aabb));
				assert(tri_left_aabb .max_[best_axis] <= best_div_val + (1.0e-4f * std::fabs(best_div_val)) + 1.0e-5f); // Take into account numerical inaccuracy in the clipping. 
				assert(tri_right_aabb.min_[best_axis] >= best_div_val - (1.0e-4f * std::fabs(best_div_val)) - 1.0e-5f);

				left_aabb.enlargeToHoldAABBox(tri_left_aabb);
				right_aabb.enlargeToHoldAABBox(tri_right_aabb);

				num_left++;
				num_right++;
			}
		}
		else // Else if triangle does not straddle clip plane:  then triangle bounds are completely in left or right side. Send triangle left or right based on triangle centroid.
		{
			if(exit_bucket_i[best_axis] <= best_bucket) // If object should go on Left side:
			{
				left_aabb.enlargeToHoldAABBox(aabb);
				num_left++;
			}
			else // else if cur object should go on Right side:
			{
				right_aabb.enlargeToHoldAABBox(aabb);
				num_right++;
			}
		}
	}

	res_out.left_aabb  = left_aabb;
	res_out.right_aabb = right_aabb;
	res_out.num_left  = num_left;
	res_out.num_right = num_right;
}


static GLARE_STRONG_INLINE void addIntersectedEdgeVert(const Vec4f& v_a, const Vec4f& v_b, const float d_a, const float d_b, float split_coord, js::AABBox& left_aabb, js::AABBox& right_aabb)
{
	const float t = (split_coord - d_a) / (d_b - d_a); // Solve for fraction along edge of position on split plane.
	const Vec4f p = v_a * (1 - t) + v_b * t;
	left_aabb.enlargeToHoldPoint(p);
	right_aabb.enlargeToHoldPoint(p);
}


static GLARE_STRONG_INLINE void clipTri(const js::AABBox& ob_aabb, const BVHBuilderTri& tri, int axis, float split_coord, js::AABBox& left_clipped_aabb_out, js::AABBox& right_clipped_aabb_out)
{
	js::AABBox left_aabb  = empty_aabb;
	js::AABBox right_aabb = empty_aabb;

	const Vec4f v0 = tri.v[0];
	const Vec4f v1 = tri.v[1];
	const Vec4f v2 = tri.v[2];

	const float v0d = v0[axis];
	const float v1d = v1[axis];
	const float v2d = v2[axis];

	if(v0d < split_coord)
	{
		left_aabb.enlargeToHoldPoint(v0);
		if(v1d >= split_coord)
		{
			right_aabb.enlargeToHoldPoint(v1);
			// Edge v0-v1 intersects clip plane
			addIntersectedEdgeVert(v0, v1, v0d, v1d, split_coord, left_aabb, right_aabb); // Edge v0-v1

			if(v2d >= split_coord)
			{
				right_aabb.enlargeToHoldPoint(v2);
				// v0 to left, v1 and v2 to right.  Edge v2-v0 intersects clip plane
				addIntersectedEdgeVert(v2, v0, v2d, v0d, split_coord, left_aabb, right_aabb); // Edge v2-v0
			}
			else
			{
				left_aabb.enlargeToHoldPoint(v2);
				// v2 is on left of clip plane, so v1-v2 intersects clip plane.
				addIntersectedEdgeVert(v1, v2, v1d, v2d, split_coord, left_aabb, right_aabb); // Edge v1-v2
			}
		}
		else // Else v1d < split_coord:
		{
			left_aabb.enlargeToHoldPoint(v1);

			//assert(v2d >= split_coord); // At least one vertex should be to right, or on split plane
			if(v2d >= split_coord)
			{
				right_aabb.enlargeToHoldPoint(v2);
				// edges v2-v0 and v1-v2 intersect clip plane
				addIntersectedEdgeVert(v2, v0, v2d, v0d, split_coord, left_aabb, right_aabb); // Edge v2-v0
				addIntersectedEdgeVert(v1, v2, v1d, v2d, split_coord, left_aabb, right_aabb); // Edge v1-v2
			}
			else
			{
				// v0d, v1d, v2d are all < split coord
				left_aabb.enlargeToHoldPoint(v2);
			}
		}
	}
	else if(v0d > split_coord)
	{
		right_aabb.enlargeToHoldPoint(v0);
		if(v1d <= split_coord)
		{
			left_aabb.enlargeToHoldPoint(v1);
			// Edge v0-v1 intersects clip plane
			addIntersectedEdgeVert(v0, v1, v0d, v1d, split_coord, left_aabb, right_aabb); // Edge v0-v1

			if(v2d <= split_coord)
			{
				left_aabb.enlargeToHoldPoint(v2);
				// v1, v2 are on left, v0 on right, so Edge v2-v0 intersects clip plane
				addIntersectedEdgeVert(v2, v0, v2d, v0d, split_coord, left_aabb, right_aabb); // Edge v2-v0
			}
			else
			{
				right_aabb.enlargeToHoldPoint(v2);
				// v0, v2 are on right, v1 on left of clip plane, so v1-v2 intersects clip plane.
				addIntersectedEdgeVert(v1, v2, v1d, v2d, split_coord, left_aabb, right_aabb); // Edge v1-v2
			}
		}
		else // Else v1d > split_coord:
		{
			right_aabb.enlargeToHoldPoint(v1);
			//assert(v2d <= split_coord); // At least one vertex should be to left, or on split plane
			if(v2d <= split_coord)
			{
				left_aabb.enlargeToHoldPoint(v2);
				// edges v2-v0 and v1-v2 intersect clip plane
				addIntersectedEdgeVert(v2, v0, v2d, v0d, split_coord, left_aabb, right_aabb); // Edge v2-v0
				addIntersectedEdgeVert(v1, v2, v1d, v2d, split_coord, left_aabb, right_aabb); // Edge v1-v2
			}
			else
			{
				// v0d, v1d and v2d are all > split_coord
				right_aabb.enlargeToHoldPoint(v2);
			}
		}
	}
	else if(v0d == split_coord)
	{
		left_aabb.enlargeToHoldPoint(v0);
		right_aabb.enlargeToHoldPoint(v0);

		if(v1d <= split_coord) 
			left_aabb.enlargeToHoldPoint(v1);
		if(v1d >= split_coord) 
			right_aabb.enlargeToHoldPoint(v1);
		if(v2d <= split_coord) 
			left_aabb.enlargeToHoldPoint(v2);
		if(v2d >= split_coord) 
			right_aabb.enlargeToHoldPoint(v2);

		if((v1d < split_coord && v2d > split_coord) || (v1d > split_coord && v2d < split_coord)) // If edge 1-2 straddles left clip plane:
			addIntersectedEdgeVert(v1, v2, v1d, v2d, split_coord, left_aabb, right_aabb); // Edge v1-v2
	}


	left_aabb  = intersection(ob_aabb, left_aabb);
	right_aabb = intersection(ob_aabb, right_aabb);

	assert(ob_aabb.containsAABBox(left_aabb));
	assert(ob_aabb.containsAABBox(right_aabb));
	assert(left_aabb .max_[axis] <= split_coord + (2.0e-3f * std::fabs(split_coord)) + 1.0e-5f);
	assert(right_aabb.min_[axis] >= split_coord - (2.0e-3f * std::fabs(split_coord)) - 1.0e-5f);

	left_clipped_aabb_out  = left_aabb;
	right_clipped_aabb_out = right_aabb;
}


static void searchForBestSplit(const js::AABBox& aabb, const js::AABBox& centroid_aabb_, const std::vector<uint64>& max_obs_at_depth, int depth, std::vector<SBVHOb>& objects_, const BVHBuilderTri* triangles, SBVHBuildStats& stats, int begin, int end, int capacity, float recip_root_node_aabb_area,
	int& best_axis_out, float& best_div_val_out,
	float& smallest_split_cost_factor_out, bool& best_split_is_spatial_out,
	int& best_num_left_out, int& best_num_right_out, 
	int& best_bucket_out
	)
{
	const js::AABBox& centroid_aabb = centroid_aabb_;
	SBVHOb* const objects = objects_.data();

	const Vec4f aabb_span = aabb.max_ - aabb.min_;

	float smallest_split_cost_factor = std::numeric_limits<float>::infinity(); // Smallest (N_L * half_left_surface_area + N_R * half_right_surface_area) found.
	int best_axis = -1;
	float best_div_val = 0;
	bool best_split_is_spatial = false;
	int best_num_left = -1;
	int best_num_right = -1;
	int best_bucket = -1;

	//===========================================================================================================
	// Look for object-partitioning splits - where the objects/triangles are partitioned left or right based on the object centroid.
	// Objects are not clipped to the splitting plane, but instead the left or right AABBs are extended to enclose the objects.

	const int max_B = 32;
	const int N = end - begin;
	const int num_buckets = myMin(max_B, (int)(4 + 0.05f * N)); // buckets per axis
	js::AABBox bucket_aabbs[max_B * 3]; // For each axis, holds AABBs of tris whose centroids fall in the bucket.
	Vec4i counts[max_B];
	for(int i=0; i<num_buckets; ++i)
	{
		for(int z=0; z<3; ++z)
			bucket_aabbs[z*max_B + i] = empty_aabb;
		counts[i] = Vec4i(0);
	}

	// Compute bucket_aabbs
	const Vec4f scale = div(Vec4f((float)num_buckets), (centroid_aabb.max_ - centroid_aabb.min_));
	for(int i=begin; i<end; ++i)
	{
		const js::AABBox ob_aabb = objects[i].aabb;
		const Vec4f centroid = ob_aabb.centroid();
		assert(centroid_aabb.contains(setWToOne(centroid)));
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

	const Vec4f axis_len_over_num_buckets = div(centroid_aabb.max_ - centroid_aabb.min_, Vec4f((float)num_buckets));
	Vec4f right_area[max_B];
	js::AABBox right_aabb[3]; // For each axis, AABBs of all objects whose centroids fall in buckets [b, num_buckets-1]
	Vec4i count(0);
	js::AABBox left_aabb[3]; // For each axis, AABBs of all objects whose centroids fall in buckets [0, b]

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
				best_div_val = centroid_aabb.min_[0] + axis_len_over_num_buckets[0] * (b + 1);
				best_num_left  = elem<0>(left_count);
				best_num_right = elem<0>(right_count);
			}
			if(min_counts[1] > max_min_count)
			{
				max_min_count = min_counts[1];
				best_bucket = b;
				best_axis = 1;
				best_div_val = centroid_aabb.min_[1] + axis_len_over_num_buckets[1] * (b + 1);
				best_num_left  = elem<1>(left_count);
				best_num_right = elem<1>(right_count);
			}
			if(min_counts[2] > max_min_count)
			{
				max_min_count = min_counts[2];
				best_bucket = b;
				best_axis = 2;
				best_div_val = centroid_aabb.min_[2] + axis_len_over_num_buckets[2] * (b + 1);
				best_num_left  = elem<2>(left_count);
				best_num_right = elem<2>(right_count);
			}
		}

		smallest_split_cost_factor = 0; // What should this be set to? Set to zero for now to avoid creating leaves instead

		best_axis_out = best_axis;
		best_div_val_out = best_div_val;
		smallest_split_cost_factor_out = smallest_split_cost_factor;
		best_split_is_spatial_out = best_split_is_spatial;
		best_num_left_out = best_num_left;
		best_num_right_out = best_num_right;
		best_bucket_out = best_bucket;
		return;
	}
	else
	{
		// Sweep right to left computing exclusive prefix surface areas
		
		
		for(int i=0; i<3; ++i)
			right_aabb[i] = empty_aabb;

		for(int b=num_buckets-1; b>=0; --b)
		{
			const float A0 = right_aabb[0].getHalfSurfaceArea();
			const float A1 = right_aabb[1].getHalfSurfaceArea();
			const float A2 = right_aabb[2].getHalfSurfaceArea();
			right_area[b] = Vec4f(A0, A1, A2, A2);

			right_aabb[0].enlargeToHoldAABBox(bucket_aabbs[b]);
			right_aabb[1].enlargeToHoldAABBox(bucket_aabbs[b +   max_B]);
			right_aabb[2].enlargeToHoldAABBox(bucket_aabbs[b + 2*max_B]);

			assert(aabb.containsAABBox(setWCoordsToOne(right_aabb[0])));
		}

		// Sweep left to right, computing inclusive left prefix surface area and counts, and computing overall cost factor
		
		for(int i=0; i<3; ++i)
			left_aabb[i] = empty_aabb;

		for(int b=0; b<num_buckets-1; ++b)
		{
			count = count + counts[b];

			left_aabb[0].enlargeToHoldAABBox(bucket_aabbs[b]);
			left_aabb[1].enlargeToHoldAABBox(bucket_aabbs[b +   max_B]);
			left_aabb[2].enlargeToHoldAABBox(bucket_aabbs[b + 2*max_B]);

			assert(aabb.containsAABBox(setWCoordsToOne(left_aabb[0])));

			const float A0 = left_aabb[0].getHalfSurfaceArea();
			const float A1 = left_aabb[1].getHalfSurfaceArea();
			const float A2 = left_aabb[2].getHalfSurfaceArea();

			const Vec4i right_count = Vec4i(N) - Vec4i(count);
			const Vec4f cost = toVec4f(count) * Vec4f(A0, A1, A2, A2) + toVec4f(right_count) * right_area[b]; // Compute SAH cost factor

			if(smallest_split_cost_factor > elem<0>(cost))
			{
				smallest_split_cost_factor = elem<0>(cost);
				best_axis = 0;
				best_div_val = centroid_aabb.min_[0] + axis_len_over_num_buckets[0] * (b + 1);
				best_num_left  = elem<0>(count);
				best_num_right = elem<0>(right_count);
				best_bucket = b;
			}

			if(smallest_split_cost_factor > elem<1>(cost))
			{
				smallest_split_cost_factor = elem<1>(cost);
				best_axis = 1;
				best_div_val = centroid_aabb.min_[1] + axis_len_over_num_buckets[1] * (b + 1);
				best_num_left  = elem<1>(count);
				best_num_right = elem<1>(right_count);
				best_bucket = b;
			}

			if(smallest_split_cost_factor > elem<2>(cost))
			{
				smallest_split_cost_factor = elem<2>(cost);
				best_axis = 2;
				best_div_val = centroid_aabb.min_[2] + axis_len_over_num_buckets[2] * (b + 1);
				best_num_left  = elem<2>(count);
				best_num_right = elem<2>(right_count);
				best_bucket = b;
			}
		}
	}

	if(best_axis == -1)
	{
		best_axis_out = best_axis;
		best_div_val_out = best_div_val;
		smallest_split_cost_factor_out = smallest_split_cost_factor;
		best_split_is_spatial_out = best_split_is_spatial;
		return;
	}

	if(end - begin == capacity)
	{
		//conPrint("hit capacity, ignoring spatial split possibility. capacity=" + toString(capacity));
		stats.num_hit_capacity_splits++;
	}

	if(capacity > end - begin) // Only attempt spatial splits if there is room for new split objects.
	{
	//===========================================================================================================
	// Look for spatial splits - splitting planes where triangles intersecting the plane are clipped into two polygons, one going to left child and one going to the right child

	// Compute lambda - surface area of intersection of left and right child AABBs.
	// Start by computing the AABBs of all the objects split left and right.  Use our bucket AABBs and best_bucket to avoid iterating over all objects.
	js::AABBox best_left_aabb = empty_aabb;
	const int axis_offset = best_axis * max_B;
	for(int b=0; b<=best_bucket; ++b)
		best_left_aabb.enlargeToHoldAABBox(bucket_aabbs[b + axis_offset]);

	js::AABBox best_right_aabb = empty_aabb;
	for(int b=num_buckets-1; b>best_bucket; --b)
		best_right_aabb.enlargeToHoldAABBox(bucket_aabbs[b + axis_offset]);

	/* // Reference code:
	js::AABBox ref_best_left_aabb = empty_aabb;
	js::AABBox ref_best_right_aabb = empty_aabb;
	for(int i=begin; i<end; ++i)
	{
		const js::AABBox ob_aabb = objects[i].aabb;
		const Vec4f centroid = ob_aabb.centroid();
		if(centroid[best_axis] <= best_div_val) // If object should go on Left side:
			ref_best_left_aabb.enlargeToHoldAABBox(ob_aabb);
		else // else if cur object should go on Right side:
			ref_best_right_aabb.enlargeToHoldAABBox(ob_aabb);
	}
	assert(best_left_aabb == ref_best_left_aabb);
	assert(best_right_aabb == ref_best_right_aabb);*/

	setAABBWToOneInPlace(best_left_aabb);
	setAABBWToOneInPlace(best_right_aabb);

	int spatial_best_axis = -1;
	float spatial_best_div_val = -666;
	float spatial_smallest_split_cost_factor = std::numeric_limits<float>::infinity();
	int spatial_best_bucket = -1;

	js::AABBox best_spatial_left_aabb;
	js::AABBox best_spatial_right_aabb;
	int best_spatial_left_num = 0;
	int best_spatial_right_num = 0;

	const js::AABBox inter = intersection(best_left_aabb, best_right_aabb);
	const float lambda = inter.isEmpty() ? 0 : inter.getSurfaceArea();

	const float lambda_over_sa_root_bound = lambda * recip_root_node_aabb_area;
	const float alpha = 1.0e-5f;
	if(lambda_over_sa_root_bound > alpha) // If should do search for a spatial split:
	{
		const int num_spatial_buckets = 16;
		assert(num_spatial_buckets <= max_B);
		
		// We will store in each bucket, the AABBs of all triangles in [begin, end) clipped to the bucket bounds.
		Vec4i entry_counts[max_B];
		Vec4i exit_counts[max_B];
		for(int i=0; i<num_spatial_buckets; ++i)
		{
			for(int z=0; z<3; ++z)
				bucket_aabbs[z*max_B + i] = empty_aabb;
			entry_counts[i] = Vec4i(0);
			exit_counts [i] = Vec4i(0);
		}

		const Vec4f spatial_scale = div(Vec4f((float)num_spatial_buckets), (aabb.max_ - aabb.min_));
		const Vec4f spatial_axis_len_over_num_buckets = div(aabb.max_ - aabb.min_, Vec4f((float)num_spatial_buckets));

		for(int i=begin; i<end; ++i)
		{
			const int ob_i = objects[i].getIndex();
			const js::AABBox ob_aabb = objects[i].aabb;
			assert(aabb.containsAABBox(setWCoordsToOne(ob_aabb)));
			const BVHBuilderTri& tri = triangles[ob_i];
			
			// Pre-fetch a triangle.  This helps because it's an indirect memory access to get the triangle.
			const int PREFETCH_DIST = 8;
			if(i + PREFETCH_DIST < end)
				_mm_prefetch((const char*)(triangles + (objects[i + PREFETCH_DIST].getIndex())), _MM_HINT_T0);

			const Vec4i entry_bucket_i = clamp(truncateToVec4i((ob_aabb.min_ - aabb.min_) * spatial_scale), Vec4i(0), Vec4i(num_spatial_buckets-1));
			const Vec4i exit_bucket_i  = clamp(truncateToVec4i((ob_aabb.max_ - aabb.min_) * spatial_scale), Vec4i(0), Vec4i(num_spatial_buckets-1));

			assert(entry_bucket_i[0] >= 0 && entry_bucket_i[0] < num_spatial_buckets);
			assert(entry_bucket_i[1] >= 0 && entry_bucket_i[1] < num_spatial_buckets);
			assert(entry_bucket_i[2] >= 0 && entry_bucket_i[2] < num_spatial_buckets);
			assert(exit_bucket_i[0] >= 0 && exit_bucket_i[0] < num_spatial_buckets);
			assert(exit_bucket_i[1] >= 0 && exit_bucket_i[1] < num_spatial_buckets);
			assert(exit_bucket_i[2] >= 0 && exit_bucket_i[2] < num_spatial_buckets);
			assert((entry_bucket_i[0] <= exit_bucket_i[0]) && (entry_bucket_i[1] <= exit_bucket_i[1]) && (entry_bucket_i[2] <= exit_bucket_i[2]));

			// Increase bucket AABBs for x axis
			for(int axis=0; axis<3; ++axis)
			{
				if(aabb_span[axis] > 0)
				{
					const int entry_b = entry_bucket_i[axis];
					const int exit_b  = exit_bucket_i[axis];
					if(entry_b == exit_b) // Special case for when triangle is completely in one bucket - no clipping is needed
						bucket_aabbs[entry_b + axis*max_B].enlargeToHoldAABBox(ob_aabb);
					else
					{
						js::AABBox last_to_right_aabb = ob_aabb; // bounds of triangle to the right of last clip plane.  Starts with the full bounds of the triangle (as clipped to current node AABB)
						for(int b=entry_b; b<exit_b; ++b)
						{
							// Clip triangle against current clip plane, taking the intersection of the results with last_to_right_aabb as well.
							js::AABBox to_left_aabb;
							js::AABBox to_right_aabb;
							clipTri(last_to_right_aabb, tri, axis, aabb.min_[axis] + spatial_axis_len_over_num_buckets[axis] * (b + 1), to_left_aabb, to_right_aabb);
							last_to_right_aabb = to_right_aabb;

							bucket_aabbs[b + axis*max_B].enlargeToHoldAABBox(to_left_aabb);
						}

						bucket_aabbs[exit_b + axis*max_B].enlargeToHoldAABBox(last_to_right_aabb);
					}

					(entry_counts[entry_b])[axis]++;
					(exit_counts[exit_b])  [axis]++;
				}
			}
		}

		// Sweep right to left, computing exclusive prefix surface areas and counts
		// right_area[b] = sum_{i=(b+1)}^{num_spatial_buckets} bucket_aabb[b].getHalfSurfaceArea()
		Vec4i right_prefix_counts[max_B];
		count = Vec4i(0);
		for(int i=0; i<3; ++i)
			right_aabb[i] = empty_aabb;

		js::AABBox right_aabbs[num_spatial_buckets*3];

		for(int b=num_spatial_buckets-1; b>=0; --b)
		{
			right_prefix_counts[b] = count;
			count = count + exit_counts[b];

			float A0 = right_aabb[0].getHalfSurfaceArea();
			float A1 = right_aabb[1].getHalfSurfaceArea();
			float A2 = right_aabb[2].getHalfSurfaceArea();
			right_area[b] = Vec4f(A0, A1, A2, A2);

			right_aabbs[b + num_spatial_buckets*0] = right_aabb[0]; // Store right AABBs
			right_aabbs[b + num_spatial_buckets*1] = right_aabb[1];
			right_aabbs[b + num_spatial_buckets*2] = right_aabb[2];

			right_aabb[0].enlargeToHoldAABBox(bucket_aabbs[b]);
			right_aabb[1].enlargeToHoldAABBox(bucket_aabbs[b +   max_B]);
			right_aabb[2].enlargeToHoldAABBox(bucket_aabbs[b + 2*max_B]);

			assert(aabb.containsAABBox(setWCoordsToOne(right_aabb[0])));
		}

		// Sweep left to right, computing inclusive left prefix surface area and counts, and computing overall cost factor
		count = Vec4i(0);
		for(int i=0; i<3; ++i)
			left_aabb[i] = empty_aabb;

		for(int b=0; b<num_spatial_buckets-1; ++b)
		{
			count = count + entry_counts[b];

			left_aabb[0].enlargeToHoldAABBox(bucket_aabbs[b]);
			left_aabb[1].enlargeToHoldAABBox(bucket_aabbs[b +   max_B]);
			left_aabb[2].enlargeToHoldAABBox(bucket_aabbs[b + 2*max_B]);

			assert(aabb.containsAABBox(setWCoordsToOne(left_aabb[0])));

			float A0 = left_aabb[0].getHalfSurfaceArea();
			float A1 = left_aabb[1].getHalfSurfaceArea();
			float A2 = left_aabb[2].getHalfSurfaceArea();

			const Vec4f cost = toVec4f(count) * Vec4f(A0, A1, A2, A2) + toVec4f(right_prefix_counts[b]) * right_area[b]; // Compute SAH cost factor

			if(aabb_span[0] > 0 && spatial_smallest_split_cost_factor > elem<0>(cost))
			{
				spatial_smallest_split_cost_factor = elem<0>(cost);
				spatial_best_axis = 0;
				spatial_best_div_val = aabb.min_[0] + spatial_axis_len_over_num_buckets[0] * (b + 1);
				spatial_best_bucket = b;
				best_spatial_left_aabb = left_aabb[0];
				best_spatial_right_aabb = right_aabbs[b + num_spatial_buckets*0];
				best_spatial_left_num = count[0];
				best_spatial_right_num = right_prefix_counts[b][0];
			}

			if(aabb_span[1] > 0 && spatial_smallest_split_cost_factor > elem<1>(cost))
			{
				spatial_smallest_split_cost_factor = elem<1>(cost);
				spatial_best_axis = 1;
				spatial_best_div_val = aabb.min_[1] + spatial_axis_len_over_num_buckets[1] * (b + 1);
				spatial_best_bucket = b;
				best_spatial_left_aabb = left_aabb[1];
				best_spatial_right_aabb = right_aabbs[b + num_spatial_buckets*1];
				best_spatial_left_num = count[1];
				best_spatial_right_num = right_prefix_counts[b][1];
			}

			if(aabb_span[2] > 0 && spatial_smallest_split_cost_factor > elem<2>(cost))
			{
				spatial_smallest_split_cost_factor = elem<2>(cost);
				spatial_best_axis = 2;
				spatial_best_div_val = aabb.min_[2] + spatial_axis_len_over_num_buckets[2] * (b + 1);
				spatial_best_bucket = b;
				best_spatial_left_aabb = left_aabb[2];
				best_spatial_right_aabb = right_aabbs[b + num_spatial_buckets*2];
				best_spatial_left_num = count[2];
				best_spatial_right_num = right_prefix_counts[b][2];
			}
		}
	
		if(spatial_smallest_split_cost_factor < smallest_split_cost_factor && (best_spatial_left_num + best_spatial_right_num <= capacity))
		{
			smallest_split_cost_factor = spatial_smallest_split_cost_factor;
			best_axis = spatial_best_axis;
			best_div_val = spatial_best_div_val;
			best_bucket = spatial_best_bucket;
			best_split_is_spatial = true;
			best_num_left = best_spatial_left_num;
			best_num_right = best_spatial_right_num;
		}

		// Do reference unsplitting at the best spatial split found.
		// See 'Spatial Splits in Bounding Volume Hierarchies' (http://citeseerx.ist.psu.edu/viewdoc/download?doi=10.1.1.550.6560&rep=rep1&type=pdf) section 4.4, 'Reference Unsplitting'.
		// The starting point is the best spatial split.
		// Note that we want to try this unsplitting even if the spatial split was not better than the object partitioning split, because the unsplit might be.
		if(spatial_smallest_split_cost_factor < std::numeric_limits<float>::infinity()) // If we had a successful spatial split:
		{
			const float left_half_area  = best_spatial_left_aabb .getHalfSurfaceArea();
			const float right_half_area = best_spatial_right_aabb.getHalfSurfaceArea();
			const float C_split = spatial_smallest_split_cost_factor;
			const float left_reduced_cost  = left_half_area  * (best_spatial_left_num  - 1);
			const float right_reduced_cost = right_half_area * (best_spatial_right_num - 1);
			bool unsplit_a_tri = false; // Did we unsplit 1 or more triangles?
			for(int i=begin; i<end; ++i)
			{
				const js::AABBox ob_aabb = objects[i].aabb;
				assert(aabb.containsAABBox(setWCoordsToOne(ob_aabb)));

				const Vec4i entry_bucket_i = clamp(truncateToVec4i((ob_aabb.min_ - aabb.min_) * spatial_scale), Vec4i(0), Vec4i(num_spatial_buckets-1));
				const Vec4i exit_bucket_i  = clamp(truncateToVec4i((ob_aabb.max_ - aabb.min_) * spatial_scale), Vec4i(0), Vec4i(num_spatial_buckets-1));

				uint32 unsplit = 0;

				if(entry_bucket_i[spatial_best_axis] <= spatial_best_bucket && exit_bucket_i[spatial_best_axis] > spatial_best_bucket) // If triangle straddles split plane:
				{
					// Compute C_1: the cost when putting object i entirely in the left child, and C_2: the cost when putting object i entirely in the right child.
					// These are conservative costs, e.g. >= the true cost (because we don't shrink the AABB of the side the object was removed from)
					const js::AABBox expanded_left_aabb  = AABBUnion(best_spatial_left_aabb, ob_aabb);
					const js::AABBox expanded_right_aabb = AABBUnion(best_spatial_right_aabb, ob_aabb);
					const float C_1 = expanded_left_aabb.getHalfSurfaceArea() * best_spatial_left_num + right_reduced_cost;
					const float C_2 = left_reduced_cost + expanded_right_aabb.getHalfSurfaceArea() * best_spatial_right_num;

					if(C_1 < C_split && C_1 <= C_2) // Only place this tri in the left child:
						unsplit = 1;
					else if(C_2 < C_split && C_2 < C_1) // Else if only place this tri in the right child:
						unsplit = 2;
					unsplit_a_tri = true;
				}

				objects[i].setUnsplit(unsplit);
			}

			// Do another pass to get the final cost given our unsplitting decisions.  This is not a conservative estimate of the cost, but an accurate calculation of the 
			// cost, that may be smaller than the conservative unsplit cost.  We want to do this pass because, even though we know the cost will be <= spatial_smallest_split_cost_factor,
			// We don't know if it will be smaller than the best object-partitioning cost (which may be smaller than spatial_smallest_split_cost_factor).
			if(unsplit_a_tri) // Cost will only have changed if we actually unsplit something.
			{
				PartitionRes res;
				spatialPartitionResultWithUnsplitting(objects_, triangles, aabb, begin, end, spatial_best_div_val, spatial_best_axis, spatial_best_bucket, res);

				if(res.num_left > 0 && res.num_right > 0) // If valid partition: (might end up with everything left or right due to numerical innaccuracy)
				{
					const float unsplit_cost = res.left_aabb.getHalfSurfaceArea() * res.num_left + res.right_aabb.getHalfSurfaceArea() * res.num_right;

					// The unsplit cost is usually, not not allways smaller than spatial_smallest_split_cost_factor.
					assert(isFinite(unsplit_cost));

					if(unsplit_cost < smallest_split_cost_factor && (res.num_left + res.num_right <= capacity))
					{
						smallest_split_cost_factor = unsplit_cost;
						best_axis = spatial_best_axis;
						best_div_val = spatial_best_div_val;
						best_bucket = spatial_best_bucket;
						best_split_is_spatial = true;
						best_num_left  = res.num_left;
						best_num_right = res.num_right;
					}
				}
			}
		}
	} // End if should do search for spatial split.

	} // end if(capacity > end - begin)
	//========================================================================================================

	best_axis_out = best_axis;
	best_div_val_out = best_div_val;
	smallest_split_cost_factor_out = smallest_split_cost_factor;
	best_split_is_spatial_out = best_split_is_spatial;
	best_num_left_out = best_num_left;
	best_num_right_out = best_num_right;
	best_bucket_out = best_bucket;
}


void SBVHBuilder::markNodeAsLeaf(SBVHPerThreadTempInfo& thread_temp_info, const js::AABBox& aabb, uint32 node_index, int begin, int end, int depth, SBVHResultChunk* node_result_chunk)
{
	const int num_objects = end - begin;
	assert(num_objects <= max_num_objects_per_leaf);

	SBVHLeafResultChunk* leaf_result_chunk = thread_temp_info.leaf_result_chunk;

	// Make sure we have space for leaf geom in leaf_result_chunk.
	// Supose MAX_RESULT_CHUNK_SIZE = 4, num_objects=2.
	// Then if size >= 3 (or size > 2) then we don't have enough room for num_objects.
	if(leaf_result_chunk->size > SBVHLeafResultChunk::MAX_RESULT_CHUNK_SIZE - num_objects) // If there is not room for leaf geom:
		leaf_result_chunk = thread_temp_info.leaf_result_chunk = allocNewLeafResultChunk();

	// Make this a leaf node
	node_result_chunk->nodes[node_index].interior = false;
	node_result_chunk->nodes[node_index].aabb = aabb;
	node_result_chunk->nodes[node_index].depth = (uint8)depth;
	node_result_chunk->nodes[node_index].left  = (int)(leaf_result_chunk->chunk_offset + leaf_result_chunk->size);
	node_result_chunk->nodes[node_index].right = (int)(leaf_result_chunk->chunk_offset + leaf_result_chunk->size + num_objects);

	for(int i=0; i<num_objects; ++i)
		leaf_result_chunk->leaf_obs[leaf_result_chunk->size + i] = this->top_level_objects[begin + i].getIndex();
	leaf_result_chunk->size += num_objects;

	assert(leaf_result_chunk->size <= SBVHLeafResultChunk::MAX_RESULT_CHUNK_SIZE);

	// Update build stats
	if(depth >= max_depth)
		thread_temp_info.stats.num_maxdepth_leaves++;
	else
		thread_temp_info.stats.num_under_thresh_leaves++;
	thread_temp_info.stats.leaf_depth_sum += depth;
	thread_temp_info.stats.max_leaf_depth = myMax(thread_temp_info.stats.max_leaf_depth, depth);
	thread_temp_info.stats.num_tris_in_leaves += num_objects;
	thread_temp_info.stats.max_num_tris_per_leaf = myMax(thread_temp_info.stats.max_num_tris_per_leaf, num_objects);
	thread_temp_info.stats.num_leaves++;
}


/*
Recursively build a subtree.
Assumptions: root node for subtree is already created and is at node_index
*/
void SBVHBuilder::doBuild(
			SBVHPerThreadTempInfo& thread_temp_info,
			const js::AABBox& aabb,
			const js::AABBox& centroid_aabb,
			uint32 node_index, 
			int begin,
			int end,
			int capacity,
			int depth,
			SBVHResultChunk* node_result_chunk
			)
{
	assert(end > begin);
	assert(capacity >= (end - begin));
	const int num_objects = end - begin;

	
	if(depth <= 5)
	{
		// conPrint("Checking for cancel at depth " + toString(depth));
		if(should_cancel_callback->shouldCancel())
		{
			// conPrint("Cancelling!");
			throw glare::CancelledException();
		}
	}

	SBVHLeafResultChunk* leaf_result_chunk = thread_temp_info.leaf_result_chunk;

	if(num_objects <= leaf_num_object_threshold || depth >= max_depth)
	{
		if(num_objects > max_num_objects_per_leaf)
		{
			// We hit max depth but there are too many objects for a leaf.  So the build has failed.
			thread_temp_info.build_failed = true;
			return;
		}

		markNodeAsLeaf(thread_temp_info, aabb, node_index, begin, end, depth, node_result_chunk);
		return;
	}

	const float traversal_cost = 1.0f;
	const float non_split_cost = num_objects * intersection_cost; // Eqn 2.
	float smallest_split_cost_factor; // Smallest (N_L * half_left_surface_area + N_R * half_right_surface_area) found.
	int best_axis;// = -1;
	float best_div_val;// = 0;
	bool best_split_was_spatial;
	int best_num_left, best_num_right, best_bucket;

	//conPrint("Looking for best split...");
	//Timer timer;
	searchForBestSplit(aabb, centroid_aabb, max_obs_at_depth, depth, this->top_level_objects, triangles, thread_temp_info.stats, begin, end, capacity, recip_root_node_aabb_area, best_axis, best_div_val, smallest_split_cost_factor, best_split_was_spatial,
		best_num_left, best_num_right, best_bucket
	);
	//conPrint("Looking for best split done.  elapsed: " + timer.elapsedString());
	//split_search_time += timer.elapsed();

	const uint64 max_num_obs_at_depth_plus_1 = max_obs_at_depth[myMin((int)max_obs_at_depth.size() - 1, depth + 1)];
	if((uint64)best_num_left > max_num_obs_at_depth_plus_1 || (uint64)best_num_right > max_num_obs_at_depth_plus_1) // If we have exceeded the max number of objects allowed at a given depth:
		best_axis = -1; // Re-partition, splitting as evenly as possible in terms of numbers left and right.

	PartitionRes res;
	if(best_axis == -1) // This can happen when all object centre coordinates along the axis are the same.
	{
		// Form a leaf when allowed, since arbitrary partitioning will just add extra traversal steps, all obs will still have to be intersected against.
		if(num_objects <= max_num_objects_per_leaf)
		{
			markNodeAsLeaf(thread_temp_info, aabb, node_index, begin, end, depth, node_result_chunk);
			return;
		}
		
		//conPrint("Best axis was -1, doing arbitrary partition");
		arbitraryPartition(this->top_level_objects, begin, end, capacity, res);
	}
	else
	{
		if(best_split_was_spatial)
			thread_temp_info.stats.num_spatial_splits++;
		else
			thread_temp_info.stats.num_object_splits++;

		// C_split = P_L N_L C_i + P_R N_R C_i + C_t
		//         = A_L/A N_L C_i + A_R/A N_R C_i + C_t
		//         = C_i (A_L N_L + A_R N_R) / A + C_t
		//         = C_i (A_L/2 N_L + A_R/2 N_R) / (A/2) + C_t
		const float smallest_split_cost = intersection_cost * smallest_split_cost_factor / aabb.getHalfSurfaceArea() + traversal_cost;

		// If it is not advantageous to split, and the number of objects is <= the max num per leaf, then form a leaf:
		if((smallest_split_cost >= non_split_cost) && (num_objects <= max_num_objects_per_leaf))
		{
			markNodeAsLeaf(thread_temp_info, aabb, node_index, begin, end, depth, node_result_chunk);
			return;
		}

		// draw partition
#if ALLOW_DEBUG_DRAWING
		if(DEBUG_DRAW)
		{
			drawAABB(main_map, Colour3f(0.5f, 0.2f, 0.2f), aabb);
			drawPartitionLine(main_map, aabb, best_axis, best_div_val, best_split_was_spatial);
		}
#endif

		//timer.reset();
		partition(this->top_level_objects, this->temp_obs, begin, end, capacity, triangles, aabb, centroid_aabb, best_div_val, best_axis, best_split_was_spatial, best_bucket, best_num_left, best_num_right, res);
		//partition_time += timer.elapsed();
		//conPrint("Partition done.  elapsed: " + timer.elapsedString());
	}


	setAABBWToOneInPlace(res.left_aabb);
	setAABBWToOneInPlace(res.left_centroid_aabb);
	setAABBWToOneInPlace(res.right_aabb);
	setAABBWToOneInPlace(res.right_centroid_aabb);

	//assert(aabb.containsAABBox(res.left_aabb));
	//assert(res.left_aabb.containsAABBox(res.left_centroid_aabb)); // failing due to precision issues?
	//assert(centroid_aabb.containsAABBox(res.left_centroid_aabb));
	//assert(aabb.containsAABBox(res.right_aabb));
	//assert(res.right_aabb.containsAABBox(res.right_centroid_aabb)); // failing due to precision issues?
	//assert(centroid_aabb.containsAABBox(res.right_centroid_aabb));

	const int num_left  = res.num_left;
	const int num_right = res.num_right;
	const int left_capacity = res.left_capacity;
	const int right_begin = begin + left_capacity;
	const int right_capacity = capacity - left_capacity;

	// Check the result AABBs and centroid AABBS
#ifndef NDEBUG
	{
		// Check left objects
		js::AABBox left_centroid_aabb = js::AABBox::emptyAABBox();
		for(int i=begin; i<begin + num_left; ++i)
		{
			const Vec4f centroid = setWToOne(this->top_level_objects[i].aabb.centroid());
			left_centroid_aabb.enlargeToHoldPoint(centroid);
			assert(res.left_centroid_aabb.contains(centroid));
		}
		assert(setWCoordsToOne(left_centroid_aabb) == setWCoordsToOne(res.left_centroid_aabb));
	}
#endif

	const uint32 left_child = allocNode(thread_temp_info); // Allocate left child node from the thread's current result chunk
	SBVHResultChunk* left_child_chunk = thread_temp_info.result_chunk;

	const bool do_right_child_in_new_task = (num_left >= new_task_num_ob_threshold) && (num_right >= new_task_num_ob_threshold);

	// Allocate right child
	SBVHResultChunk* right_child_chunk;
	SBVHLeafResultChunk* right_child_leaf_chunk;
	uint32 right_child;
	if(!do_right_child_in_new_task)
	{
		right_child = allocNode(thread_temp_info);
		right_child_chunk = thread_temp_info.result_chunk;
		right_child_leaf_chunk = leaf_result_chunk;
	}
	else
	{
		right_child_chunk = allocNewResultChunk(); // This child subtree will be built in a new task, so probably in a different thread.  So use a new chunk for it.
		right_child_leaf_chunk = allocNewLeafResultChunk();
		right_child = 0; // Root node of new task subtree chunk.

		// Build right subtree in a new task
		Reference<SBVHBuildSubtreeTask> subtree_task = new SBVHBuildSubtreeTask(*this);
		subtree_task->result_chunk = right_child_chunk;
		subtree_task->leaf_result_chunk = right_child_leaf_chunk;
		subtree_task->node_aabb = res.right_aabb;
		subtree_task->centroid_aabb = res.right_centroid_aabb;
		subtree_task->depth = depth + 1;
		subtree_task->begin = right_begin;
		subtree_task->end = right_begin + num_right;
		subtree_task->capacity = right_capacity;
		task_manager->addTask(subtree_task); // Launch the task
	}

	// Mark this node as an interior node.
	node_result_chunk->nodes[node_index].interior = true;
	node_result_chunk->nodes[node_index].aabb = aabb;
	node_result_chunk->nodes[node_index].left  = (int)(left_child  + left_child_chunk->chunk_offset);
	node_result_chunk->nodes[node_index].right = (int)(right_child + right_child_chunk->chunk_offset);
	node_result_chunk->nodes[node_index].right_child_chunk_index = -1;
	node_result_chunk->nodes[node_index].depth = (uint8)depth;

	thread_temp_info.stats.num_interior_nodes++;

	// Build left child
	doBuild(
		thread_temp_info,
		res.left_aabb, // aabb
		res.left_centroid_aabb,
		left_child, // node index
		begin, // begin
		begin + num_left, // end
		left_capacity, // capacity
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
			right_begin, // begin
			right_begin + num_right, // end
			right_capacity, // capacity
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
#include "../utils/Plotter.h"
#include "../utils/FileUtils.h"


static void rotateVerts(BVHBuilderTri& tri)
{
	BVHBuilderTri old_tri = tri;
	tri.v[0] = old_tri.v[2];
	tri.v[1] = old_tri.v[0];
	tri.v[2] = old_tri.v[1];
}


static void testSBVHWithNumObsAndMaxDepth(int num_objects, int max_depth, int max_num_objects_per_leaf, bool failure_expected)
{
	glare::TaskManager task_manager;
	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;

	PCG32 rng_(1);
	js::Vector<js::AABBox, 16> aabbs(num_objects);
	js::Vector<BVHBuilderTri, 16> tris(num_objects);
	for(int z=0; z<num_objects; ++z)
	{
		const Vec4f v0(rng_.unitRandom() * 0.8f, rng_.unitRandom() * 0.8f, rng_.unitRandom() * 0.8f, 1);
		const Vec4f v1 = v0 + Vec4f(rng_.unitRandom(), rng_.unitRandom(), rng_.unitRandom(), 0) * 0.02f;
		const Vec4f v2 = v0 + Vec4f(rng_.unitRandom(), rng_.unitRandom(), rng_.unitRandom(), 0) * 0.02f;
		tris[z].v[0] = v0;
		tris[z].v[1] = v1;
		tris[z].v[2] = v2;
		aabbs[z] = js::AABBox::emptyAABBox();
		aabbs[z].enlargeToHoldPoint(v0);
		aabbs[z].enlargeToHoldPoint(v1);
		aabbs[z].enlargeToHoldPoint(v2);
	}

	try
	{
		Timer timer;

		const float intersection_cost = 1.f;
		SBVHBuilder builder(1, max_num_objects_per_leaf, /*max depth=*/max_depth, intersection_cost,
			tris.data(),
			num_objects
		);

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


void SBVHBuilder::test(bool comprehensive_tests)
{
	conPrint("SBVHBuilder::test()");

	PCG32 rng(1);
	glare::TaskManager task_manager;// (1);
	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;


	//==================== Test Builds very close to max depth and num obs constraints ====================
	conPrint("Testing near build failure...");
	testSBVHWithNumObsAndMaxDepth(/*num obs=*/1, /*max depth=*/0, /*max_num_objects_per_leaf=*/1, /*failure_expected=*/false);
	testSBVHWithNumObsAndMaxDepth(/*num obs=*/2, /*max depth=*/1, /*max_num_objects_per_leaf=*/1, /*failure_expected=*/false);
	testSBVHWithNumObsAndMaxDepth(/*num obs=*/4, /*max depth=*/2, /*max_num_objects_per_leaf=*/1, /*failure_expected=*/false);
	testSBVHWithNumObsAndMaxDepth(/*num obs=*/8, /*max depth=*/3, /*max_num_objects_per_leaf=*/1, /*failure_expected=*/false);
	testSBVHWithNumObsAndMaxDepth(/*num obs=*/24, /*max depth=*/3, /*max_num_objects_per_leaf=*/3, /*failure_expected=*/false);

	testSBVHWithNumObsAndMaxDepth(/*num obs=*/48, /*max depth=*/4, /*max_num_objects_per_leaf=*/3, /*failure_expected=*/false);
	testSBVHWithNumObsAndMaxDepth(/*num obs=*/47, /*max depth=*/4, /*max_num_objects_per_leaf=*/3, /*failure_expected=*/false);
	testSBVHWithNumObsAndMaxDepth(/*num obs=*/20, /*max depth=*/4, /*max_num_objects_per_leaf=*/3, /*failure_expected=*/false);

	conPrint("Testing expected build failures...");
	testSBVHWithNumObsAndMaxDepth(/*num obs=*/9,    /*max depth=*/3,  /*max_num_objects_per_leaf=*/1, /*failure_expected=*/true);
	testSBVHWithNumObsAndMaxDepth(/*num obs=*/1025, /*max depth=*/10, /*max_num_objects_per_leaf=*/1, /*failure_expected=*/true);


	//==================== Test building on every igmesh we can find ====================
	if(true)
	{
		Timer timer;
		std::vector<std::string> files = FileUtils::getFilesInDirWithExtensionFullPathsRecursive(TestUtils::getTestReposDir(), "igmesh");
		std::sort(files.begin(), files.end());

		const size_t num_to_test = comprehensive_tests ? files.size() : 40;
		for(size_t i=0; i<num_to_test; ++i)
		{
			Indigo::Mesh mesh;
			try
			{
				//if(i < 1364)
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
				SBVHBuilder builder(1, max_num_objects_per_leaf, /*max depth=*/60, intersection_cost, tris.data(),
					(int)tris.size()
				);
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

	//==================== Perf test ====================
	const bool DO_PERF_TESTS = false;
	if(DO_PERF_TESTS)
	{
		const int num_objects = 100000;

		PCG32 rng_(1);
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		js::Vector<BVHBuilderTri, 16> tris(num_objects);
		for(int z=0; z<num_objects; ++z)
		{
			const Vec4f v0(rng_.unitRandom() * 0.8f, rng_.unitRandom() * 0.8f, 0/*rng_.unitRandom()*/ * 0.8f, 1);
			const Vec4f v1 = v0 + Vec4f(rng_.unitRandom(), rng_.unitRandom(), /*rng_.unitRandom()*/0, 0) * 0.02f;
			const Vec4f v2 = v0 + Vec4f(rng_.unitRandom(), rng_.unitRandom(), /*rng_.unitRandom()*/0, 0) * 0.02f;
			tris[z].v[0] = v0;
			tris[z].v[1] = v1;
			tris[z].v[2] = v2;
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
			//conPrint("------------- perf test --------------");
			Timer timer;

			const int max_num_objects_per_leaf = 16;
			const float intersection_cost = 1.f;

			//------------- SBVHBuilder -----------------
			SBVHBuilder builder(1, max_num_objects_per_leaf, /*max depth=*/60, intersection_cost,
				tris.data(),
				num_objects
			);

			js::Vector<ResultNode, 64> result_nodes;
			builder.build(task_manager,
				should_cancel_callback,
				print_output,
				result_nodes
			);

			const double elapsed = timer.elapsed();
			sum_time += elapsed;
			min_time = myMin(min_time, elapsed);
			conPrint("SBVHBuilder: BVH building for " + toString(num_objects) + " objects took " + toString(elapsed) + " s");

			BVHBuilderTests::testResultsValid(builder.getResultObjectIndices(), result_nodes, aabbs.size(), /*duplicate_prims_allowed=*/true);

			const float SAH_cost = BVHBuilder::getSAHCost(result_nodes, intersection_cost);
			conPrint("SAH_cost: " + toString(SAH_cost));
			//--------------------------------------
		}

		const double av_time = sum_time / NUM_ITERS;
		conPrint("av_time:  " + toString(av_time) + " s");
		conPrint("min_time: " + toString(min_time) + " s");
	}


	//---------------------- Test clipTri() -------------------------
	{
		/*
		tri with one vertex on left side of left plane, two on right
		*/
		SBVHTri tri;
		tri.v[0] = Vec4f(0.5f, 1.f,  0, 1);
		tri.v[1] = Vec4f(1.5f, 1.5f, 0, 1);
		tri.v[2] = Vec4f(1.5f, 2.f,  0, 1);
		const js::AABBox aabb(Vec4f(0, 0, 0, 1), Vec4f(3, 3, 3, 1));

		for(int i=0; i<3; ++i)
		{
			js::AABBox left_clipped_aabb, right_clipped_aabb;
			clipTri(aabb, tri, 0, /*clip coord=*/1.0f, left_clipped_aabb, right_clipped_aabb);
			testEpsEqual(left_clipped_aabb.min_, Vec4f(0.5, 1.f, 0, 1));
			testEpsEqual(left_clipped_aabb.max_, Vec4f(1.0f, 1.5f, 0, 1));
			
			testEpsEqual(right_clipped_aabb.min_, Vec4f(1.f, 1.25f, 0, 1));
			testEpsEqual(right_clipped_aabb.max_, Vec4f(1.5f, 2.0f, 0, 1));
		
			rotateVerts(tri);
		}
	}

	{
		/*
		tri with one vertex on clip plane, two on right
		*/
		SBVHTri tri;
		tri.v[0] = Vec4f(1.0f, 1.f, 0, 1);
		tri.v[1] = Vec4f(1.5f, 1.5f, 0, 1);
		tri.v[2] = Vec4f(1.5f, 2.f, 0, 1);
		const js::AABBox aabb(Vec4f(0, 0, 0, 1), Vec4f(3, 3, 3, 1));
		for(int i=0; i<3; ++i)
		{
			js::AABBox left_clipped_aabb, right_clipped_aabb;
			clipTri(aabb, tri, 0, /*clip coord=*/1.0f, left_clipped_aabb, right_clipped_aabb);
			testEpsEqual(left_clipped_aabb.min_, Vec4f(1.f, 1.f, 0, 1));
			testEpsEqual(left_clipped_aabb.max_, Vec4f(1.0f, 1.f, 0, 1));

			testEpsEqual(right_clipped_aabb.min_, Vec4f(1.f, 1.0f, 0, 1));
			testEpsEqual(right_clipped_aabb.max_, Vec4f(1.5f, 2.0f, 0, 1));

			rotateVerts(tri);
		}
	}
	{
		/*
		tri with one vertex on clip plane, two on left
		*/
		SBVHTri tri;
		tri.v[0] = Vec4f(1.0f, 1.f, 0, 1);
		tri.v[1] = Vec4f(0.5f, 1.5f, 0, 1);
		tri.v[2] = Vec4f(0.5f, 2.f, 0, 1);
		const js::AABBox aabb(Vec4f(0, 0, 0, 1), Vec4f(3, 3, 3, 1));
		for(int i=0; i<3; ++i)
		{
			js::AABBox left_clipped_aabb, right_clipped_aabb;
			clipTri(aabb, tri, 0, /*clip coord=*/1.0f, left_clipped_aabb, right_clipped_aabb);
			testEpsEqual(left_clipped_aabb.min_, Vec4f(0.5f, 1.f, 0, 1));
			testEpsEqual(left_clipped_aabb.max_, Vec4f(1.0f, 2.f, 0, 1));

			testEpsEqual(right_clipped_aabb.min_, Vec4f(1.f, 1.0f, 0, 1));
			testEpsEqual(right_clipped_aabb.max_, Vec4f(1.0f, 1.0f, 0, 1));

			rotateVerts(tri);
		}
	}

	{
		/*
		tri with one vertex on clip plane, one on left, one on right
		*/
		SBVHTri tri;
		tri.v[0] = Vec4f(0.0f, 1.0f, 0, 1);
		tri.v[1] = Vec4f(1.0f, 0.5f, 0, 1);
		tri.v[2] = Vec4f(2.0f, 2.0f, 0, 1);
		const js::AABBox aabb(Vec4f(0, 0, 0, 1), Vec4f(3, 3, 3, 1));
		for(int i=0; i<3; ++i)
		{
			js::AABBox left_clipped_aabb, right_clipped_aabb;
			clipTri(aabb, tri, 0, /*clip coord=*/1.0f, left_clipped_aabb, right_clipped_aabb);
			testEpsEqual(left_clipped_aabb.min_, Vec4f(0.0f, 0.5f, 0, 1));
			testEpsEqual(left_clipped_aabb.max_, Vec4f(1.0f, 1.5f, 0, 1));

			testEpsEqual(right_clipped_aabb.min_, Vec4f(1.0f, 0.5f, 0, 1));
			testEpsEqual(right_clipped_aabb.max_, Vec4f(2.0f, 2.0f, 0, 1));

			rotateVerts(tri);
		}
	}

	{
		/*
		tri with two vertices on clip plane, one to right.
		*/
		SBVHTri tri;
		tri.v[0] = Vec4f(1.0f, 0.5f, 0, 1);
		tri.v[1] = Vec4f(1.0f, 2.0f, 0, 1);
		tri.v[2] = Vec4f(2.0f, 1.0f, 0, 1);
		const js::AABBox aabb(Vec4f(0, 0, 0, 1), Vec4f(3, 3, 3, 1));
		for(int i=0; i<3; ++i)
		{
			js::AABBox left_clipped_aabb, right_clipped_aabb;
			clipTri(aabb, tri, 0, /*clip coord=*/1.0f, left_clipped_aabb, right_clipped_aabb);
			testEpsEqual(left_clipped_aabb.min_, Vec4f(1.0f, 0.5f, 0, 1));
			testEpsEqual(left_clipped_aabb.max_, Vec4f(1.0f, 2.0f, 0, 1));

			testEpsEqual(right_clipped_aabb.min_, Vec4f(1.0f, 0.5f, 0, 1));
			testEpsEqual(right_clipped_aabb.max_, Vec4f(2.0f, 2.0f, 0, 1));

			rotateVerts(tri);
		}
	}

	{
		/*
		tri with three vertices on clip plane.
		*/
		SBVHTri tri;
		tri.v[0] = Vec4f(1.0f, 0.5f, 0, 1);
		tri.v[1] = Vec4f(1.0f, 2.0f, 0, 1);
		tri.v[2] = Vec4f(1.0f, 1.0f, 0, 1);
		const js::AABBox aabb(Vec4f(0, 0, 0, 1), Vec4f(3, 3, 3, 1));
		for(int i=0; i<3; ++i)
		{
			js::AABBox left_clipped_aabb, right_clipped_aabb;
			clipTri(aabb, tri, 0, /*clip coord=*/1.0f, left_clipped_aabb, right_clipped_aabb);
			testEpsEqual(left_clipped_aabb.min_, Vec4f(1.0f, 0.5f, 0, 1));
			testEpsEqual(left_clipped_aabb.max_, Vec4f(1.0f, 2.0f, 0, 1));

			testEpsEqual(right_clipped_aabb.min_, Vec4f(1.0f, 0.5f, 0, 1));
			testEpsEqual(right_clipped_aabb.max_, Vec4f(1.0f, 2.0f, 0, 1));

			rotateVerts(tri);
		}
	}

	{
		/*
		tri with all vertices to left
		*/
		SBVHTri tri;
		tri.v[0] = Vec4f(0.5f, 1.f, 0, 1);
		tri.v[1] = Vec4f(1.5f, 1.5f, 0, 1);
		tri.v[2] = Vec4f(1.5f, 2.f, 0, 1);
		const js::AABBox aabb(Vec4f(0, 0, 0, 1), Vec4f(3, 3, 3, 1));

		for(int i=0; i<3; ++i)
		{
			js::AABBox left_clipped_aabb, right_clipped_aabb;
			clipTri(aabb, tri, 0, /*clip coord=*/10.0f, left_clipped_aabb, right_clipped_aabb);
			testEpsEqual(left_clipped_aabb.min_, Vec4f(0.5, 1.f, 0, 1));
			testEpsEqual(left_clipped_aabb.max_, Vec4f(1.5f, 2.0f, 0, 1));

			testAssert(right_clipped_aabb == empty_aabb);

			rotateVerts(tri);
		}
	}

	{
		/*
		tri with all vertices to right
		*/
		SBVHTri tri;
		tri.v[0] = Vec4f(0.5f, 1.f, 0, 1);
		tri.v[1] = Vec4f(1.5f, 1.5f, 0, 1);
		tri.v[2] = Vec4f(1.5f, 2.f, 0, 1);
		const js::AABBox aabb(Vec4f(0, 0, 0, 1), Vec4f(3, 3, 3, 1));

		for(int i=0; i<3; ++i)
		{
			js::AABBox left_clipped_aabb, right_clipped_aabb;
			clipTri(aabb, tri, 0, /*clip coord=*/0.2f, left_clipped_aabb, right_clipped_aabb);
			testAssert(left_clipped_aabb == empty_aabb);
			testEpsEqual(right_clipped_aabb.min_, Vec4f(0.5, 1.f, 0, 1));
			testEpsEqual(right_clipped_aabb.max_, Vec4f(1.5f, 2.0f, 0, 1));

			rotateVerts(tri);
		}
	}

	conPrint("SBVHBuilder::test() done");
}


#endif // BUILD_TESTS
