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


static const js::AABBox empty_aabb = js::AABBox::emptyAABBox();


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
	for(size_t i=0; i<left_obs.size(); ++i)
		s += left_obs[i].capacity() * sizeof(SBVHOb);
	for(size_t i=0; i<right_obs.size(); ++i)
		s += right_obs[i].capacity() * sizeof(SBVHOb);
	s += unsplit.capacity() * sizeof(int);
	return s;
}


SBVHBuilder::SBVHBuilder(int leaf_num_object_threshold_, int max_num_objects_per_leaf_, float intersection_cost_,
	const js::AABBox* aabbs_,
	const SBVHTri* triangles_,
	const int num_objects_
)
:	leaf_num_object_threshold(leaf_num_object_threshold_),
	max_num_objects_per_leaf(max_num_objects_per_leaf_),
	intersection_cost(intersection_cost_)
{
	assert(intersection_cost > 0.f);

	aabbs = aabbs_;
	triangles = triangles_;
	m_num_objects = num_objects_;

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

		builder.per_thread_temp_info[thread_index].result_chunk = result_chunk;
		builder.per_thread_temp_info[thread_index].leaf_result_chunk = leaf_result_chunk;

		result_chunk->size = 1;

		builder.doBuild(
			builder.per_thread_temp_info[thread_index],
			node_aabb,
			centroid_aabb,
			0, // node index
			objects,
			depth, 
			result_chunk
		);
	}

	SBVHBuilder& builder;
	SBVHResultChunk* result_chunk;
	SBVHLeafResultChunk* leaf_result_chunk;
	std::vector<SBVHOb> objects;
	js::AABBox node_aabb;
	js::AABBox centroid_aabb;
	int depth;
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


// Top-level build method
void SBVHBuilder::build(
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
	per_thread_temp_info.resize(task_manager->getNumThreads());
	for(size_t i=0; i<per_thread_temp_info.size(); ++i)
	{
		per_thread_temp_info[i].left_obs.resize(62);
		per_thread_temp_info[i].right_obs.resize(62);
	}

	} // End scope for initial init


	SBVHResultChunk* root_chunk = allocNewResultChunk();
	SBVHLeafResultChunk* root_leaf_chunk = allocNewLeafResultChunk();

	Reference<SBVHBuildSubtreeTask> task = new SBVHBuildSubtreeTask(*this);
	task->objects = this->top_level_objects;
	task->depth = 0;
	task->node_aabb = root_aabb;
	task->centroid_aabb = centroid_aabb;
	task->result_chunk = root_chunk;
	task->leaf_result_chunk = root_leaf_chunk;
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


	// First combine leaf geom
	js::Vector<uint32, 16> final_leaf_geom_indices(leaf_result_chunks.size() * SBVHLeafResultChunk::MAX_RESULT_CHUNK_SIZE);
	uint32 write_i = 0;
	for(size_t c=0; c<leaf_result_chunks.size(); ++c)
		for(size_t i=0; i<leaf_result_chunks[c]->size; ++i)
			final_leaf_geom_indices[c * SBVHLeafResultChunk::MAX_RESULT_CHUNK_SIZE + i] = write_i++;

#ifndef NDEBUG
	const int total_num_leaf_geom = write_i;
#endif
	result_indices.resizeNoCopy(write_i);

	for(size_t c=0; c<leaf_result_chunks.size(); ++c)
	{
		const SBVHLeafResultChunk& chunk = *leaf_result_chunks[c];
		size_t src_i = c * SBVHLeafResultChunk::MAX_RESULT_CHUNK_SIZE;

		for(size_t i=0; i<chunk.size; ++i)
		{
			int leaf_geom = chunk.leaf_obs[i];

			const uint32 final_pos = final_leaf_geom_indices[src_i];
			result_indices[final_pos] = leaf_geom; // Copy leaf geom index to final array
			src_i++;
		}

		delete leaf_result_chunks[c];
	}




	js::Vector<uint32, 16> final_node_indices(result_chunks.size() * SBVHResultChunk::MAX_RESULT_CHUNK_SIZE);
	write_i = 0;
	for(size_t c=0; c<result_chunks.size(); ++c)
	{
		for(size_t i=0; i<result_chunks[c]->size; ++i)
		{
			final_node_indices[c * SBVHResultChunk::MAX_RESULT_CHUNK_SIZE + i] = write_i++;
		}
	}

