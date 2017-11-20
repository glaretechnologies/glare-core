/*=====================================================================
SBVHBuilder.cpp
-------------------
Copyright Glare Technologies Limited 2017 -
=====================================================================*/
#include "SBVHBuilder.h"


#include "jscol_aabbox.h"
#include "../graphics/bitmap.h"
#include "../graphics/Drawing.h"
#include "../graphics/PNGDecoder.h"
#include "../utils/Exception.h"
#include "../utils/Sort.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/TaskManager.h"
#include "../utils/Lock.h"
#include "../utils/Timer.h"
#include "../utils/ProfilerStore.h"
#include <algorithm>


//#define ALLOW_DEBUG_DRAWING 1
const bool DEBUG_DRAW = false;
static const js::AABBox empty_aabb = js::AABBox::emptyAABBox();


static const Vec2f lower(0, 0);
static const float draw_scale = 1;
static const int map_res = 1000;
#if ALLOW_DEBUG_DRAWING
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


SBVHBuilder::SBVHBuilder(int leaf_num_object_threshold_, int max_num_objects_per_leaf_, float intersection_cost_)
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


SBVHBuilder::~SBVHBuilder()
{
}


SBVHBuildStats::SBVHBuildStats()
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
	num_object_splits = 0;
	num_spatial_splits = 0;
}


void SBVHBuildStats::accumStats(const SBVHBuildStats& other)
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
	num_object_splits				+= other.num_object_splits;
	num_spatial_splits				+= other.num_spatial_splits;
}


/*
Builds a subtree with the objects[begin] ... objects[end-1]
Writes its results to the per-thread buffer 'per_thread_temp_info[thread_index].result_buf', and describes the result in result_chunk,
which is added to the global result_chunks list.

May spawn new BuildSubtreeTasks.
*/
class SBVHBuildSubtreeTask : public Indigo::Task
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	SBVHBuildSubtreeTask(SBVHBuilder& builder_) : builder(builder_) {}
	
	virtual void run(size_t thread_index)
	{
		ScopeProfiler _scope("SBVHBuildSubtreeTask", (int)thread_index);

		// Create subtree root node
		const int root_node_index = (int)builder.per_thread_temp_info[thread_index].result_buf.size();
		builder.per_thread_temp_info[thread_index].result_buf.push_back_uninitialised();
		builder.per_thread_temp_info[thread_index].result_buf[root_node_index].right_child_chunk_index = -666;

		builder.doBuild(
			builder.per_thread_temp_info[thread_index],
			node_aabb,
			centroid_aabb,
			root_node_index, // node index
			objects,
			depth, sort_key,
			*result_chunk
		);

		result_chunk->thread_index = (int)thread_index;
		result_chunk->offset = root_node_index;
		result_chunk->size = (int)builder.per_thread_temp_info[thread_index].result_buf.size() - root_node_index;
		result_chunk->sort_key = this->sort_key;
	}

	SBVHBuilder& builder;
	SBVHResultChunk* result_chunk;
	std::vector<SBVHOb> objects;
	js::AABBox node_aabb;
	js::AABBox centroid_aabb;
	int depth;
	uint64 sort_key;
};


struct SBVHResultChunkPred
{
	bool operator () (const SBVHResultChunk& a, const SBVHResultChunk& b)
	{
		return a.sort_key < b.sort_key;
	}
};


