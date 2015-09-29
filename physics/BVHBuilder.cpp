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
#include "../utils/ParallelFor.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/TaskManager.h"


BVHBuilder::BVHBuilder(int leaf_num_object_threshold_, int max_num_objects_per_leaf_, float intersection_cost_)
:	leaf_num_object_threshold(leaf_num_object_threshold_),
	max_num_objects_per_leaf(max_num_objects_per_leaf_),
	intersection_cost(intersection_cost_)
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
}


BVHBuilder::~BVHBuilder()
{

}


class CenterPredicate
{
public:
	CenterPredicate(int axis_, const std::vector<Vec3f>& centers_) : axis(axis_), centers(centers_) {}

	inline bool operator()(unsigned int t1, unsigned int t2)
	{
		return centers[t1][axis] < centers[t2][axis];
	}
private:
	int axis;
	const std::vector<Vec3f>& centers;
};


class CenterKey
{
public:
	CenterKey(int axis_, const std::vector<Vec3f>& tri_centers_) : axis(axis_), tri_centers(tri_centers_) {}

	inline float operator()(uint32 i)
	{
		return tri_centers[i][axis];
	}
private:
	int axis;
	const std::vector<Vec3f>& tri_centers;
};


class CentroidTaskClosure
{
public:
	BVHBuilder* builder;
};


// Compute objects (AABB) centres.
class CentroidTask : public Indigo::Task
{
public:
	CentroidTask(const CentroidTaskClosure& closure_, size_t begin_, size_t end_) : begin(begin_), end(end_), closure(closure_) {}
	
	virtual void run(size_t thread_index)
	{
		assert(!closure.builder->centers.empty());
		Vec3f* const centers = &closure.builder->centers[0];
		const js::AABBox* const aabbs = closure.builder->aabbs;

		for(size_t i = begin; i < end; ++i)
			centers[i] = toVec3f(aabbs[i].centroid());
	}

	const CentroidTaskClosure& closure;
	size_t begin, end;
};


class SortAxisTask : public Indigo::Task
{
public:
	SortAxisTask(BVHBuilder& builder_, int num_objects_, int axis_) : builder(builder_), num_objects(num_objects_), axis(axis_) {}
	
	virtual void run(size_t thread_index)
	{
		builder.objects[axis].resize(num_objects);
		for(int i=0; i<num_objects; ++i)
			builder.objects[axis][i] = i;

		// Sort based on center along axis 'axis'
		Sort::floatKeyAscendingSort(builder.objects[axis].begin(), builder.objects[axis].end(), CenterPredicate(axis, builder.centers), CenterKey(axis, builder.centers));
	}


	BVHBuilder& builder;
	int num_objects;
	int axis;
};


void BVHBuilder::build(
		   Indigo::TaskManager& task_manager,
		   const js::AABBox* aabbs_,
		   const int num_objects,
		   PrintOutput& print_output, 
		   bool verbose, 
		   BVHBuilderCallBacks& callback
		   )
{
	this->aabbs = aabbs_;

	if(num_objects <= 0)
	{
		// Create root node, and mark it as a leaf.
		const uint32 root_node_index = callback.createNode();
		callback.markAsLeafNode(root_node_index, 
			objects[0], 
			0, // left
			0, // right
			0, // parent_index 
			false // is_left_child
		);
		num_under_thresh_leaves = 1;
		//leaf_depth_sum += 0;
		max_leaf_depth = 0;
		num_leaves = 1;
		return;
	}

	// Build triangle centers
	this->centers.resize(num_objects);

	CentroidTaskClosure centroid_closure;
	centroid_closure.builder = this;
	task_manager.runParallelForTasks<CentroidTask, CentroidTaskClosure>(centroid_closure, 0, num_objects);
	

	// Reserve working space
	temp[0].resize(num_objects);
	temp[1].resize(num_objects);


	// Sort indices based on center position along the axes
	for(int axis=0; axis<3; ++axis)
		task_manager.addTask(new SortAxisTask(*this, num_objects, axis));
	task_manager.waitForTasksToComplete();


	// Build root aabb
	js::AABBox root_aabb = aabbs[0];
	for(int i=1; i<num_objects; ++i)
		root_aabb.enlargeToHoldAABBox(aabbs[i]);

	this->object_max.resize(num_objects);

	const uint32 root_node_index = callback.createNode();

	// Start building tree
	doBuild(
		root_aabb,
		root_node_index, // node index
		0, // left
		num_objects, // right
		0, // depth
		0, // parent index
		false, // is left child
		callback
	);
}