#ifndef NDEBUG
	const int total_num_nodes = write_i;
#endif
	result_nodes_out.resizeNoCopy(write_i);

	for(size_t c=0; c<result_chunks.size(); ++c)
	{
		const SBVHResultChunk& chunk = *result_chunks[c];
		size_t src_node_i = c * SBVHResultChunk::MAX_RESULT_CHUNK_SIZE;

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
			else
			{
				const int old_left = chunk_node.left;
				const int old_right = chunk_node.right;

				chunk_node.left  = final_leaf_geom_indices[chunk_node.left];
				chunk_node.right = chunk_node.left + (old_right - old_left);

				assert(chunk_node.left  >= 0 && chunk_node.left  < total_num_leaf_geom);
				assert(chunk_node.right >= 0 && chunk_node.right <= total_num_leaf_geom);
			}

			const uint32 final_pos = final_node_indices[src_node_i];
			result_nodes_out[final_pos] = chunk_node; // Copy node to final array
			src_node_i++;
		}

		delete result_chunks[c];
	}
	//conPrint("Final merge elapsed: " + timer.elapsedString());

	// Combine stats from each thread
	for(size_t i=0; i<per_thread_temp_info.size(); ++i)
		this->stats.accumStats(per_thread_temp_info[i].stats);

	// Dump some mem usage stats
	if(false)
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
	const js::Vector<int, 16>& unsplit,
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


// Partition half the list left and half right.
static void arbitraryPartition(const std::vector<SBVHOb>& objects_, int left, int right, PartitionRes& res_out, 
	std::vector<SBVHOb>& left_obs_out,
	std::vector<SBVHOb>& right_obs_out)
{
	const SBVHOb* const objects = objects_.data();

	left_obs_out.resize(0);
	right_obs_out.resize(0);

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
		left_obs_out.push_back(objects[i]);
	}

	for(int i=split_i; i<right; ++i)
	{
		const js::AABBox aabb = objects[i].aabb;
		const Vec4f centroid = aabb.centroid();
		right_aabb.enlargeToHoldAABBox(aabb);
		right_centroid_aabb.enlargeToHoldPoint(centroid);
		right_obs_out.push_back(objects[i]);
	}

	res_out.left_aabb = left_aabb;
	res_out.left_centroid_aabb = left_centroid_aabb;
	res_out.right_aabb = right_aabb;
	res_out.right_centroid_aabb = right_centroid_aabb;
}


static void tentativeSpatialPartition(const std::vector<SBVHOb>& objects_, const SBVHTri* triangles, const js::AABBox& parent_aabb, int left, int right, 
	float best_div_val, int best_axis, const js::Vector<int, 16>* unsplits,  PartitionRes& res_out, int& num_left_out, int& num_right_out)
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


static INDIGO_STRONG_INLINE void addIntersectedEdgeVert(const Vec4f& v_a, const Vec4f& v_b, const float d_a, const float d_b, float split_coord, js::AABBox& left_aabb, js::AABBox& right_aabb)
{
	const float t = (split_coord - d_a) / (d_b - d_a); // Solve for fraction along edge of position on split plane.
	const Vec4f p = v_a * (1 - t) + v_b * t;
	left_aabb.enlargeToHoldPoint(p);
	right_aabb.enlargeToHoldPoint(p);
}

static INDIGO_STRONG_INLINE void clipTri(const js::AABBox& ob_aabb, const SBVHTri& tri, int axis, float split_coord, js::AABBox& left_clipped_aabb_out, js::AABBox& right_clipped_aabb_out)
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
	assert(left_aabb .max_[axis] <= split_coord + 1.0e-5f);
	assert(right_aabb.min_[axis] >= split_coord - 1.0e-5f);

	left_clipped_aabb_out  = left_aabb;
	right_clipped_aabb_out = right_aabb;
}