// Top-level build method
void SBVHBuilder::build(
		Indigo::TaskManager& task_manager_,
		const js::AABBox* aabbs_,
		const SBVHTri* triangles_,
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
	this->triangles = triangles_;

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

	// Build overall AABB
	root_aabb = aabbs[0];
	centroid_aabb = js::AABBox(aabbs[0].centroid(), aabbs[0].centroid());
	for(size_t i = 0; i < num_objects; ++i)
	{
		root_aabb.enlargeToHoldAABBox(aabbs[i]);
		centroid_aabb.enlargeToHoldPoint(aabbs[i].centroid());
	}

	this->recip_root_node_aabb_area = 1 / root_aabb.getSurfaceArea();

	// Alloc space for objects for each axis
	this->top_level_objects.resize(num_objects);
	for(size_t i = 0; i < num_objects; ++i)
	{
		top_level_objects[i].aabb = aabbs[i];
		top_level_objects[i].setIndex((int)i);
	}
	
	
	// Reserve working space for each thread.
	const int initial_result_buf_reserve_cap = 2 * num_objects / (int)task_manager->getNumThreads();
	per_thread_temp_info.resize(task_manager->getNumThreads());
	for(size_t i=0; i<per_thread_temp_info.size(); ++i)
	{
		per_thread_temp_info[i].left_obs.resize(62);
		per_thread_temp_info[i].right_obs.resize(62);

		per_thread_temp_info[i].result_buf.reserve(initial_result_buf_reserve_cap);
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

	Reference<SBVHBuildSubtreeTask> task = new SBVHBuildSubtreeTask(*this);
	task->objects = this->top_level_objects;
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

	result_indices.reserve(num_objects);

	result_nodes_out.resizeNoCopy(total_num_nodes);
	int write_index = 0;

	for(size_t c=0; c<num_result_chunks; ++c)
	{
		const SBVHResultChunk& chunk = result_chunks[c];
		const int chunk_node_offset = final_chunk_offset[c] - chunk.offset; // delta for references internal to chunk.
		const int chunk_leaf_geom_offset = (int)result_indices.size();

		const js::Vector<ResultNode, 64>& chunk_nodes = per_thread_temp_info[chunk.thread_index].result_buf; // Get the per-thread result node buffer that this chunk is in.
		const std::vector<int>& leaf_geom             = per_thread_temp_info[chunk.thread_index].leaf_obs; // Get the per-thread leaf geometry

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
			else // Else if chunk is leaf:
			{
				const int new_left = (int)result_indices.size();
				const int new_right = new_left + (chunk_node.right - chunk_node.left);
				// Append leaf primitive indices to result_indices
				for(int z=chunk_node.left; z<chunk_node.right; ++z)
				{
					const int ob_i = leaf_geom[z];
					assert(ob_i >= 0 && ob_i < num_objects);
					result_indices.push_back(ob_i);
				}

				// Update leaf node left and right
				chunk_node.left = new_left;
				chunk_node.right = new_right;
			}

			result_nodes_out[write_index] = chunk_node; // Copy node to final array
			write_index++;
		}
	}

	//conPrint("Final merge elapsed: " + timer.elapsedString());

	// Combine stats from each thread
	for(size_t i=0; i<per_thread_temp_info.size(); ++i)
		this->stats.accumStats(per_thread_temp_info[i].stats);

	// Dump some mem usage stats
	if(true)
	{
		const double build_time = build_timer.elapsed();

		conPrint("----------- SBVHBuilder Build Stats ----------");
		conPrint("Mem usage:");
		conPrint("objects:                    " + getNiceByteSize(sizeof(SBVHOb) * top_level_objects.size()));

		size_t total_per_thread_size = 0;
		for(size_t i=0; i<per_thread_temp_info.size(); ++i)
			total_per_thread_size += per_thread_temp_info[i].dataSizeBytes();
		conPrint("per_thread_temp_info:       " + getNiceByteSize(total_per_thread_size));

		conPrint("result_chunks:              " + getNiceByteSize(result_chunks.dataSizeBytes()));
		conPrint("result_nodes_out:           " + toString(result_nodes_out.size()) + " nodes * " + toString(sizeof(ResultNode)) + "B = " + getNiceByteSize(result_nodes_out.dataSizeBytes()));
		
		const size_t total_size = (sizeof(SBVHOb) * top_level_objects.size()) + total_per_thread_size + result_chunks.dataSizeBytes() + result_nodes_out.dataSizeBytes();
		
		conPrint("total:                      " + getNiceByteSize(total_size));
		conPrint("");

		//conPrint("split_search_time: " + toString(split_search_time) + " s");
		//conPrint("partition_time:    " + toString(partition_time) + " s");

		conPrint("Num triangles:              " + toString(top_level_objects.size()));
		conPrint("num interior nodes:         " + toString(stats.num_interior_nodes));
		conPrint("num leaves:                 " + toString(stats.num_leaves));
		conPrint("num maxdepth leaves:        " + toString(stats.num_maxdepth_leaves));
		conPrint("num under_thresh leaves:    " + toString(stats.num_under_thresh_leaves));
		conPrint("num cheaper nosplit leaves: " + toString(stats.num_cheaper_nosplit_leaves));
		conPrint("num could not split leaves: " + toString(stats.num_could_not_split_leaves));
		conPrint("num arbitrary split leaves: " + toString(stats.num_arbitrary_split_leaves));
		conPrint("av num tris per leaf:       " + toString((float)top_level_objects.size() / stats.num_leaves));
		conPrint("max num tris per leaf:      " + toString(stats.max_num_tris_per_leaf));
		conPrint("av leaf depth:              " + toString((float)stats.leaf_depth_sum / stats.num_leaves));
		conPrint("max leaf depth:             " + toString(stats.max_leaf_depth));
		conPrint("num leaf primitives:        " + toString(result_indices.size()));
		conPrint("num object splits:          " + toString(stats.num_object_splits));
		conPrint("num spatial splits:         " + toString(stats.num_spatial_splits));
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
	js::AABBox left_aabb, left_centroid_aabb;
	js::AABBox right_aabb, right_centroid_aabb;
};


static void partition(const std::vector<SBVHOb>& objects_, const SBVHTri* triangles, const js::AABBox& parent_aabb, float best_div_val, int best_axis, bool best_split_was_spatial, 
	const std::vector<int>& unsplit,
	PartitionRes& res_out,
	std::vector<SBVHOb>& left_obs_out,
	std::vector<SBVHOb>& right_obs_out
	)
{
	left_obs_out.resize(0);
	right_obs_out.resize(0);

	const SBVHOb* const objects = objects_.data();
	const int num_objects = (int)objects_.size();

	js::AABBox left_aabb = empty_aabb;
	js::AABBox left_centroid_aabb = empty_aabb;
	js::AABBox right_aabb = empty_aabb;
	js::AABBox right_centroid_aabb = empty_aabb;

	if(best_split_was_spatial)
	{
		for(int cur = 0; cur < num_objects; ++cur)
		{
			const int ob_i = objects[cur].getIndex();
			const SBVHTri& tri = triangles[ob_i];
			const js::AABBox aabb = objects[cur].aabb;
			assert(parent_aabb.containsAABBox(aabb));
			if(aabb.min_[best_axis] < best_div_val && aabb.max_[best_axis] > best_div_val) // If triangle straddles split plane:
			{
				if(unsplit[cur] == 1)
				{
					left_obs_out.push_back(objects[cur]);
					left_aabb.enlargeToHoldAABBox(aabb);
					left_centroid_aabb.enlargeToHoldPoint(aabb.centroid());
				}
				else if(unsplit[cur] == 2)
				{
					right_obs_out.push_back(objects[cur]);
					right_aabb.enlargeToHoldAABBox(aabb);
					right_centroid_aabb.enlargeToHoldPoint(aabb.centroid());
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

						if(v[best_axis] <= best_div_val) tri_left_aabb .enlargeToHoldPoint(v);
						if(v[best_axis] >= best_div_val) tri_right_aabb.enlargeToHoldPoint(v);
					}

					// Make sure AABBs of clipped triangle don't extend past the bounds of the triangle clipped to the current AABB.
					tri_left_aabb  = intersection(tri_left_aabb,  aabb);
					tri_right_aabb = intersection(tri_right_aabb, aabb);

					// Check tri_left_aabb, tri_right_aabb
					assert(parent_aabb.containsAABBox(tri_left_aabb));
					assert(parent_aabb.containsAABBox(tri_right_aabb));
					assert(tri_left_aabb .max_[best_axis] <= best_div_val + 1.0e-5f);
					assert(tri_right_aabb.min_[best_axis] >= best_div_val - 1.0e-5f);

					SBVHOb left_ob;
					left_ob.aabb = tri_left_aabb;
					left_ob.setIndex(ob_i);
					left_obs_out.push_back(left_ob);

					SBVHOb right_ob;
					right_ob.aabb = tri_right_aabb;
					right_ob.setIndex(ob_i);
					right_obs_out.push_back(right_ob);

					left_centroid_aabb .enlargeToHoldPoint(tri_left_aabb.centroid()); // Get centroid of the part of the triangle clipped to left child volume, add to left centroid aabb.
					right_centroid_aabb.enlargeToHoldPoint(tri_right_aabb.centroid());

					left_aabb .enlargeToHoldAABBox(tri_left_aabb);
					right_aabb.enlargeToHoldAABBox(tri_right_aabb);
				}
			}
			else // Else if triangle does not straddle clip plane:  then triangle bounds are completely in left or right side. Send triangle left or right based on triangle centroid.
			{
				const Vec4f centroid = aabb.centroid();
				if(centroid[best_axis] <= best_div_val) // If object should go on Left side:
				{
					left_obs_out.push_back(objects[cur]);
					left_aabb.enlargeToHoldAABBox(aabb);
					left_centroid_aabb.enlargeToHoldPoint(centroid);
				}
				else // else if cur object should go on Right side:
				{
					right_obs_out.push_back(objects[cur]);
					right_aabb.enlargeToHoldAABBox(aabb);
					right_centroid_aabb.enlargeToHoldPoint(centroid);
				}
			}
		}
	}
	else // Else if best split was just an object split:
	{
		for(int cur = 0; cur < num_objects; ++cur)
		{
			const js::AABBox aabb = objects[cur].aabb;
			const Vec4f centroid = aabb.centroid();
			if(centroid[best_axis] <= best_div_val) // If object should go on Left side:
			{
				left_obs_out.push_back(objects[cur]);
				left_aabb.enlargeToHoldAABBox(aabb);
				left_centroid_aabb.enlargeToHoldPoint(centroid);
			}
			else // else if cur object should go on Right side:
			{
				right_obs_out.push_back(objects[cur]);
				right_aabb.enlargeToHoldAABBox(aabb);
				right_centroid_aabb.enlargeToHoldPoint(centroid);
			}
		}
	}

	res_out.left_aabb = left_aabb;
	res_out.left_centroid_aabb = left_centroid_aabb;
	res_out.right_aabb = right_aabb;
	res_out.right_centroid_aabb = right_centroid_aabb;
}


