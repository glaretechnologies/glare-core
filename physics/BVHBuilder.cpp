/*=====================================================================
BVHBuilder.cpp
-------------------
Copyright Glare Technologies Limited 2015 -
Generated at Tue Apr 27 15:25:47 +1200 2010
=====================================================================*/
#include "BVHBuilder.h"


#include "jscol_aabbox.h"
#include <algorithm>
#include "../utils/Exception.h"
#include "../utils/Sort.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/TaskManager.h"
#include "../utils/Lock.h"
#include "../utils/Timer.h"


BVHBuilder::BVHBuilder(int leaf_num_object_threshold_, int max_num_objects_per_leaf_, float intersection_cost_)
:	leaf_num_object_threshold(leaf_num_object_threshold_),
	max_num_objects_per_leaf(max_num_objects_per_leaf_),
	intersection_cost(intersection_cost_),
	local_task_manager(NULL)
{
	assert(intersection_cost > 0.f);

	aabbs = NULL;

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

	axis_parallel_num_ob_threshold = 1 << 20;
	new_task_num_ob_threshold = 1 << 10;

	static_assert(sizeof(Ob) == 48, "sizeof(Ob) == 48");
}


BVHBuilder::~BVHBuilder()
{
	delete local_task_manager;
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
class SortAxisTask : public Indigo::Task
{
public:
	SortAxisTask(BVHBuilder& builder_, int num_objects_, int axis_, js::Vector<CenterItem, 16>* axis_centres_) : builder(builder_), num_objects(num_objects_), axis(axis_), axis_centres(axis_centres_) {}
	
	virtual void run(size_t thread_index)
	{
		const js::Vector<CenterItem, 16>& centres = *axis_centres; // Sorted CenterItems
		js::Vector<Ob, 64>& axis_obs = builder.objects[axis]; // Array we will be writing to.
		const js::AABBox* const aabbs = builder.aabbs; // Unsorted AABBs

		//Timer timer2;
		for(int i=0; i<num_objects; ++i)
		{
			const int src_i = centres[i].i;
			axis_obs[i].aabb = aabbs[src_i];
			axis_obs[i].centre = toVec3f(aabbs[src_i].centroid());
			axis_obs[i].index = src_i;
		}
		//conPrint("post-sort ob copy: " + timer2.elapsedString());
	}

	BVHBuilder& builder;
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

	BuildSubtreeTask(BVHBuilder& builder_) : builder(builder_) {}
	
	virtual void run(size_t thread_index)
	{
		// Create subtree root node
		const int root_node_index = (int)builder.per_thread_temp_info[thread_index].result_buf.size();
		builder.per_thread_temp_info[thread_index].result_buf.push_back_uninitialised();
		builder.per_thread_temp_info[thread_index].result_buf[root_node_index].right_child_chunk_index = -666;

		builder.doBuild(
			builder.per_thread_temp_info[thread_index],
			node_aabb, 
			root_node_index, // node index
			begin, end, depth,
			*result_chunk
		);

		result_chunk->thread_index = (int)thread_index;
		result_chunk->offset = root_node_index;
		result_chunk->size = (int)builder.per_thread_temp_info[thread_index].result_buf.size() - root_node_index;
	}

	BVHBuilder& builder;
	ResultChunk* result_chunk;
	js::AABBox node_aabb;
	int depth;
	int begin;
	int end;
};


// top-level build method
void BVHBuilder::build(
		   Indigo::TaskManager& task_manager_,
		   const js::AABBox* aabbs_,
		   const int num_objects,
		   PrintOutput& print_output, 
		   bool verbose, 
		   js::Vector<ResultNode, 64>& result_nodes_out
		   )
{
	//Timer timer;

	this->task_manager = &task_manager_;
	this->aabbs = aabbs_;

	// Create a new task manager for the BestSplitSearchTask tasks, if needed.
	if(num_objects >= axis_parallel_num_ob_threshold)
	{
		delete local_task_manager;
		local_task_manager = new Indigo::TaskManager(3);
	}


	if(num_objects <= 0)
	{
		// Create root node, and mark it as a leaf.
		result_nodes_out.push_back_uninitialised();
		result_nodes_out[0].interior = false;
		result_nodes_out[0].left = 0;
		result_nodes_out[0].right = 0;


		num_under_thresh_leaves = 1;
		//leaf_depth_sum += 0;
		max_leaf_depth = 0;
		num_leaves = 1;
		return;
	}

	// Build overall AABB
	js::AABBox root_aabb = aabbs[0];
	for(size_t i = 0; i < num_objects; ++i)
		root_aabb.enlargeToHoldAABBox(aabbs[i]);

	// Alloc space for objects for each axis
	//timer.reset();
	for(int c=0; c<3; ++c)
	{
		js::Vector<Ob, 64>& axis_obs = this->objects[c];
		axis_obs.resizeUninitialised(num_objects);
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
			axis_centres.resizeUninitialised(num_objects);

			//Timer timer2;
			for(int i=0; i<num_objects; ++i)
			{
				axis_centres[i].f = aabbs[i].centroid()[axis];
				axis_centres[i].i = i;
			}
			//conPrint("building axis_centres[]: " + timer2.elapsedString());

			// Sort axis_centres
			//timer2.reset();
			Sort::bucketSort(*task_manager, axis_centres.begin(), working_space.begin(), num_objects, CenterItemPredicate(), CenterItemKey());
			//conPrint("sort: " + timer2.elapsedString());
		}

		for(int axis=0; axis<3; ++axis)
			task_manager->addTask(new SortAxisTask(*this, num_objects, axis, &all_axis_centres[axis]));
		task_manager->waitForTasksToComplete();
	}
	//conPrint("SortAxisTasks : " + timer.elapsedString());

	// Check we sorted objects along each axis properly:
#ifndef NDEBUG
	for(int axis=0; axis<3; ++axis)
		for(int i=1; i<num_objects; ++i)
		{
			const Ob& ob   = objects[axis][i];
			const Ob& prev = objects[axis][i-1];
			assert(ob.centre[axis] >= prev.centre[axis]);
		}
#endif



	// Reserve working space for each thread.
	const int initial_result_buf_reserve_cap = 2 * num_objects / (int)task_manager->getNumThreads();
	per_thread_temp_info.resize(task_manager->getNumThreads());
	for(size_t i=0; i<per_thread_temp_info.size(); ++i)
	{
		per_thread_temp_info[i].temp[0].resizeUninitialised(num_objects);
		per_thread_temp_info[i].temp[1].resizeUninitialised(num_objects);
		per_thread_temp_info[i].object_max.resizeUninitialised(num_objects);
		per_thread_temp_info[i].result_buf.reserve(initial_result_buf_reserve_cap);
	}

	// Alloc per_local_thread_temp_info.  This is used for per-axis computations, so there will be a max of 3 threads.
	if(num_objects >= axis_parallel_num_ob_threshold)
	{
		per_axis_thread_temp_info.resize(task_manager->getNumThreads());//NEW!
		for(size_t i=0; i<per_axis_thread_temp_info.size(); ++i)
		{
			per_axis_thread_temp_info[i].temp[0].resizeUninitialised(num_objects);
			per_axis_thread_temp_info[i].temp[1].resizeUninitialised(num_objects);
			per_axis_thread_temp_info[i].object_max.resizeUninitialised(num_objects);
		}
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
	result_chunks.resizeUninitialised(myMax(1, num_objects / new_task_num_ob_threshold));





	next_result_chunk++;

	Reference<BuildSubtreeTask> task = new BuildSubtreeTask(*this);
	task->begin = 0;
	task->end = num_objects;
	task->depth = 0;
	task->node_aabb = root_aabb;
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
	// Do a pass over the chunks to build chunk_root_node_indices and get the total number of nodes.
	const int num_result_chunks = (int)next_result_chunk;
	js::Vector<int, 16> final_chunk_offset(num_result_chunks); // Index (in combined/final array) of first node in chunk, for each chunk.
	size_t total_num_nodes = 0;

	for(size_t c=0; c<num_result_chunks; ++c)
	{
		//conPrint("Chunk " + toString(c) + ": offset=" + toString(result_chunks[c]->offset) + ", size=" + toString(result_chunks[c]->size) + ", thread_index=" + toString(result_chunks[c]->thread_index));
		final_chunk_offset[c] = (int)total_num_nodes;
		total_num_nodes += result_chunks[c].size;
		//conPrint("-------pre-merge Chunk " + toString(c) + "--------");
		//printResultNodes(per_thread_temp_info[result_chunks[c]->thread_index].result_buf);
	}

	result_nodes_out.resizeUninitialised(total_num_nodes);
	int write_index = 0;

	for(size_t c=0; c<num_result_chunks; ++c)
	{
		const ResultChunk& chunk = result_chunks[c];
		const int chunk_node_offset = final_chunk_offset[c] - chunk.offset;

		const js::Vector<ResultNode, 64>& chunk_nodes = per_thread_temp_info[chunk.thread_index].result_buf;

		for(size_t i=0; i<chunk.size; ++i)
		{
			const ResultNode& chunk_node = chunk_nodes[chunk.offset + i];
			result_nodes_out[write_index] = chunk_node; // Copy node to final array

			// If this is an interior node, we need to fix up some links.
			if(result_nodes_out[write_index].interior)
			{
				// Offset the child node indices by the offset of this chunk
				result_nodes_out[write_index].left += chunk_node_offset;

				const int chunk_index = result_nodes_out[write_index].right_child_chunk_index;

				assert(chunk_index == -1 || (chunk_index >= 0 && chunk_index < num_result_chunks));

				if(result_nodes_out[write_index].right_child_chunk_index != -1) // If the right child index refers to another chunk:
				{
					result_nodes_out[write_index].right = final_chunk_offset[chunk_index]; // Update it with the final node index.
				}
				else
					result_nodes_out[write_index].right += chunk_node_offset;

				assert(result_nodes_out[write_index].left < total_num_nodes);
				assert(result_nodes_out[write_index].right < total_num_nodes);
			}
			
			write_index++;
		}
	}

	//conPrint("Final merge elapsed: " + timer.elapsedString());


	result_indices.resize(num_objects);
	for(int i=0; i<num_objects; ++i)
		result_indices[i] = objects[0][i].index;
}


// Search along a given axis for the best split location, according to the SAH.
class BestSplitSearchTask : public Indigo::Task
{
public:
	BestSplitSearchTask(BVHBuilder& builder_) : builder(builder_) {}
	
	virtual void run(size_t thread_index)
	{
		const js::AABBox aabb = *node_aabb; // Put on stack to avoid potential repeated dereferencing.
		const js::Vector<Ob, 64>& axis_tris = builder.objects[axis];
		float* const use_object_max = builder.per_axis_thread_temp_info[thread_index].object_max.data();
		const float traversal_cost = 1.0f;
		const float aabb_surface_area = aabb.getSurfaceArea();
		const float recip_aabb_surf_area = 1.0f / aabb_surface_area;
		const float intersection_cost = builder.intersection_cost;

		const unsigned int nonsplit_axes[3][2] = {{1, 2}, {0, 2}, {0, 1}};

		// SAH stuff
		const unsigned int axis1 = nonsplit_axes[axis][0];
		const unsigned int axis2 = nonsplit_axes[axis][1];
		const float two_cap_area = (aabb.axisLength(axis1) * aabb.axisLength(axis2)) * 2.0f;
		const float circum = (aabb.axisLength(axis1) + aabb.axisLength(axis2)) * 2.0f;

		smallest_split_cost = std::numeric_limits<float>::infinity();
		best_N_L = -1;
		best_div_val = 0;


		// Go from left to right, Building the current max bound seen so far for tris 0...i
		float running_max = -std::numeric_limits<float>::infinity();
		for(int i=left; i<right; ++i)
		{
			running_max = myMax(running_max, axis_tris[i].aabb.max_[axis]);
			use_object_max[i-left] = running_max;
		}

		// For Each triangle centroid
		float running_min = axis_tris[right-1].aabb.min_[axis]; // Min value along axis of all AABBs assigned to right child node.
		float last_split_val = axis_tris[right-1].centre[axis];
		for(int i=right-2; i>=left; --i)
		{
			const float splitval = axis_tris[i].centre[axis];
			const float current_max = use_object_max[i-left];

			// Compute the SAH cost at the centroid position
			const int N_L = (i - left) + 1;
			const int N_R = (right - left) - N_L;
/*
#ifndef NDEBUG
			// Check that our AABB calculations are correct:
			float left_aabb_max = aabb.min_[axis];
			for(int t=left; t<=i; ++t)
				left_aabb_max = myMax(left_aabb_max, aabbs[axis_tris[t]].max_[axis]);

			float right_aabb_min = aabb.max_[axis];
			for(int t=i+1; t<right; ++t)
				right_aabb_min = myMin(right_aabb_min, aabbs[axis_tris[t]].min_[axis]);

			assert(left_aabb_max == current_max);
			assert(right_aabb_min == running_min);
#endif*/

			// Compute SAH cost
			const float negchild_surface_area = two_cap_area + (current_max - aabb.min_[axis]) * circum;
			const float poschild_surface_area = two_cap_area + (aabb.max_[axis] - running_min) * circum;

			const float cost = traversal_cost + ((float)N_L * negchild_surface_area + (float)N_R * poschild_surface_area) * 
				recip_aabb_surf_area * intersection_cost;

			if(cost < smallest_split_cost && splitval != last_split_val) // If this is the smallest cost so far, and this is the first such split position seen:
			{
				best_N_L = N_L;
				smallest_split_cost = cost;
				best_div_val = splitval;
			}
			last_split_val = splitval;
			running_min = myMin(running_min, axis_tris[i].aabb.min_[axis]);
		}
	}

	BVHBuilder& builder;
	int axis; // Axis to search along.
	const js::AABBox* node_aabb;
	int left;
	int right;

	// Results:
	float smallest_split_cost;
	int best_N_L;
	float best_div_val;
};


// Partition the object list for the given axis ('axis').
// This needs to be a stable partition. (preserve ordering)
class PartitionTask : public Indigo::Task
{
public:
	PartitionTask(BVHBuilder& builder_) : builder(builder_) {}
	
	virtual void run(size_t thread_index)
	{
		js::Vector<Ob, 64>& axis_obs = builder.objects[axis];
		js::Vector<Ob, 64>& temp_0 = builder.per_axis_thread_temp_info[thread_index].temp[0];
		js::Vector<Ob, 64>& temp_1 = builder.per_axis_thread_temp_info[thread_index].temp[1];

		int num_left_tris = 0;
		int num_right_tris = 0;

		for(int i=left; i<right; ++i)
		{
			if(axis_obs[i].centre[best_axis] <= best_div_val) // If on Left side
				temp_0[num_left_tris++] = axis_obs[i];
			else // else if on Right side
				temp_1[num_right_tris++] = axis_obs[i];
		}

		// Copy temp back to the tri list

		/*for(int z=0; z<num_left_tris; ++z)
			axis_obs[left + z] = temp_0[z];

		for(int z=0; z<num_right_tris; ++z)
			axis_obs[left + num_left_tris + z] = temp_1[z];*/
		std::memcpy(&axis_obs[left], &temp_0[0], sizeof(Ob) * num_left_tris);
		std::memcpy(&axis_obs[left + num_left_tris], &temp_1[0], sizeof(Ob) * num_right_tris);

		split_i = left + num_left_tris;
		assert(split_i >= left && split_i <= right);


		// Compute AABBs as well.  Task for axis 0 will compute left AABB, task for axis 1 will compute right AABB
		if(axis == 0)
		{
			js::AABBox aabb = js::AABBox::emptyAABBox();
			for(int z=0; z<num_left_tris; ++z)
				aabb.enlargeToHoldAABBox(temp_0[z].aabb);

			*left_aabb = aabb;
		}
		else if(axis == 1)
		{
			js::AABBox aabb = js::AABBox::emptyAABBox();
			for(int z=0; z<num_right_tris; ++z)
				aabb.enlargeToHoldAABBox(temp_1[z].aabb);

			*right_aabb = aabb;
		}
	}

	BVHBuilder& builder;
	int axis;
	int best_axis;
	float best_div_val;
	int left;
	int right;
	
	int split_i;

	js::AABBox* left_aabb;
	js::AABBox* right_aabb;
};


/*
Recursively build a subtree.
Assumptions: root node for subtree is already created and is at node_index
*/
void BVHBuilder::doBuild(
			PerThreadTempInfo& per_thread_temp_info,
			const js::AABBox& aabb,
			 uint32 node_index, 
			 int left, 
			 int right, 
			 int depth,
			 ResultChunk& result_chunk
			 )
{
	const int MAX_DEPTH = 60;

	js::Vector<ResultNode, 64>& chunk_nodes = per_thread_temp_info.result_buf;

	if(right - left <= leaf_num_object_threshold || depth >= MAX_DEPTH)
	{
		assert(right - left <= max_num_objects_per_leaf);

		// Make this a leaf node
		chunk_nodes[node_index].interior = false;
		chunk_nodes[node_index].left = left;
		chunk_nodes[node_index].right = right;

		// Update build stats
		if(depth >= MAX_DEPTH)
			num_maxdepth_leaves++;
		else
			num_under_thresh_leaves++;
		leaf_depth_sum += depth;
		max_leaf_depth = myMax(max_leaf_depth, depth);
		max_num_tris_per_leaf = myMax(max_num_tris_per_leaf, right - left);
		num_leaves++;
		return;
	}

	// Compute best split plane
	js::Vector<Vec4f, 16>& use_object_max = per_thread_temp_info.object_max;
	const float traversal_cost = 1.0f;
	const float aabb_surface_area = aabb.getSurfaceArea();
	const float recip_aabb_surf_area = 1.0f / aabb_surface_area;

	const unsigned int nonsplit_axes[3][2] = {{1, 2}, {0, 2}, {0, 1}};

	// Compute non-split cost
	const float non_split_cost = (float)(right - left) * intersection_cost;

	float smallest_split_cost = std::numeric_limits<float>::infinity();
	int best_N_L = -1;
	int best_axis = -1;
	float best_div_val = 0;

	// for each axis 0..2
	//conPrint("Looking for best split...");
	//Timer timer;

	// If there are enough objects to process, do in parallel, one task for each axis
	if((right - left) >= axis_parallel_num_ob_threshold)
	{
		Reference<BestSplitSearchTask> tasks[3];
		for(int axis = 0; axis < 3; ++axis)
		{
			tasks[axis] = new BestSplitSearchTask(*this);
			tasks[axis]->node_aabb = &aabb;
			tasks[axis]->axis = axis;
			tasks[axis]->left = left;
			tasks[axis]->right = right;
			local_task_manager->addTask(tasks[axis]);
		}

		local_task_manager->waitForTasksToComplete();

		for(int axis = 0; axis < 3; ++axis)
		{
			if(tasks[axis]->smallest_split_cost < smallest_split_cost)
			{
				best_N_L = tasks[axis]->best_N_L;
				smallest_split_cost = tasks[axis]->smallest_split_cost;
				best_axis = tasks[axis]->axis;
				best_div_val = tasks[axis]->best_div_val;
			}
		}
	}
	else
	{
		// Sweep over all the objects for this subtree, computing the max of the AABBs along each axis
		Vec4f running_max(-std::numeric_limits<float>::infinity());
		const js::Vector<Ob, 64>& axis_0_obs = objects[0];
		const js::Vector<Ob, 64>& axis_1_obs = objects[1];
		const js::Vector<Ob, 64>& axis_2_obs = objects[2];
		for(int i=left; i<right; ++i)
		{
			Vec4f m(axis_0_obs[i].aabb.max_[0], axis_1_obs[i].aabb.max_[1], axis_2_obs[i].aabb.max_[2], 0.f);
			running_max = max(running_max, m);
			use_object_max[i-left] = running_max;
		}

		for(unsigned int axis=0; axis<3; ++axis)
		{
			const js::Vector<Ob, 64>& axis_tris = objects[axis];

			// SAH stuff
			const unsigned int axis1 = nonsplit_axes[axis][0];
			const unsigned int axis2 = nonsplit_axes[axis][1];
			const float two_cap_area = (aabb.axisLength(axis1) * aabb.axisLength(axis2)) * 2.0f;
			const float circum = (aabb.axisLength(axis1) + aabb.axisLength(axis2)) * 2.0f;

			/*


			  |---------0--------|
			|-----------1----------|
			                   |---------------2---------------|
			                           |-------3-------|
			                                                 |----4----|   

			If we split at the midpoint of objects 0 and 1, then 0 and 1 are assigned to the left child node, and 2 is assigned to the right.
			The left aabb therefore has right bound max(aabb_max_0, aabb_max_1), and the right aabb has left bound min(aabb_min_2, aabb_min_3).
			Note here that running_max[0] = aabb_max_0, running_max[1] = max(aabb_max_0, aabb_max_1) = aabb_max_1.
			This means that for identical centres, it's the largest/rightmost running_max we should use (e.g. running_max[1] not running_max[0]).
			

			  |---------0--------|
			|-----------1----------|
			                           |-----2-----|
			                   |-------------3-------------|
			                 |--------------------4------------------|   


			*/

			// For Each triangle centroid
			float running_min = axis_tris[right-1].aabb.min_[axis]; // Min value along axis of all AABBs assigned to right child node.

			float last_split_val = axis_tris[right-1].centre[axis];

			for(int i=right-2; i>=left; --i) // Start with 1 object assigned to right child node, go down to 1 child assigned to left child node.
			{
				const float splitval = axis_tris[i].centre[axis];
				const float current_max = use_object_max[i-left].x[axis]; // Max value along axis of all AABBs assigned to left child node.

				// Compute the SAH cost at the centroid position
				const int N_L = (i - left) + 1;
				//const int N_R = (right - left) - N_L;
				const int N_R = right - i - 1;
				assert(N_L + N_R == right - left);

#ifndef NDEBUG
				// Check that our AABB calculations are correct:
				float left_aabb_max = aabb.min_[axis];
				for(int t=left; t<=i; ++t)
					left_aabb_max = myMax(left_aabb_max, axis_tris[t].aabb.max_[axis]);

				float right_aabb_min = aabb.max_[axis];
				for(int t=i+1; t<right; ++t)
					right_aabb_min = myMin(right_aabb_min, axis_tris[t].aabb.min_[axis]);

				assert(left_aabb_max == current_max);
				assert(right_aabb_min == running_min);
#endif

				// Compute SAH cost
				const float negchild_surface_area = two_cap_area + (current_max - aabb.min_[axis]) * circum;
				const float poschild_surface_area = two_cap_area + (aabb.max_[axis] - running_min) * circum;

				const float cost = traversal_cost + ((float)N_L * negchild_surface_area + (float)N_R * poschild_surface_area) * 
					recip_aabb_surf_area * intersection_cost;

				if(cost < smallest_split_cost && splitval != last_split_val) // If this is the smallest cost so far, and this is the first such split position seen:
				{
					best_N_L = N_L;
					smallest_split_cost = cost;
					best_axis = axis;
					best_div_val = splitval;
				}

				last_split_val = splitval;
				running_min = myMin(running_min, axis_tris[i].aabb.min_[axis]);
			}
		}
	}

	//conPrint("Looking for best split done.  elapsed: " + timer.elapsedString());

	if(best_axis == -1) // This can happen when all object centres coorindates along the axis are the same.
	{
		doArbitrarySplits(per_thread_temp_info, aabb, node_index, left, right, depth, result_chunk);
		return;
	}

	// If it is not advantageous to split, and the number of objects is <= the max num per leaf, then form a leaf:
	if((smallest_split_cost >= non_split_cost) && ((right - left) <= max_num_objects_per_leaf))
	{
		// If the least cost is to not split the node, then make this node a leaf node
		chunk_nodes[node_index].interior = false;
		chunk_nodes[node_index].left = left;
		chunk_nodes[node_index].right = right;

		// Update build stats
		num_cheaper_nosplit_leaves++;
		leaf_depth_sum += depth;
		max_leaf_depth = myMax(max_leaf_depth, depth);
		max_num_tris_per_leaf = myMax(max_num_tris_per_leaf, right - left);
		num_leaves++;
		return;
	}
		
	js::Vector<Ob, 64>& temp0 = per_thread_temp_info.temp[0];
	js::Vector<Ob, 64>& temp1 = per_thread_temp_info.temp[1];

	
	//Timer timer;
	//timer.reset();

	// Now we need to partition the triangle index lists, while maintaining ordering.
	// Compute AABBs for children as well
	js::AABBox left_aabb  = js::AABBox::emptyAABBox();
	js::AABBox right_aabb = js::AABBox::emptyAABBox();

	int split_i;
	if((right - left) >= axis_parallel_num_ob_threshold)
	{
		//conPrint("Partitioning with task (right - left = " + toString(right - left) + ") ...");

		Reference<PartitionTask> tasks[3];
		for(int axis = 0; axis < 3; ++axis)
		{
			tasks[axis] = new PartitionTask(*this);
			tasks[axis]->axis = axis;
			tasks[axis]->best_axis = best_axis;
			tasks[axis]->best_div_val = best_div_val;
			tasks[axis]->left = left;
			tasks[axis]->right = right;
			tasks[axis]->left_aabb = &left_aabb;
			tasks[axis]->right_aabb = &right_aabb;
			local_task_manager->addTask(tasks[axis]);
		}

		local_task_manager->waitForTasksToComplete();

		split_i = tasks[0]->split_i;
		assert(tasks[0]->split_i == tasks[1]->split_i && tasks[0]->split_i == tasks[2]->split_i);

		//conPrint("   Partitioning done.  elapsed: " + timer.elapsedString());
	}
	else
	{
		//if((right - left) >= axis_parallel_num_ob_threshold)
		//	conPrint("Partitioning without task (right - left = " + toString(right - left) + ") ...");

		for(int axis=0; axis<3; ++axis)
		{
			js::Vector<Ob, 64>& axis_obs = objects[axis];

			int num_left_tris  = 0;
			int num_right_tris = 0;

			for(int i=left; i<right; ++i)
			{
				if(axis_obs[i].centre[best_axis] <= best_div_val) // If on Left side
					temp0[num_left_tris++] = axis_obs[i];
				else // else if on Right side
					temp1[num_right_tris++] = axis_obs[i];
			}

			assert(num_left_tris == best_N_L);

			// Copy temp objects back to the object list
			std::memcpy(&axis_obs[left], &temp0[0], sizeof(Ob) * num_left_tris);
			std::memcpy(&axis_obs[left + num_left_tris], &temp1[0], sizeof(Ob) * num_right_tris);

			split_i = left + num_left_tris;
		}

		//if((right - left) >= axis_parallel_num_ob_threshold)
		//	conPrint("   Partitioning done.  elapsed: " + timer.elapsedString());

		// Compute AABBs for children
		//Timer timer;
		const js::Vector<Ob, 64>& axis0_obs = objects[0];
		
		// Spell out the min/maxing code here so that VS 2012 will generate optimal code, which it doesn't with enlargeToHoldAABBox.
		Vec4f box_min = left_aabb.min_;
		Vec4f box_max = left_aabb.max_;
		for(int i=left; i<split_i; ++i)
		{
			box_min = min(box_min, axis0_obs[i].aabb.min_);
			box_max = max(box_max, axis0_obs[i].aabb.max_);
			//left_aabb.enlargeToHoldAABBox(axis0_obs[i].aabb);
		}
		left_aabb = js::AABBox(box_min, box_max);
			
			

		box_min = right_aabb.min_;
		box_max = right_aabb.max_;
		for(int i=split_i; i<right; ++i)
		{
			box_min = min(box_min, axis0_obs[i].aabb.min_);
			box_max = max(box_max, axis0_obs[i].aabb.max_);
			//right_aabb.enlargeToHoldAABBox(axis0_obs[i].aabb);
		}
		right_aabb = js::AABBox(box_min, box_max);
			
		//if(right - left > 10000)
		//	conPrint("Computing child AABBs: " + timer.elapsedString());
	}

	
	const int num_left_tris = split_i - left;
	if(num_left_tris == 0 || num_left_tris == right - left)
	{
		if((right - left) <= max_num_objects_per_leaf)
		{
			chunk_nodes[node_index].interior = false;
			chunk_nodes[node_index].left = left;
			chunk_nodes[node_index].right = right;

			num_could_not_split_leaves++;
			leaf_depth_sum += depth;
			max_leaf_depth = myMax(max_leaf_depth, depth);
			max_num_tris_per_leaf = myMax(max_num_tris_per_leaf, right - left);
			num_leaves++;
			return;
		}
		else
		{
			// Split objects arbitrarily until the number in each leaf is <= max_num_objects_per_leaf.
			doArbitrarySplits(per_thread_temp_info, aabb, node_index, left, right, depth, result_chunk);
			return;
		}
	}


	//conPrint("Partitioning done.  elapsed: " + timer.elapsedString());
	//timer.reset();


	// Create child nodes
	const uint32 left_child = (uint32)chunk_nodes.size();
	chunk_nodes.push_back_uninitialised();

	
	const bool do_right_child_in_new_task = ((split_i - left) >= new_task_num_ob_threshold) && ((right - split_i) >= new_task_num_ob_threshold);// && !task_manager->areAllThreadsBusy();

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
	chunk_nodes[node_index].interior = true;
	chunk_nodes[node_index].left_aabb = left_aabb;
	chunk_nodes[node_index].right_aabb = right_aabb;
	chunk_nodes[node_index].left = left_child;
	chunk_nodes[node_index].right = right_child;
	chunk_nodes[node_index].right_child_chunk_index = -1;

	num_interior_nodes++;

	// Build left child
	doBuild(
		per_thread_temp_info,
		left_aabb, // aabb
		left_child, // node index
		left, // left
		split_i, // right
		depth + 1, // depth
		result_chunk
	);

	if(do_right_child_in_new_task)
	{
		// Add to chunk list
		chunk_nodes[node_index].right_child_chunk_index = (int)next_result_chunk++;
		assert((int)chunk_nodes[node_index].right_child_chunk_index < (int)result_chunks.size());
		
		// Put this subtree into a task
		//conPrint("Making new task...");
		Reference<BuildSubtreeTask> subtree_task = new BuildSubtreeTask(*this);
		subtree_task->result_chunk = &result_chunks[chunk_nodes[node_index].right_child_chunk_index];
		subtree_task->node_aabb = right_aabb;
		subtree_task->depth = depth + 1;
		subtree_task->begin = split_i;
		subtree_task->end = right;
		task_manager->addTask(subtree_task);
	}
	else
	{
		// Build right child
		doBuild(
			per_thread_temp_info,
			right_aabb, // aabb
			right_child, // node index
			split_i, // left
			right, // right
			depth + 1, // depth
			result_chunk
		);
	}
}


// We may get multiple objects with the same bounding box.
// These objects can't be split in the usual way.
// Also the number of such objects assigned to a subtree may be > max_num_objects_per_leaf.
// In this case we will just divide the object list into two until the num per subtree is <= max_num_objects_per_leaf.
void BVHBuilder::doArbitrarySplits(
		PerThreadTempInfo& per_thread_temp_info,
		const js::AABBox& aabb,
		uint32 node_index, 
		int left, 
		int right, 
		int depth,
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

	js::Vector<ResultNode, 64>& chunk_nodes = per_thread_temp_info.result_buf;

	if(right - left <= max_num_objects_per_leaf)
	{
		// Make this a leaf node
		chunk_nodes[node_index].interior = false;
		chunk_nodes[node_index].left = left;
		chunk_nodes[node_index].right = right;
		
		// Update build stats
		num_arbitrary_split_leaves++;
		leaf_depth_sum += depth;
		max_leaf_depth = myMax(max_leaf_depth, depth);
		max_num_tris_per_leaf = myMax(max_num_tris_per_leaf, right - left);
		num_leaves++;
		return;
	}

	const int split_i = left + (right - left)/2;


	// Compute AABBs for children
	js::AABBox left_aabb  = js::AABBox::emptyAABBox();
	js::AABBox right_aabb = js::AABBox::emptyAABBox();

	for(int i=left; i<split_i; ++i)
		left_aabb.enlargeToHoldAABBox(objects[0][i].aabb);

	for(int i=split_i; i<right; ++i)
		right_aabb.enlargeToHoldAABBox(objects[0][i].aabb);

	// Create child nodes
	const uint32 left_child  = (uint32)chunk_nodes.size();
	const uint32 right_child = (uint32)chunk_nodes.size() + 1;
	chunk_nodes.push_back_uninitialised();
	chunk_nodes.push_back_uninitialised();

	// Create interior node
	chunk_nodes[node_index].interior = true;
	chunk_nodes[node_index].left_aabb = left_aabb;
	chunk_nodes[node_index].right_aabb = right_aabb;
	chunk_nodes[node_index].left = left_child;
	chunk_nodes[node_index].right = right_child;
	chunk_nodes[node_index].right_child_chunk_index = -1;

	num_interior_nodes++;

	// TODO: Task splitting here as well?

	// Build left child
	doArbitrarySplits(
		per_thread_temp_info,
		left_aabb, // aabb
		left_child, // node index
		left, // left
		split_i, // right
		depth + 1, // depth
		result_chunk
	);

	// Build right child
	doArbitrarySplits(
		per_thread_temp_info,
		right_aabb, // aabb
		right_child, // node index
		split_i, // left
		right, // right
		depth + 1, // depth
		result_chunk
	);
}


void BVHBuilder::printResultNodes(const js::Vector<ResultNode, 64>& result_nodes)
{
	for(size_t i=0; i<result_nodes.size(); ++i)
	{
		conPrintStr("node " + toString(i) + ": ");
		if(result_nodes[i].interior)
			conPrint(" Interior");
		else
			conPrint(" Leaf");

		if(result_nodes[i].interior)
		{
			conPrint("	left_child_index:  " + toString(result_nodes[i].left));
			conPrint("	right_child_index: " + toString(result_nodes[i].right));
			//if(result_nodes[i].right_child_chunk_index == 0)
			conPrint("	right_child_chunk_index: " + toString(result_nodes[i].right_child_chunk_index));
		}
		else
		{
			conPrint("	objects_begin: " + toString(result_nodes[i].left));
			conPrint("	objects_end:   " + toString(result_nodes[i].right));
		}
	}
}