static void search(const js::AABBox& aabb, const js::AABBox& centroid_aabb_, const std::vector<SBVHOb>& objects_, const SBVHTri* triangles, int left, int right, float recip_root_node_aabb_area, 
	int& best_axis_out, float& best_div_val_out,
	float& smallest_split_cost_factor_out, bool& best_split_is_spatial_out, js::Vector<int, 16>& unsplit_out)
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
		const int num_spatial_buckets = 16;
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
			
			// Pre-fetch a triangle.  This helps because it's an indirect memory access to get the triangle.
			const int PREFETCH_DIST = 8;
			if(i + PREFETCH_DIST < right)
				_mm_prefetch((const char*)(triangles + (objects[i + PREFETCH_DIST].getIndex())), _MM_HINT_T0);
		   
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
			unsplit_out.resizeNoCopy(right - left);

			PartitionRes res;
			int num_left, num_right;
			tentativeSpatialPartition(objects_, triangles, aabb, left, right, spatial_best_div_val, spatial_best_axis, /*unsplits=*/NULL, res, num_left, num_right);

			const float left_half_area  = res.left_aabb. getHalfSurfaceArea();
			const float right_half_area = res.right_aabb.getHalfSurfaceArea();
			const float C_split = left_half_area * num_left + right_half_area * num_right;
			//assert(epsEqual(C_split, spatial_smallest_split_cost_factor));

			const float left_reduced_cost  = res.left_aabb.getHalfSurfaceArea()  * (num_left - 1);
			const float right_reduced_cost = res.right_aabb.getHalfSurfaceArea() * (num_right - 1);
			int num_unsplit = 0;
			for(int i=left; i<right; ++i)
			{
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
			assert(unsplit_cost <= spatial_smallest_split_cost_factor/* + 1.0e-4f*/); // TODO: investigate the need for this eps

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
		unsplit_out.resizeNoCopy(right - left);
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


const static int MAX_DEPTH = 60;


void SBVHBuilder::markNodeAsLeaf(SBVHPerThreadTempInfo& thread_temp_info, const js::AABBox& aabb, uint32 node_index, const std::vector<SBVHOb>& obs, int depth, SBVHResultChunk* node_result_chunk)
{
	const int num_objects = (int)obs.size();
	assert(num_objects <= max_num_objects_per_leaf);

	SBVHLeafResultChunk* leaf_result_chunk = thread_temp_info.leaf_result_chunk;

	// Make sure we have space for leaf geom
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
		leaf_result_chunk->leaf_obs[leaf_result_chunk->size + i] = obs[i].getIndex();
	leaf_result_chunk->size += num_objects;

	assert(leaf_result_chunk->size <= SBVHLeafResultChunk::MAX_RESULT_CHUNK_SIZE);

	// Update build stats
	if(depth >= MAX_DEPTH)
		thread_temp_info.stats.num_maxdepth_leaves++;
	else
		thread_temp_info.stats.num_under_thresh_leaves++;
	thread_temp_info.stats.leaf_depth_sum += depth;
	thread_temp_info.stats.max_leaf_depth = myMax(thread_temp_info.stats.max_leaf_depth, depth);
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
			const std::vector<SBVHOb>& obs,
			int depth,
			SBVHResultChunk* node_result_chunk
			)
{
	const int num_objects = (int)obs.size();

	SBVHLeafResultChunk* leaf_result_chunk = thread_temp_info.leaf_result_chunk;

	if(num_objects <= leaf_num_object_threshold || depth >= MAX_DEPTH)
	{
		markNodeAsLeaf(thread_temp_info, aabb, node_index, obs, depth, node_result_chunk);
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

	PartitionRes res;
	if(best_axis == -1) // This can happen when all object centres coordinates along the axis are the same.
	{
		arbitraryPartition(obs, 0, num_objects, res, thread_temp_info.left_obs[depth + 1], thread_temp_info.right_obs[depth + 1]);
	}
	else
	{
		if(best_split_was_spatial)
			thread_temp_info.stats.num_spatial_splits++;
		else
			thread_temp_info.stats.num_object_splits++;

		// NOTE: the factor of 2 compensates for the surface area vars being half the areas.
		const float smallest_split_cost = 2 * intersection_cost * smallest_split_cost_factor / aabb.getSurfaceArea() + traversal_cost; // Eqn 1.

		// If it is not advantageous to split, and the number of objects is <= the max num per leaf, then form a leaf:
		if((smallest_split_cost >= non_split_cost) && (num_objects <= max_num_objects_per_leaf))
		{
			markNodeAsLeaf(thread_temp_info, aabb, node_index, obs, depth, node_result_chunk);
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
		thread_temp_info.left_obs[depth + 1].reserve(obs.size()/2);
		thread_temp_info.right_obs[depth + 1].reserve(obs.size()/2);

		partition(obs, triangles, aabb, best_div_val, best_axis, best_split_was_spatial, thread_temp_info.unsplit, res, thread_temp_info.left_obs[depth + 1], thread_temp_info.right_obs[depth + 1]);
		//conPrint("Partition done.  elapsed: " + timer.elapsedString());
	}

	

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

	int num_left_tris  = (int)thread_temp_info.left_obs[depth + 1].size();
	int num_right_tris = (int)thread_temp_info.right_obs[depth + 1].size();

	if(num_left_tris == 0 || num_right_tris == 0)
	{
		// Split objects arbitrarily until the number in each leaf is <= max_num_objects_per_leaf.
		arbitraryPartition(obs, 0, num_objects, res, thread_temp_info.left_obs[depth + 1], thread_temp_info.right_obs[depth + 1]);

		num_left_tris  = (int)thread_temp_info.left_obs[depth + 1].size();
		num_right_tris = (int)thread_temp_info.right_obs[depth + 1].size();
	}

	// Allocate left child from the thread's current result chunk
	if(thread_temp_info.result_chunk->size >= SBVHResultChunk::MAX_RESULT_CHUNK_SIZE) // If the current chunk is full:
		thread_temp_info.result_chunk = allocNewResultChunk();
	
	const size_t left_child = thread_temp_info.result_chunk->size++;
	SBVHResultChunk* left_child_chunk = thread_temp_info.result_chunk;

	const bool do_right_child_in_new_task = (num_left_tris >= new_task_num_ob_threshold) && (num_right_tris >= new_task_num_ob_threshold);

	// Allocate right child
	SBVHResultChunk* right_child_chunk;
	SBVHLeafResultChunk* right_child_leaf_chunk;
	uint32 right_child;
	if(!do_right_child_in_new_task)
	{
		if(thread_temp_info.result_chunk->size >= SBVHResultChunk::MAX_RESULT_CHUNK_SIZE) // If the current chunk is full:
			thread_temp_info.result_chunk = allocNewResultChunk();

		right_child = (uint32)(thread_temp_info.result_chunk->size++);
		right_child_chunk = thread_temp_info.result_chunk;
		right_child_leaf_chunk = leaf_result_chunk;
	}
	else
	{
		right_child_chunk = allocNewResultChunk(); // This child subtree will be built in a new task, so probably in a different thread.  So use a new chunk for it.
		right_child_leaf_chunk = allocNewLeafResultChunk();
		right_child = 0; // Root node of new task subtree chunk.
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
		(uint32)left_child, // node index
		thread_temp_info.left_obs[depth + 1], // objects
		depth + 1, // depth
		left_child_chunk
	);

	if(do_right_child_in_new_task)
	{
		// Put this subtree into a task
		Reference<SBVHBuildSubtreeTask> subtree_task = new SBVHBuildSubtreeTask(*this);
		subtree_task->result_chunk = right_child_chunk;
		subtree_task->leaf_result_chunk = right_child_leaf_chunk;
		subtree_task->node_aabb = res.right_aabb;
		subtree_task->centroid_aabb = res.right_centroid_aabb;
		subtree_task->depth = depth + 1;
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
			right_child_chunk
		);
	}
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


#endif