static void tentativeSpatialPartition(const std::vector<SBVHOb>& objects_, const SBVHTri* triangles, const js::AABBox& parent_aabb, int left, int right, 
	float best_div_val, int best_axis, const std::vector<int>* unsplits,  PartitionRes& res_out, int& num_left_out, int& num_right_out)
{
	num_left_out = num_right_out = 0;

	const SBVHOb* const objects = objects_.data();

	js::AABBox left_aabb = empty_aabb;
	js::AABBox right_aabb = empty_aabb;

	for(int cur = left; cur < right; ++cur)
	{
		const int ob_i = objects[cur].getIndex();
		const SBVHTri& tri = triangles[ob_i];
		const js::AABBox aabb = objects[cur].aabb;
		assert(parent_aabb.containsAABBox(aabb));
		if(aabb.min_[best_axis] < best_div_val && aabb.max_[best_axis] > best_div_val) // If triangle straddles split plane:
		{
			const int unsplit_tri = unsplits ? ((*unsplits)[cur]) : 0;
			if(unsplit_tri == 1)
			{
				left_aabb.enlargeToHoldAABBox(aabb);
				num_left_out++;
			}
			else if(unsplit_tri == 2)
			{
				right_aabb.enlargeToHoldAABBox(aabb);
				num_right_out++;
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
				assert(tri_left_aabb.max_[best_axis] <= best_div_val + 1.0e-5f);
				assert(tri_right_aabb.min_[best_axis] >= best_div_val - 1.0e-5f);

				left_aabb.enlargeToHoldAABBox(tri_left_aabb);
				right_aabb.enlargeToHoldAABBox(tri_right_aabb);

				num_left_out++;
				num_right_out++;
			}
		}
		else // Else if triangle does not straddle clip plane:  then triangle bounds are completely in left or right side. Send triangle left or right based on triangle centroid.
		{
			const Vec4f centroid = aabb.centroid();
			if(centroid[best_axis] <= best_div_val) // If object should go on Left side:
			{
				left_aabb.enlargeToHoldAABBox(aabb);
				num_left_out++;
			}
			else // else if cur object should go on Right side:
			{
				right_aabb.enlargeToHoldAABBox(aabb);
				num_right_out++;
			}
		}
	}

	res_out.left_aabb  = left_aabb;
	res_out.right_aabb = right_aabb;
}


// Since we store the object index in the AABB.min_.w, we want to restore this value to 1 before we return it to the client code.
static inline void setAABBWToOne(js::AABBox& aabb)
{
	aabb.min_ = select(Vec4f(1.f), aabb.min_, bitcastToVec4f(Vec4i(0, 0, 0, 0xFFFFFFFF)));
	aabb.max_ = select(Vec4f(1.f), aabb.max_, bitcastToVec4f(Vec4i(0, 0, 0, 0xFFFFFFFF)));
}