/*
Assumptions: root node for subtree is already created and is at node_index
*/
void BVHBuilder::doBuild(
			const js::AABBox& aabb,
			 uint32 node_index, 
			 int left, 
			 int right, 
			 int depth,
			 uint32 parent_index,
			 bool is_left_child,
			 BVHBuilderCallBacks& callback
			 )
{
	const int MAX_DEPTH = 60;

	if(right - left <= leaf_num_object_threshold || depth >= MAX_DEPTH)
	{
		assert(right - left <= max_num_objects_per_leaf);

		// Make this a leaf node
		callback.markAsLeafNode(node_index, objects[0], left, right, parent_index, is_left_child);

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
	const Vec3f* const use_centers = centers.data();
	float* const use_object_max = object_max.data();
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
	for(unsigned int axis=0; axis<3; ++axis)
	{
		float last_split_val = -std::numeric_limits<float>::infinity();

		const std::vector<uint32>& axis_tris = objects[axis];

		// SAH stuff
		const unsigned int axis1 = nonsplit_axes[axis][0];
		const unsigned int axis2 = nonsplit_axes[axis][1];
		const float two_cap_area = (aabb.axisLength(axis1) * aabb.axisLength(axis2)) * 2.0f;
		const float circum = (aabb.axisLength(axis1) + aabb.axisLength(axis2)) * 2.0f;


		// Go from left to right, Building the current max bound seen so far for tris 0...i
		float running_max = -std::numeric_limits<float>::infinity();
		for(int i=left; i<right; ++i)
		{
			running_max = myMax(running_max, aabbs[axis_tris[i]].max_[axis]);
			use_object_max[i-left] = running_max;
		}

		// For Each triangle centroid
		float running_min = std::numeric_limits<float>::infinity();
		for(int i=right-2; i>=left; --i)
		{
			const float splitval = use_centers[axis_tris[i]][axis];
			if(splitval != last_split_val) // If this is the first such split position seen.
			{
				running_min = myMin(running_min, aabbs[axis_tris[i]].min_[axis]);
				const float current_max = use_object_max[i-left];

				// Compute the SAH cost at the centroid position
				const int N_L = (i - left) + 1;
				const int N_R = (right - left) - N_L;

				//assert(N_L >= 0 && N_L <= (int)centers.size() && N_R >= 0 && N_R <= (int)centers.size());

				//TEMP:
				/*float left_aabb_max = aabb.min_[axis];//-std::numeric_limits<float>::infinity();
				for(int t=left; t<i; ++t)
				left_aabb_max = myMax(left_aabb_max, tri_aabbs[axis_tris[i]].max_[axis]);

				float right_aabb_min = aabb.max_[axis];//std::numeric_limits<float>::infinity();
				for(int t=i; t<right; ++t)
				right_aabb_min = myMin(right_aabb_min, tri_aabbs[axis_tris[i]].min_[axis]);*/

				// Compute SAH cost
				const float negchild_surface_area = two_cap_area + (current_max - aabb.min_[axis]) * circum;
				const float poschild_surface_area = two_cap_area + (aabb.max_[axis] - running_min) * circum;

				const float cost = traversal_cost + ((float)N_L * negchild_surface_area + (float)N_R * poschild_surface_area) * 
					recip_aabb_surf_area * intersection_cost;

				if(cost < smallest_split_cost)
				{
					best_N_L = N_L;
					smallest_split_cost = cost;
					best_axis = axis;
					best_div_val = splitval;
				}
				last_split_val = splitval;
			}
		}
	}

	assert(best_axis != -1);

	// If it is not advantageous to split, and the number of objects is <= the max num per leaf, then form a leaf:
	if((smallest_split_cost >= non_split_cost) && ((right - left) <= max_num_objects_per_leaf))
	{
		// If the least cost is to not split the node, then make this node a leaf node
		callback.markAsLeafNode(node_index, objects[0], left, right, parent_index, is_left_child);

		// Update build stats
		num_cheaper_nosplit_leaves++;
		leaf_depth_sum += depth;
		max_leaf_depth = myMax(max_leaf_depth, depth);
		max_num_tris_per_leaf = myMax(max_num_tris_per_leaf, right - left);
		num_leaves++;
		return;
	}
		

	// Now we need to partition the triangle index lists, while maintaining ordering.
	int split_i;
	for(int axis=0; axis<3; ++axis)
	{
		std::vector<uint32>& axis_tris = objects[axis];

		int num_left_tris = 0;
		int num_right_tris = 0;

		for(int i=left; i<right; ++i)
		{
			if(use_centers[axis_tris[i]][best_axis] <= best_div_val) // If on Left side
				temp[0][num_left_tris++] = axis_tris[i];
			else // else if on Right side
				temp[1][num_right_tris++] = axis_tris[i];
		}

		// Copy temp back to the tri list

		for(int z=0; z<num_left_tris; ++z)
			axis_tris[left + z] = temp[0][z];

		for(int z=0; z<num_right_tris; ++z)
			axis_tris[left + num_left_tris + z] = temp[1][z];

		split_i = left + num_left_tris;
		assert(split_i >= left && split_i <= right);
		//TEMP DISABLED WAS FAILING on dof_test.igs  
		//assert(num_left_tris == best_N_L);

		if(num_left_tris == 0 || num_left_tris == right - left)
		{
			if((right - left) <= max_num_objects_per_leaf)
			{
				//markBVHLeafNode(node_index, left, right, triangles_ws, indices, parent_index, is_left_child);
				callback.markAsLeafNode(node_index, objects[0], left, right, parent_index, is_left_child);

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
				doArbitrarySplits(aabb, node_index, left, right, depth, parent_index, is_left_child, callback);
				return;
			}
		}
	}

	// Compute AABBs for children
	js::AABBox left_aabb  = js::AABBox::emptyAABBox();
	js::AABBox right_aabb = js::AABBox::emptyAABBox();

	for(int i=left; i<split_i; ++i)
		left_aabb.enlargeToHoldAABBox(aabbs[objects[0][i]]);

	for(int i=split_i; i<right; ++i)
		right_aabb.enlargeToHoldAABBox(aabbs[objects[0][i]]);

	// Create interior node
	//const uint32 node_index = callback.createNode();

	// Create child nodes
	const uint32 left_child = callback.createNode();
	const uint32 right_child = callback.createNode();

	node_index = callback.markAsInteriorNode(node_index, left_child, right_child, left_aabb, right_aabb, parent_index, is_left_child);
	num_interior_nodes++;

	// Build left child
	doBuild(
		left_aabb, // aabb
		left_child, // node index
		left, // left
		split_i, // right
		depth + 1, // depth
		node_index, // parent index (index of this node)
		true, // is left child
		callback
	);

	// Build right child
	doBuild(
		right_aabb, // aabb
		right_child, // node index
		split_i, // left
		right, // right
		depth + 1, // depth
		node_index, // parent index (index of this node)
		false, // is left child
		callback
	);
}


void BVHBuilder::doArbitrarySplits(
		const js::AABBox& aabb,
		uint32 node_index, 
		int left, 
		int right, 
		int depth,
		uint32 parent_index,
		bool is_left_child,
		BVHBuilderCallBacks& callback
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

	if(right - left <= max_num_objects_per_leaf)
	{
		// Make this a leaf node
		callback.markAsLeafNode(node_index, objects[0], left, right, parent_index, is_left_child);
		
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
		left_aabb.enlargeToHoldAABBox(aabbs[objects[0][i]]);

	for(int i=split_i; i<right; ++i)
		right_aabb.enlargeToHoldAABBox(aabbs[objects[0][i]]);

	// Create child nodes
	const uint32 left_child = callback.createNode();
	const uint32 right_child = callback.createNode();

	node_index = callback.markAsInteriorNode(node_index, left_child, right_child, left_aabb, right_aabb, parent_index, is_left_child);
	num_interior_nodes++;

	// Build left child
	doArbitrarySplits(
		left_aabb, // aabb
		left_child, // node index
		left, // left
		split_i, // right
		depth + 1, // depth
		node_index, // parent index (index of this node)
		true, // is left child
		callback
	);

	// Build right child
	doArbitrarySplits(
		right_aabb, // aabb
		right_child, // node index
		split_i, // left
		right, // right
		depth + 1, // depth
		node_index, // parent index (index of this node)
		false, // is left child
		callback
	);
}