static void getTriClippedtoBucket(const js::AABBox& ob_aabb, const SBVHTri& tri, int axis, float left, float right, js::AABBox& clipped_aabb_out)
{
	clipped_aabb_out = empty_aabb;

	const Vec4f v0 = tri.v[0];
	const Vec4f v1 = tri.v[1];
	const Vec4f v2 = tri.v[2];

	// For each vertex, if it lies between the clip planes, enlarge bounding box to contain it.
	if(v0[axis] >= left && v0[axis] <= right)
		clipped_aabb_out.enlargeToHoldPoint(v0);

	if(v1[axis] >= left && v1[axis] <= right)
		clipped_aabb_out.enlargeToHoldPoint(v1);

	if(v2[axis] >= left && v2[axis] <= right)
		clipped_aabb_out.enlargeToHoldPoint(v2);

	// Clip against left plane
	if((v0[axis] < left && v1[axis] > left) || (v0[axis] > left && v1[axis] < left)) // If edge 0-1 straddles left clip plane:
	{
		const float t = (left - v0[axis]) / (v1[axis] - v0[axis]); // Solve for fraction along edge of position on split plane.
		const Vec4f p = v0 * (1 - t) + v1 * t;
		clipped_aabb_out.enlargeToHoldPoint(p);
	}

	if((v1[axis] < left && v2[axis] > left) || (v1[axis] > left && v2[axis] < left)) // If edge 1-2 straddles left clip plane:
	{
		const float t = (left - v1[axis]) / (v2[axis] - v1[axis]); // Solve for fraction along edge of position on split plane.
		const Vec4f p = v1 * (1 - t) + v2 * t;
		clipped_aabb_out.enlargeToHoldPoint(p);
	}

	if((v2[axis] < left && v0[axis] > left) || (v2[axis] > left && v0[axis] < left)) // If edge 2-0 straddles left clip plane:
	{
		const float t = (left - v2[axis]) / (v0[axis] - v2[axis]); // Solve for fraction along edge of position on split plane.
		const Vec4f p = v2 * (1 - t) + v0 * t;
		clipped_aabb_out.enlargeToHoldPoint(p);
	}

	// Clip against right plane
	if((v0[axis] < right && v1[axis] > right) || (v0[axis] > right && v1[axis] < right)) // If edge 0-1 straddles right clip plane:
	{
		const float t = (right - v0[axis]) / (v1[axis] - v0[axis]); // Solve for fraction along edge of position on split plane.
		const Vec4f p = v0 * (1 - t) + v1 * t;
		clipped_aabb_out.enlargeToHoldPoint(p);
	}

	if((v1[axis] < right && v2[axis] > right) || (v1[axis] > right && v2[axis] < right)) // If edge 1-2 straddles right clip plane:
	{
		const float t = (right - v1[axis]) / (v2[axis] - v1[axis]); // Solve for fraction along edge of position on split plane.
		const Vec4f p = v1 * (1 - t) + v2 * t;
		clipped_aabb_out.enlargeToHoldPoint(p);
	}

	if((v2[axis] < right && v0[axis] > right) || (v2[axis] > right && v0[axis] < right)) // If edge 2-0 straddles right clip plane:
	{
		const float t = (right - v2[axis]) / (v0[axis] - v2[axis]); // Solve for fraction along edge of position on split plane.
		const Vec4f p = v2 * (1 - t) + v0 * t;
		clipped_aabb_out.enlargeToHoldPoint(p);
	}

	clipped_aabb_out = intersection(ob_aabb, clipped_aabb_out);

	assert(ob_aabb.containsAABBox(clipped_aabb_out));
	assert(clipped_aabb_out.min_[axis] >= left  - 1.0e-5f);
	assert(clipped_aabb_out.max_[axis] <= right + 1.0e-5f);
}


static void search(const js::AABBox& aabb, const js::AABBox& centroid_aabb_, const std::vector<SBVHOb>& objects_, const SBVHTri* triangles, int left, int right, float recip_root_node_aabb_area, 
	int& best_axis_out, float& best_div_val_out,
	float& smallest_split_cost_factor_out, bool& best_split_is_spatial_out, std::vector<int>& unsplit_out)
{
	const js::AABBox& centroid_aabb = centroid_aabb_;
	const SBVHOb* const objects = objects_.data();

	const Vec4f aabb_span = aabb.max_ - aabb.min_;

	float smallest_split_cost_factor = std::numeric_limits<float>::infinity(); // Smallest (N_L * half_left_surface_area + N_R * half_right_surface_area) found.
	int best_axis = -1;
	float best_div_val = 0;
	bool best_split_is_spatial = false;

	
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

		assert(aabb.containsAABBox(right_aabb[0]));
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

		assert(aabb.containsAABBox(left_aabb[0]));

		float A0 = left_aabb[0].getHalfSurfaceArea();
		float A1 = left_aabb[1].getHalfSurfaceArea();
		float A2 = left_aabb[2].getHalfSurfaceArea();

		const Vec4f cost = mul(toVec4f(count), Vec4f(A0, A1, A2, A2)) + mul(toVec4f(right_prefix_counts[b]), right_area[b]); // Compute SAH cost factor

		if(smallest_split_cost_factor > elem<0>(cost))
		{
			smallest_split_cost_factor = elem<0>(cost);
			best_axis = 0;
			best_div_val = centroid_aabb.min_[0] + axis_len_over_num_buckets[0] * (b + 1);
		}

		if(smallest_split_cost_factor > elem<1>(cost))
		{
			smallest_split_cost_factor = elem<1>(cost);
			best_axis = 1;
			best_div_val = centroid_aabb.min_[1] + axis_len_over_num_buckets[1] * (b + 1);
		}

		if(smallest_split_cost_factor > elem<2>(cost))
		{
			smallest_split_cost_factor = elem<2>(cost);
			best_axis = 2;
			best_div_val = centroid_aabb.min_[2] + axis_len_over_num_buckets[2] * (b + 1);
		}
	}


	//===========================================================================================================
	// Look for spatial splits - splitting planes where triangles intersecting the plane are clipped into two polygons, one going to left child and one going to the right child

	if(best_axis == -1)
	{
		best_axis_out = best_axis;
		best_div_val_out = best_div_val;
		smallest_split_cost_factor_out = smallest_split_cost_factor;
		best_split_is_spatial_out = best_split_is_spatial;
		return;
	}

	// Compute lambda - surface area of intersection of left and right child AABBs.
	js::AABBox best_left_aabb = empty_aabb;
	js::AABBox best_right_aabb = empty_aabb;
	for(int i=left; i<right; ++i)
	{
		const js::AABBox ob_aabb = objects[i].aabb;
		const Vec4f centroid = ob_aabb.centroid();
		if(centroid[best_axis] <= best_div_val) // If object should go on Left side:
			best_left_aabb.enlargeToHoldAABBox(ob_aabb);
		else // else if cur object should go on Right side:
			best_right_aabb.enlargeToHoldAABBox(ob_aabb);
	}

	setAABBWToOne(best_left_aabb);
	setAABBWToOne(best_right_aabb);

	int spatial_best_axis = -1;
	float spatial_best_div_val = -666;
	float spatial_smallest_split_cost_factor = std::numeric_limits<float>::infinity();

	const js::AABBox inter = intersection(best_left_aabb, best_right_aabb);
	const float lambda = inter.isEmpty() ? 0 : inter.getSurfaceArea();

	const float lambda_over_sa_root_bound = lambda * recip_root_node_aabb_area;
	const float alpha = 1.0e-5f;
	if(lambda_over_sa_root_bound > alpha)
	{
		const int num_spatial_buckets = 32;
		assert(num_spatial_buckets <= max_B);
		
		Vec4i entry_counts[max_B];
		Vec4i exit_counts[max_B];
		for(int i=0; i<num_spatial_buckets; ++i)
			for(int z=0; z<3; ++z)
			{
				bucket_aabbs[z*max_B + i] = empty_aabb;
				entry_counts[i] = Vec4i(0);
				exit_counts[i] = Vec4i(0);
			}

		const Vec4f spatial_scale = div(Vec4f((float)num_spatial_buckets), (aabb.max_ - aabb.min_));
		const Vec4f spatial_axis_len_over_num_buckets = div(aabb.max_ - aabb.min_, Vec4f((float)num_spatial_buckets));

		for(int i=left; i<right; ++i)
		{
			const int ob_i = objects[i].getIndex();
			const js::AABBox ob_aabb = objects[i].aabb;
			assert(aabb.containsAABBox(ob_aabb));
			const SBVHTri& tri = triangles[ob_i];

			const Vec4i entry_bucket_i = clamp(truncateToVec4i(mul((ob_aabb.min_ - aabb.min_), spatial_scale)), Vec4i(0), Vec4i(num_spatial_buckets-1));
			const Vec4i exit_bucket_i  = clamp(truncateToVec4i(mul((ob_aabb.max_ - aabb.min_), spatial_scale)), Vec4i(0), Vec4i(num_spatial_buckets-1));

			assert(entry_bucket_i[0] >= 0 && entry_bucket_i[0] < num_spatial_buckets);
			assert(entry_bucket_i[1] >= 0 && entry_bucket_i[1] < num_spatial_buckets);
			assert(entry_bucket_i[2] >= 0 && entry_bucket_i[2] < num_spatial_buckets);
			assert(exit_bucket_i[0] >= 0 && exit_bucket_i[0] < num_spatial_buckets);
			assert(exit_bucket_i[1] >= 0 && exit_bucket_i[1] < num_spatial_buckets);
			assert(exit_bucket_i[2] >= 0 && exit_bucket_i[2] < num_spatial_buckets);

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
						for(int b=entry_b; b<=exit_b; ++b)
						{
							js::AABBox clipped_aabb;
							getTriClippedtoBucket(ob_aabb, tri, axis, aabb.min_[axis] + spatial_axis_len_over_num_buckets[axis] * b, aabb.min_[axis] + spatial_axis_len_over_num_buckets[axis] * (b + 1), clipped_aabb);
							bucket_aabbs[b + axis*max_B].enlargeToHoldAABBox(clipped_aabb);
						}
					}

					(entry_counts[entry_b])[axis]++;
					(exit_counts[exit_b])  [axis]++;
				}
			}
		}

		// Sweep right to left, computing exclusive prefix surface areas and counts
		// right_area[b] = sum_{i=(b+1)}^{num_buckets} bucket_aabb[b].getHalfSurfaceArea()
		count = Vec4i(0);
		for(int i=0; i<3; ++i)
			right_aabb[i] = empty_aabb;

		for(int b=num_spatial_buckets-1; b>=0; --b)
		{
			right_prefix_counts[b] = count;
			count = count + exit_counts[b];

			float A0 = right_aabb[0].getHalfSurfaceArea();
			float A1 = right_aabb[1].getHalfSurfaceArea();
			float A2 = right_aabb[2].getHalfSurfaceArea();
			right_area[b] = Vec4f(A0, A1, A2, A2);

			right_aabb[0].enlargeToHoldAABBox(bucket_aabbs[b]);
			right_aabb[1].enlargeToHoldAABBox(bucket_aabbs[b +   max_B]);
			right_aabb[2].enlargeToHoldAABBox(bucket_aabbs[b + 2*max_B]);

			assert(aabb.containsAABBox(right_aabb[0]));
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

			assert(aabb.containsAABBox(left_aabb[0]));

			float A0 = left_aabb[0].getHalfSurfaceArea();
			float A1 = left_aabb[1].getHalfSurfaceArea();
			float A2 = left_aabb[2].getHalfSurfaceArea();

			const Vec4f cost = mul(toVec4f(count), Vec4f(A0, A1, A2, A2)) + mul(toVec4f(right_prefix_counts[b]), right_area[b]); // Compute SAH cost factor

			if(aabb_span[0] > 0 && spatial_smallest_split_cost_factor > elem<0>(cost))
			{
				spatial_smallest_split_cost_factor = elem<0>(cost);
				spatial_best_axis = 0;
				spatial_best_div_val = aabb.min_[0] + spatial_axis_len_over_num_buckets[0] * (b + 1);
			}

			if(aabb_span[1] > 0 && spatial_smallest_split_cost_factor > elem<1>(cost))
			{
				spatial_smallest_split_cost_factor = elem<1>(cost);
				spatial_best_axis = 1;
				spatial_best_div_val = aabb.min_[1] + spatial_axis_len_over_num_buckets[1] * (b + 1);
			}

			if(aabb_span[2] > 0 && spatial_smallest_split_cost_factor > elem<2>(cost))
			{
				spatial_smallest_split_cost_factor = elem<2>(cost);
				spatial_best_axis = 2;
				spatial_best_div_val = aabb.min_[2] + spatial_axis_len_over_num_buckets[2] * (b + 1);
			}
		}
	}

	// Do reference unsplitting at the best spatial split found.
	const bool DO_UNSPLITTING = true;
	if(DO_UNSPLITTING)
	{
		if(spatial_smallest_split_cost_factor < std::numeric_limits<float>::infinity())
		{
			unsplit_out.resize(right - left);

			PartitionRes res;
			int num_left, num_right;
			tentativeSpatialPartition(objects_, triangles, aabb, left, right, spatial_best_div_val, spatial_best_axis, /*unsplits=*/NULL, res, num_left, num_right);

			const float left_half_area  = res.left_aabb. getHalfSurfaceArea();
			const float right_half_area = res.right_aabb.getHalfSurfaceArea();
			const float C_split = left_half_area * num_left + right_half_area * num_right;
			assert(epsEqual(C_split, spatial_smallest_split_cost_factor));

			const float left_reduced_cost  = res.left_aabb.getHalfSurfaceArea()  * (num_left - 1);
			const float right_reduced_cost = res.right_aabb.getHalfSurfaceArea() * (num_right - 1);
			int num_unsplit = 0;
			for(int i=left; i<right; ++i)
			{
				const int ob_i = objects[i].getIndex();
				const js::AABBox ob_aabb = objects[i].aabb;
				assert(aabb.containsAABBox(ob_aabb));

				if(ob_aabb.min_[best_axis] < best_div_val && ob_aabb.max_[best_axis] > best_div_val) // If triangle straddles split plane:
				{
					// Compute C_1: the cost when putting object i entirely in the left child.
					const js::AABBox expanded_left_aabb  = AABBUnion(res.left_aabb,  ob_aabb);
					const js::AABBox expanded_right_aabb = AABBUnion(res.right_aabb, ob_aabb);
					const float C_1 = expanded_left_aabb.getHalfSurfaceArea() * num_left  + right_reduced_cost;
					const float C_2 = left_reduced_cost + expanded_right_aabb.getHalfSurfaceArea() * num_right;

					unsplit_out[i] = 0;
					if(C_1 < C_split && C_1 <= C_2)
					{
						unsplit_out[i] = 1;
						num_unsplit++;
					}
					else if(C_2 < C_split && C_2 < C_1)
					{
						unsplit_out[i] = 2;
						num_unsplit++;
					}
				}
				else
					unsplit_out[i] = 0;
			}

			// Do another pass to get the final cost given our unsplitting decisions
			tentativeSpatialPartition(objects_, triangles, aabb, left, right, spatial_best_div_val, spatial_best_axis, &unsplit_out, res, num_left, num_right);
			const float unsplit_cost = res.left_aabb.getHalfSurfaceArea() * num_left + res.right_aabb.getHalfSurfaceArea() * num_right;
			assert(unsplit_cost <= spatial_smallest_split_cost_factor);

			//if(unsplit_cost < spatial_smallest_split_cost_factor)
			//	conPrint("unsplit " + toString(num_unsplit) + " / " + toString(right - left) + " tris: unsplit_cost: " + toString(unsplit_cost) + ", old cost: " + toString(spatial_smallest_split_cost_factor));

			if(unsplit_cost < smallest_split_cost_factor)
			{
				smallest_split_cost_factor = unsplit_cost;
				best_axis = spatial_best_axis;
				best_div_val = spatial_best_div_val;
				best_split_is_spatial = true;
			}
		}
	}
	else // Else if !DO_UNSPLITTING:
	{
		unsplit_out.resize(right - left);
		for(int i=0; i<right-left; ++i)
			unsplit_out[i] = 0;

		if(spatial_smallest_split_cost_factor < smallest_split_cost_factor)
		{
			smallest_split_cost_factor = spatial_smallest_split_cost_factor;
			best_axis = spatial_best_axis;
			best_div_val = spatial_best_div_val;
			best_split_is_spatial = true;
		}
	}

	//========================================================================================================

	best_axis_out = best_axis;
	best_div_val_out = best_div_val;
	smallest_split_cost_factor_out = smallest_split_cost_factor;
	best_split_is_spatial_out = best_split_is_spatial;
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
			const std::vector<SBVHOb>& obs,
			int depth,
			uint64 sort_key,
			SBVHResultChunk& result_chunk
			)
{
	const int MAX_DEPTH = 60;

	const int num_objects = (int)obs.size();

	js::Vector<ResultNode, 64>& chunk_nodes = thread_temp_info.result_buf;

	if(num_objects <= leaf_num_object_threshold || depth >= MAX_DEPTH)
	{
		assert(num_objects <= max_num_objects_per_leaf);

		// Make this a leaf node
		chunk_nodes[node_index].interior = false;
		chunk_nodes[node_index].aabb = aabb;
		chunk_nodes[node_index].depth = (uint8)depth;
		chunk_nodes[node_index].left  = (int)thread_temp_info.leaf_obs.size();
		chunk_nodes[node_index].right = (int)thread_temp_info.leaf_obs.size() + num_objects;
		for(int i=0; i<num_objects; ++i)
			thread_temp_info.leaf_obs.push_back(obs[i].getIndex());

		// Update build stats
		if(depth >= MAX_DEPTH)
			thread_temp_info.stats.num_maxdepth_leaves++;
		else
			thread_temp_info.stats.num_under_thresh_leaves++;
		thread_temp_info.stats.leaf_depth_sum += depth;
		thread_temp_info.stats.max_leaf_depth = myMax(thread_temp_info.stats.max_leaf_depth, depth);
		thread_temp_info.stats.max_num_tris_per_leaf = myMax(thread_temp_info.stats.max_num_tris_per_leaf, num_objects);
		thread_temp_info.stats.num_leaves++;
		return;
	}

	const float traversal_cost = 1.0f;
	const float non_split_cost = num_objects * intersection_cost; // Eqn 2.
	float smallest_split_cost_factor; // Smallest (N_L * half_left_surface_area + N_R * half_right_surface_area) found.
	int best_axis;// = -1;
	float best_div_val;// = 0;
	bool best_split_was_spatial;

	//conPrint("Looking for best split...");
	//Timer timer;
	search(aabb, centroid_aabb, obs, triangles, 0, num_objects, recip_root_node_aabb_area, best_axis, best_div_val, smallest_split_cost_factor, best_split_was_spatial, thread_temp_info.unsplit);
	//conPrint("Looking for best split done.  elapsed: " + timer.elapsedString());
	//split_search_time += timer.elapsed();

	if(best_axis == -1) // This can happen when all object centres coordinates along the axis are the same.
	{
		doArbitrarySplits(thread_temp_info, aabb, node_index, obs, /*left, right, */depth, result_chunk);
		return;
	}

	if(best_split_was_spatial)
		thread_temp_info.stats.num_spatial_splits++;
	else
		thread_temp_info.stats.num_object_splits++;

	// NOTE: the factor of 2 compensates for the surface area vars being half the areas.
	const float smallest_split_cost = 2 * intersection_cost * smallest_split_cost_factor / aabb.getSurfaceArea() + traversal_cost; // Eqn 1.

	// If it is not advantageous to split, and the number of objects is <= the max num per leaf, then form a leaf:
	if((smallest_split_cost >= non_split_cost) && (num_objects <= max_num_objects_per_leaf))
	{
		chunk_nodes[node_index].interior = false;
		chunk_nodes[node_index].aabb = aabb;
		chunk_nodes[node_index].depth = (uint8)depth;
		chunk_nodes[node_index].left  = (int)thread_temp_info.leaf_obs.size();
		chunk_nodes[node_index].right = (int)thread_temp_info.leaf_obs.size() + num_objects;
		for(int i=0; i<num_objects; ++i)
			thread_temp_info.leaf_obs.push_back(obs[i].getIndex());

		// Update build stats
		thread_temp_info.stats.num_cheaper_nosplit_leaves++;
		thread_temp_info.stats.leaf_depth_sum += depth;
		thread_temp_info.stats.max_leaf_depth = myMax(thread_temp_info.stats.max_leaf_depth, depth);
		thread_temp_info.stats.max_num_tris_per_leaf = myMax(thread_temp_info.stats.max_num_tris_per_leaf, num_objects);
		thread_temp_info.stats.num_leaves++;
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
	PartitionRes res;
	partition(obs, triangles, aabb, best_div_val, best_axis, best_split_was_spatial, thread_temp_info.unsplit, res, thread_temp_info.left_obs[depth + 1], thread_temp_info.right_obs[depth + 1]);
	//conPrint("Partition done.  elapsed: " + timer.elapsedString());

	const int num_left_tris  = (int)thread_temp_info.left_obs[depth + 1].size();
	const int num_right_tris = (int)thread_temp_info.right_obs[depth + 1].size();

	setAABBWToOne(res.left_aabb);
	setAABBWToOne(res.left_centroid_aabb);
	setAABBWToOne(res.right_aabb);
	setAABBWToOne(res.right_centroid_aabb);

	assert(aabb.containsAABBox(res.left_aabb));
	assert(res.left_aabb.containsAABBox(res.left_centroid_aabb));
	//assert(centroid_aabb.containsAABBox(res.left_centroid_aabb));
	assert(aabb.containsAABBox(res.right_aabb));
	assert(res.right_aabb.containsAABBox(res.right_centroid_aabb));
	//assert(centroid_aabb.containsAABBox(res.right_centroid_aabb));


	if(num_left_tris == 0 || num_right_tris == 0)
	{
		// Split objects arbitrarily until the number in each leaf is <= max_num_objects_per_leaf.
		doArbitrarySplits(thread_temp_info, aabb, node_index, obs, depth, result_chunk);
		return;
	}

	//conPrint("Partitioning done.  elapsed: " + timer.elapsedString());
	//timer.reset();
	//partition_time += timer.elapsed();

	// Create child nodes
	const uint32 left_child = (uint32)chunk_nodes.size();
	chunk_nodes.push_back_uninitialised();

	const bool do_right_child_in_new_task = (num_left_tris >= new_task_num_ob_threshold) && (num_right_tris >= new_task_num_ob_threshold);

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
		thread_temp_info.left_obs[depth + 1], // objects
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
		Reference<SBVHBuildSubtreeTask> subtree_task = new SBVHBuildSubtreeTask(*this);
		subtree_task->result_chunk = &result_chunks[chunk_index];
		subtree_task->node_aabb = res.right_aabb;
		subtree_task->centroid_aabb = res.right_centroid_aabb;
		subtree_task->depth = depth + 1;
		subtree_task->sort_key = (sort_key << 1) | 1;
		subtree_task->objects = thread_temp_info.right_obs[depth + 1];
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
			thread_temp_info.right_obs[depth + 1],
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
void SBVHBuilder::doArbitrarySplits(
		SBVHPerThreadTempInfo& thread_temp_info,
		const js::AABBox& aabb,
		uint32 node_index, 
		const ObjectVecType& obs,
		int depth,
		SBVHResultChunk& result_chunk
	)
{
	const int num_objects = (int)obs.size();

	js::Vector<ResultNode, 64>& chunk_nodes = thread_temp_info.result_buf;

	if(num_objects <= max_num_objects_per_leaf)
	{
		// Make this a leaf node
		chunk_nodes[node_index].interior = false;
		chunk_nodes[node_index].aabb = aabb;
		chunk_nodes[node_index].depth = (uint8)depth;

		chunk_nodes[node_index].left  = (int)thread_temp_info.leaf_obs.size(); // left;
		chunk_nodes[node_index].right = (int)thread_temp_info.leaf_obs.size() + num_objects; //right;
		for(int i=0; i<num_objects; ++i)
			thread_temp_info.leaf_obs.push_back(obs[i].getIndex());

		// Update build stats
		thread_temp_info.stats.num_arbitrary_split_leaves++;
		thread_temp_info.stats.leaf_depth_sum += depth;
		thread_temp_info.stats.max_leaf_depth = myMax(thread_temp_info.stats.max_leaf_depth, depth);
		thread_temp_info.stats.max_num_tris_per_leaf = myMax(thread_temp_info.stats.max_num_tris_per_leaf, num_objects);
		thread_temp_info.stats.num_leaves++;
		return;
	}

	const int split_i = num_objects/2;


	// Compute AABBs for children (partition as well)
	js::AABBox left_aabb  = empty_aabb;
	js::AABBox right_aabb = empty_aabb;

	thread_temp_info.left_obs[depth + 1].resize(0);
	thread_temp_info.right_obs[depth + 1].resize(0);

	for(int i=0; i<split_i; ++i)
	{
		left_aabb.enlargeToHoldAABBox(obs[i].aabb);
		thread_temp_info.left_obs[depth + 1].push_back(obs[i]);
	}

	for(int i=split_i; i<num_objects; ++i)
	{
		right_aabb.enlargeToHoldAABBox(obs[i].aabb);
		thread_temp_info.right_obs[depth + 1].push_back(obs[i]);
	}

	// Partition into left and right child lists


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

	// Build left child
	doArbitrarySplits(
		thread_temp_info,
		left_aabb, // aabb
		left_child, // node index
		obs,
		depth + 1, // depth
		result_chunk
	);

	// Build right child
	doArbitrarySplits(
		thread_temp_info,
		right_aabb, // aabb
		right_child, // node index
		obs,
		depth + 1, // depth
		result_chunk
	);
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/StandardPrintOutput.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Vector.h"
#include "../utils/MTwister.h"
#include "../utils/TaskManager.h"
#include "../utils/Timer.h"
#include "../utils/Plotter.h"
#include "../utils/FileUtils.h"


static void rotateVerts(SBVHTri& tri)
{
	SBVHTri old_tri = tri;
	tri.v[0] = old_tri.v[2];
	tri.v[1] = old_tri.v[0];
	tri.v[2] = old_tri.v[1];
}


void SBVHBuilder::test()
{
	conPrint("SBVHBuilder::test()");


	//---------------------- Test getTriClippedtoBucket(const SBVHTri& tri, int axis, float left, float right, js::AABBox& clipped_aabb_out) -------------------------
	{
		/*
		tri with one vertex on left side of left plane, two between left and right
		*/
		SBVHTri tri;
		tri.v[0] = Vec4f(0.5f, 1.f,  0, 1);
		tri.v[1] = Vec4f(1.5f, 1.5f, 0, 1);
		tri.v[2] = Vec4f(1.5f, 2.f,  0, 1);
		const js::AABBox aabb(Vec4f(0, 0, 0, 1), Vec4f(3, 3, 3, 1));

		for(int i=0; i<3; ++i)
		{
			js::AABBox clipped_aabb;
			getTriClippedtoBucket(aabb, tri, 0, /*left=*/1.0f, /*right=*/2.0f, clipped_aabb);
			testEpsEqual(clipped_aabb.min_, Vec4f(1.f, 1.25f, 0, 1));
			testEpsEqual(clipped_aabb.max_, Vec4f(1.5f, 2.f, 0, 1));
		
			rotateVerts(tri);
		}
	}

	{
		/*
		tri with two vertices on left side of left plane, one between left and right
		*/
		SBVHTri tri;
		tri.v[0] = Vec4f(0.5f, 1.5f, 0, 1);
		tri.v[1] = Vec4f(0.5f, 2.0f, 0, 1);
		tri.v[2] = Vec4f(1.5f, 1.f, 0, 1);
		const js::AABBox aabb(Vec4f(0, 0, 0, 1), Vec4f(3, 3, 3, 1));

		for(int i=0; i<3; ++i)
		{
			js::AABBox clipped_aabb;
			getTriClippedtoBucket(aabb, tri, 0, /*left=*/1.0f, /*right=*/2.0f, clipped_aabb);
			testEpsEqual(clipped_aabb.min_, Vec4f(1.0f, 1.0f, 0, 1));
			testEpsEqual(clipped_aabb.max_, Vec4f(1.5f, 1.5f, 0, 1));

			rotateVerts(tri);
		}
	}

	{
		/*
		tri with one vertex on right side of right plane, two between left and right
		*/
		SBVHTri tri;
		tri.v[0] = Vec4f(1.5f, 1.5f, 0, 1);
		tri.v[1] = Vec4f(1.5f, 2.0f, 0, 1);
		tri.v[2] = Vec4f(2.5f, 1.0f, 0, 1);
		const js::AABBox aabb(Vec4f(0, 0, 0, 1), Vec4f(3, 3, 3, 1));

		for(int i=0; i<3; ++i)
		{
			js::AABBox clipped_aabb;
			getTriClippedtoBucket(aabb, tri, 0, /*left=*/1.0f, /*right=*/2.0f, clipped_aabb);
			testEpsEqual(clipped_aabb.min_, Vec4f(1.5f, 1.25f, 0, 1));
			testEpsEqual(clipped_aabb.max_, Vec4f(2.0f, 2.0f, 0, 1));

			rotateVerts(tri);
		}
	}

	{
		/*
		tri with two vertices on right side of right plane, one between left and right
		*/
		SBVHTri tri;
		tri.v[0] = Vec4f(1.5f, 1.f, 0, 1);
		tri.v[1] = Vec4f(2.5f, 1.5f, 0, 1);
		tri.v[2] = Vec4f(2.5f, 2.f, 0, 1);
		const js::AABBox aabb(Vec4f(0, 0, 0, 1), Vec4f(3, 3, 3, 1));

		for(int i=0; i<3; ++i)
		{
			js::AABBox clipped_aabb;
			getTriClippedtoBucket(aabb, tri, 0, /*left=*/1.0f, /*right=*/2.0f, clipped_aabb);
			testEpsEqual(clipped_aabb.min_, Vec4f(1.5f, 1.0f, 0, 1));
			testEpsEqual(clipped_aabb.max_, Vec4f(2.0f, 1.5f, 0, 1));

			rotateVerts(tri);
		}
	}

	{
		/*
		tri with all vertices between left and right
		*/
		SBVHTri tri;
		tri.v[0] = Vec4f(1.1f, 1.f, 0, 1);
		tri.v[1] = Vec4f(1.5f, 1.5f, 0, 1);
		tri.v[2] = Vec4f(1.5f, 2.f, 0, 1);
		const js::AABBox aabb(Vec4f(0, 0, 0, 1), Vec4f(3, 3, 3, 1));

		for(int i=0; i<3; ++i)
		{
			js::AABBox clipped_aabb;
			getTriClippedtoBucket(aabb, tri, 0, /*left=*/1.0f, /*right=*/2.0f, clipped_aabb);
			testEpsEqual(clipped_aabb.min_, Vec4f(1.1f, 1.0f, 0, 1));
			testEpsEqual(clipped_aabb.max_, Vec4f(1.5f, 2.f, 0, 1));

			rotateVerts(tri);
		}
	}

	{
		/*
		tri with one vertex on left side of left plane, two on right side of right plane
		*/
		SBVHTri tri;
		tri.v[0] = Vec4f(0.5f, 1.f, 0, 1);
		tri.v[1] = Vec4f(2.5f, 1.5f, 0, 1);
		tri.v[2] = Vec4f(2.5f, 2.f, 0, 1);
		const js::AABBox aabb(Vec4f(0, 0, 0, 1), Vec4f(3, 3, 3, 1));

		for(int i=0; i<3; ++i)
		{
			js::AABBox clipped_aabb;
			getTriClippedtoBucket(aabb, tri, 0, /*left=*/1.0f, /*right=*/2.0f, clipped_aabb);
			testEpsEqual(clipped_aabb.min_, Vec4f(1.f, 1.125f, 0, 1));
			testEpsEqual(clipped_aabb.max_, Vec4f(2.f, 1.75f, 0, 1));

			rotateVerts(tri);
		}
	}

	{
		/*
		tri with one vertex on left side of left plane, one between planes, one on right side of plane
		*/
		SBVHTri tri;
		tri.v[0] = Vec4f(0.5f, 1.f, 0, 1);
		tri.v[1] = Vec4f(1.5f, 1.25f, 0, 1);
		tri.v[2] = Vec4f(2.5f, 2.f, 0, 1);
		const js::AABBox aabb(Vec4f(0, 0, 0, 1), Vec4f(3, 3, 3, 1));

		for(int i=0; i<3; ++i)
		{
			js::AABBox clipped_aabb;
			getTriClippedtoBucket(aabb, tri, 0, /*left=*/1.0f, /*right=*/2.0f, clipped_aabb);
			testEpsEqual(clipped_aabb.min_, Vec4f(1.f, 1.125f, 0, 1));
			testEpsEqual(clipped_aabb.max_, Vec4f(2.f, 1.75f, 0, 1));

			rotateVerts(tri);
		}
	}

	conPrint("SBVHBuilder::test() done");
}


#endif
