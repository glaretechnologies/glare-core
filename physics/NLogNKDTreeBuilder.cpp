/*=====================================================================
NLogNKDTreeBuilder.cpp
--------------------
File created by ClassTemplate on Sun Mar 23 23:42:55 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "NLogNKDTreeBuilder.h"

#include <algorithm>
#include "../graphics/TriBoxIntersection.h"
#include "../indigo/globals.h"
#include "../utils/StringUtils.h"
#include "../indigo/PrintOutput.h"
#include "../indigo/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/Sort.h"
#include "../utils/ParallelFor.h"
#include <algorithm>


namespace js
{

//#define CLIP_TRIANGLES 1
const static bool DO_EMPTY_SPACE_CUTOFF = true;


NLogNKDTreeBuilder::NLogNKDTreeBuilder()
{
}


NLogNKDTreeBuilder::~NLogNKDTreeBuilder()
{

}


class LowerPred
{
public:
	inline bool operator()(const NLogNKDTreeBuilder::LowerBound& a, const NLogNKDTreeBuilder::LowerBound& b)
	{
	   return a.lower < b.lower;
	}
};


class LowerKey
{
public:
	inline float operator()(const NLogNKDTreeBuilder::LowerBound& bound)
	{
		return bound.lower;
	}
};


class UpperPred
{
public:
	inline bool operator()(const NLogNKDTreeBuilder::UpperBound& a, const NLogNKDTreeBuilder::UpperBound& b)
	{
	   return a.upper < b.upper;
	}
};


class UpperKey
{
public:
	inline float operator()(const NLogNKDTreeBuilder::UpperBound& bound)
	{
		return bound.upper;
	}
};


class SortAxisTask
{
public:
	SortAxisTask(NLogNKDTreeBuilder::LayerInfo& root_layer_) : root_layer(root_layer_) {}
	
	void operator() (int axis, int thread_index) const
	{
		Sort::floatKeyAscendingSort(root_layer.lower_bounds[axis].begin(), root_layer.lower_bounds[axis].end(), LowerPred(), LowerKey());
		Sort::floatKeyAscendingSort(root_layer.upper_bounds[axis].begin(), root_layer.upper_bounds[axis].end(), UpperPred(), UpperKey());
	}

	NLogNKDTreeBuilder::LayerInfo& root_layer;
};


void NLogNKDTreeBuilder::build(PrintOutput& print_output, bool verbose, KDTree& tree, const AABBox& root_aabb, KDTree::NODE_VECTOR_TYPE& nodes_out, KDTree::LEAF_GEOM_ARRAY_TYPE& leaf_tri_indices_out)
{
	unsigned int max_depth = tree.calcMaxDepth();

	Timer timer;
	// Build triangle AABBs
	tri_aabbs.resize(tree.numTris());
	for(unsigned int i=0; i<tree.numTris(); ++i)
	{
		tri_aabbs[i].min_ = tree.triVertPos(i, 0).toVec4fPoint();
		tri_aabbs[i].max_ = tree.triVertPos(i, 0).toVec4fPoint();
		tri_aabbs[i].enlargeToHoldPoint(tree.triVertPos(i, 1).toVec4fPoint());
		tri_aabbs[i].enlargeToHoldPoint(tree.triVertPos(i, 2).toVec4fPoint());
	}

	//print_output.print("Building tri aabbs took " + timer.elapsedString());
	//timer.reset();

	layers.resize(max_depth + 1);

	// Initialise upper and lower bounds
	for(int ax=0; ax<3; ++ax)
	{
		layers[0].lower_bounds[ax].resize(tree.numTris());
		layers[0].upper_bounds[ax].resize(tree.numTris());

		for(unsigned int i=0; i<tree.numTris(); ++i)
		{
			(layers[0].lower_bounds[ax])[i].lower = tri_aabbs[i].min_[ax];
			(layers[0].lower_bounds[ax])[i].tri_index = i;
			(layers[0].upper_bounds[ax])[i].upper = tri_aabbs[i].max_[ax];
			(layers[0].upper_bounds[ax])[i].tri_index = i;
		}
	}

	//print_output.print("Building layers took " + timer.elapsedString());
	timer.reset();

	// Sort bounds
	/*#ifndef INDIGO_NO_OPENMP
	#pragma omp parallel for
	#endif
	for(int axis=0; axis<3; ++axis)
	{
		Sort::floatKeyAscendingSort(layers[0].lower_bounds[axis].begin(), layers[0].lower_bounds[axis].end(), LowerPred(), LowerKey());
		Sort::floatKeyAscendingSort(layers[0].upper_bounds[axis].begin(), layers[0].upper_bounds[axis].end(), UpperPred(), UpperKey());
	}*/
	ParallelFor::exec<SortAxisTask>(SortAxisTask(layers[0]), 0, 3);

	if(verbose) print_output.print("\t\tSort took " + timer.elapsedString());

	doBuild(
		print_output,
		verbose,
		tree,
		KDTree::ROOT_NODE_INDEX,
		0,
		max_depth,
		root_aabb, 
		nodes_out,
		leaf_tri_indices_out
		);
}


void NLogNKDTreeBuilder::doBuild(
						PrintOutput& print_output, 
						bool verbose, 
						KDTree& tree, 
						unsigned int cur, // index of current node getting built
						unsigned int depth, // depth of current node
						unsigned int maxdepth, // max permissible depth
						const AABBox& cur_aabb, // AABB of current node
						std::vector<KDTreeNode>& nodes,
						KDTree::LEAF_GEOM_ARRAY_TYPE& leaf_tri_indices_out
						)
{
	// Get the list of tris intersecting this volume
	//assert(depth < (unsigned int)node_tri_layers.size());
	//const std::vector<TriInfo>& nodetris = node_tri_layers[depth];
	Timer timer;//TEMP

	LayerInfo& layer = this->layers[depth];

#ifdef _DEBUG
	const size_t num_lowers = layer.lower_bounds[0].size();

	// Check all our bounds are sorted correctly
	for(int axis=0; axis<3; ++axis)
	{
		assert(layer.lower_bounds[axis].size() == num_lowers);
		assert(layer.upper_bounds[axis].size() == num_lowers);

		for(size_t z=1; z<layer.lower_bounds[axis].size(); ++z)
		{
			assert((layer.lower_bounds[axis])[z-1].lower <= (layer.lower_bounds[axis])[z].lower);
			assert((layer.upper_bounds[axis])[z-1].upper <= (layer.upper_bounds[axis])[z].upper);
		}
	}
#endif


	assert(cur < (int)nodes.size());

	// Print progress message
	if(depth == 5)
	{
		if(verbose)	print_output.print("\t" + ::toString(tree.numnodesbuilt) + "/" + ::toString(1 << 5) + " nodes at depth 5 built.");
		tree.numnodesbuilt++;
	}

	const unsigned int numtris = (unsigned int)layer.lower_bounds[0].size();

	//------------------------------------------------------------------------
	//test for termination of splitting
	//------------------------------------------------------------------------
	const int SPLIT_THRESHOLD = 1;
	if(numtris <= SPLIT_THRESHOLD || depth >= maxdepth || cur_aabb.getSurfaceArea() == 0.0f)
	{
		// Make this node a leaf node.
		nodes[cur] = KDTreeNode(
			(unsigned int)leaf_tri_indices_out.size(), // Leaf geom index
			numtris // num leaf geom
			);

		for(unsigned int i=0; i<numtris; ++i)
			leaf_tri_indices_out.push_back((layer.lower_bounds[0])[i].tri_index); //nodetris[i].tri_index);

		// Record stats
		if(depth >= maxdepth)
			tree.num_maxdepth_leafs++;
		else
			tree.num_under_thresh_leafs++;
		return;
	}

	// SAH cost constants
	const float traversal_cost = 1.0f;
	const float intersection_cost = 4.0f;

	const float aabb_surface_area = cur_aabb.getSurfaceArea();
	const float recip_aabb_surf_area = 1.0f / aabb_surface_area;

	int best_axis = -1;
	float best_div_val = -std::numeric_limits<float>::max();
	int best_num_in_neg = 0;
	int best_num_in_pos = 0;
	//int best_num_on_split_plane = 0;
	bool best_push_right = false;

	float best_cutoff_frac = 0.f;
	float best_cutoff_splitval = -666.0f;
	int best_cutoff_axis = -1;
	bool best_cutoff_push_right = false;

	//------------------------------------------------------------------------
	//compute non-split cost
	//------------------------------------------------------------------------
	float smallest_cost = (float)numtris * intersection_cost;

	const unsigned int nonsplit_axes[3][2] = {{1, 2}, {0, 2}, {0, 1}};

	//------------------------------------------------------------------------
	//Find Best axis and split plane value
	//------------------------------------------------------------------------
	for(int axis=0; axis<3; ++axis)
	{
		const js::Vector<LowerBound, 4>& lower = layer.lower_bounds[axis];
		const js::Vector<UpperBound, 4>& upper = layer.upper_bounds[axis];

		const unsigned int axis1 = nonsplit_axes[axis][0];
		const unsigned int axis2 = nonsplit_axes[axis][1];
		const float two_cap_area = (cur_aabb.axisLength(axis1) * cur_aabb.axisLength(axis2)) * 2.0f;
		const float circum = (cur_aabb.axisLength(axis1) + cur_aabb.axisLength(axis2)) * 2.0f;

		if(cur_aabb.axisLength(axis) == 0.0f)
			continue; // Don't try to split a zero-width bounding box

		// Try explicit 'Early empty-space cutoff', see section 4.4 of Havran's thesis
		// 'Construction of Kd-Trees with Utilization of Empty Spatial Regions'
		if(DO_EMPTY_SPACE_CUTOFF)
		{
			const float lower_cutoff_frac = myClamp((lower[0].lower - cur_aabb.min_.x[axis]) / cur_aabb.axisLength(axis), 0.f, 1.f);
			const float upper_cutoff_frac = myClamp((cur_aabb.max_.x[axis] - upper[numtris-1].upper) / cur_aabb.axisLength(axis), 0.f, 1.f);
			assert(Maths::inRange(lower_cutoff_frac, 0.0f, 1.0f));
			assert(Maths::inRange(upper_cutoff_frac, 0.0f, 1.0f));
			if(lower_cutoff_frac > best_cutoff_frac)
			{
				best_cutoff_frac = lower_cutoff_frac;
				best_cutoff_splitval = lower[0].lower;
				best_cutoff_axis = axis;
				best_cutoff_push_right = true;
			}
			if(upper_cutoff_frac > best_cutoff_frac)
			{
				best_cutoff_frac = upper_cutoff_frac;
				best_cutoff_splitval = upper[numtris-1].upper;
				best_cutoff_axis = axis;
				best_cutoff_push_right = false;
			}
		}

		unsigned int upper_index = 0;
		//Index of first triangle in upper volume
		//Index of first tri with upper bound >= then current split val.
		//Also the number of tris with upper bound <= current split val.
		//which is the number of tris *not* in pos volume

		float last_splitval = -std::numeric_limits<float>::max();
		for(unsigned int i=0; i<numtris; ++i)
		{
			const float splitval = lower[i].lower;
			//assert(splitval >= cur_aabb.min_.x[axis] && splitval <= cur_aabb.max_.x[axis]);

			if(splitval != last_splitval) // Only consider first tri seen with a given lower bound.
			{
				if(splitval >= cur_aabb.min_.x[axis] && splitval <= cur_aabb.max_.x[axis]) // If split val is actually in AABB
				{

				// Get number of tris on splitting plane
				int num_on_splitting_plane = 0;
				for(unsigned int z=i; z<numtris && lower[z].lower == splitval; ++z) // For all other triangles that share the current splitting plane as a lower bound
					if(this->tri_aabbs[lower[z].tri_index].max_[axis]/*lower[z].upper*/ == splitval) // If the tri has zero extent along the current axis
						num_on_splitting_plane++; // Then it lies on the splitting plane.

				while(upper_index < numtris && upper[upper_index].upper <= splitval)
					upper_index++;

				assert(upper_index == numtris || upper[upper_index].upper > splitval);

				const int num_in_neg = i;
				const int num_in_pos = numtris - upper_index;
				assert(num_in_neg >= 0 && num_in_neg <= (int)numtris);
				assert(num_in_pos >= 0 && num_in_pos <= (int)numtris);
				assert(num_in_neg + num_in_pos + num_on_splitting_plane >= (int)numtris);


				const float negchild_surface_area = two_cap_area + (splitval - cur_aabb.min_.x[axis]) * circum;
				const float poschild_surface_area = two_cap_area + (cur_aabb.max_.x[axis] - splitval) * circum;
				assert(negchild_surface_area >= 0.f && negchild_surface_area <= aabb_surface_area * (1.0f + NICKMATHS_EPSILON));
				assert(poschild_surface_area >= 0.f && negchild_surface_area <= aabb_surface_area * (1.0f + NICKMATHS_EPSILON));
				assert(::epsEqual(negchild_surface_area + poschild_surface_area - two_cap_area, aabb_surface_area, aabb_surface_area * 1.0e-6f));

				if(num_on_splitting_plane == 0)
				{
					const float cost = traversal_cost + ((float)num_in_neg * negchild_surface_area + (float)num_in_pos * poschild_surface_area) *
						recip_aabb_surf_area * intersection_cost;
					assert(cost >= 0.0);

					if(cost < smallest_cost)
					{
						smallest_cost = cost;
						best_axis = axis;
						best_div_val = splitval;
						best_num_in_neg = num_in_neg;
						best_num_in_pos = num_in_pos;
						//best_num_on_split_plane = 0;
					}
				}
				else
				{
					// Try pushing tris on splitting plane to left
					const float push_left_cost = traversal_cost + ((float)(num_in_neg + num_on_splitting_plane) * negchild_surface_area + (float)num_in_pos * poschild_surface_area) *
						recip_aabb_surf_area * intersection_cost;
					assert(push_left_cost >= 0.0);

					if(push_left_cost < smallest_cost)
					{
						smallest_cost = push_left_cost;
						best_axis = axis;
						best_div_val = splitval;
						best_num_in_neg = num_in_neg + num_on_splitting_plane;
						best_num_in_pos = num_in_pos;
						//best_num_on_split_plane = 0;
						best_push_right = false;
					}

					// Try pushing tris on splitting plane to right
					const float push_right_cost = traversal_cost + ((float)num_in_neg * negchild_surface_area + (float)(num_in_pos + num_on_splitting_plane) * poschild_surface_area) *
						recip_aabb_surf_area * intersection_cost;
					assert(push_right_cost >= 0.0);

					if(push_right_cost < smallest_cost)
					{
						smallest_cost = push_right_cost;
						best_axis = axis;
						best_div_val = splitval;
						best_num_in_neg = num_in_neg;
						best_num_in_pos = num_in_pos + num_on_splitting_plane;
						//best_num_on_split_plane = 0;
						best_push_right = true;
					}
				}

				}

				last_splitval = splitval;
			}
		}
		unsigned int lower_index = 0;//index of first tri with lower bound >= current split val
		//Also the number of tris that are in the negative volume

		for(unsigned int i=0; i<numtris; ++i)
		{
			// Advance to greatest index with given upper bound
			while(i+1 < numtris && upper[i].upper == upper[i+1].upper) // While triangle i has some upper bound as triangle i+1
				++i;


			const float splitval = upper[i].upper;
			//assert(splitval >= cur_aabb.min_.x[axis] && splitval <= cur_aabb.max_.x[axis]);

			if(splitval >= cur_aabb.min_.x[axis] && splitval <= cur_aabb.max_.x[axis])
			{

			int num_on_splitting_plane = 0;
			for(int z=i; z>=0 && upper[z].upper == splitval; --z) // For each triangle sharing an upper bound with the current triangle
				if(this->tri_aabbs[upper[z].tri_index].min_[axis]/*upper[z].lower*/ == splitval) // If tri has zero extent along this axis
					num_on_splitting_plane++;

			while(lower_index < numtris && lower[lower_index].lower < splitval)
				lower_index++;

			// Postcondition:
			assert(lower_index == numtris || lower[lower_index].lower >= splitval);

			const int num_in_neg = lower_index;
			const int num_in_pos = numtris - (i + 1);
			assert(num_in_neg >= 0 && num_in_neg <= (int)numtris);
			assert(num_in_pos >= 0 && num_in_pos <= (int)numtris);
			assert(num_in_neg + num_in_pos + num_on_splitting_plane >= (int)numtris);

			//const int num_on_splitting_plane = numtris - (num_in_neg + num_in_pos);

			const float negchild_surface_area = two_cap_area + (splitval - cur_aabb.min_.x[axis]) * circum;
			const float poschild_surface_area = two_cap_area + (cur_aabb.max_.x[axis] - splitval) * circum;
			assert(negchild_surface_area >= 0.f && negchild_surface_area <= aabb_surface_area * (1.0f + NICKMATHS_EPSILON));
			assert(poschild_surface_area >= 0.f && negchild_surface_area <= aabb_surface_area * (1.0f + NICKMATHS_EPSILON));
			assert(::epsEqual(negchild_surface_area + poschild_surface_area - two_cap_area, aabb_surface_area, aabb_surface_area * 1.0e-6f));

			if(num_on_splitting_plane == 0)
			{
				const float cost = traversal_cost + ((float)num_in_neg * negchild_surface_area + (float)num_in_pos * poschild_surface_area) *
					recip_aabb_surf_area * intersection_cost;
				assert(cost >= 0.0);

				if(cost < smallest_cost)
				{
					smallest_cost = cost;
					best_axis = axis;
					best_div_val = splitval;
					best_num_in_neg = num_in_neg;
					best_num_in_pos = num_in_pos;
					//best_num_on_split_plane = 0;
				}
			}
			else
			{
				// Try pushing tris on splitting plane to left
				const float push_left_cost = traversal_cost + ((float)(num_in_neg + num_on_splitting_plane) * negchild_surface_area + (float)num_in_pos * poschild_surface_area) *
					recip_aabb_surf_area * intersection_cost;
				assert(push_left_cost >= 0.0);

				if(push_left_cost < smallest_cost)
				{
					smallest_cost = push_left_cost;
					best_axis = axis;
					best_div_val = splitval;
					best_num_in_neg = num_in_neg + num_on_splitting_plane;
					best_num_in_pos = num_in_pos;
					//best_num_on_split_plane = 0;
					best_push_right = false;
				}

				// Try pushing tris on splitting plane to right
				const float push_right_cost = traversal_cost + ((float)num_in_neg * negchild_surface_area + (float)(num_in_pos + num_on_splitting_plane) * poschild_surface_area) *
					recip_aabb_surf_area * intersection_cost;
				assert(push_right_cost >= 0.0);

				if(push_right_cost < smallest_cost)
				{
					smallest_cost = push_right_cost;
					best_axis = axis;
					best_div_val = splitval;
					best_num_in_neg = num_in_neg;
					best_num_in_pos = num_in_pos + num_on_splitting_plane;
					//best_num_on_split_plane = 0;
					best_push_right = true;
				}
			}

			}
		}
	}

	// TEMP NEW: Do empty space cutoff
	if(DO_EMPTY_SPACE_CUTOFF && best_cutoff_frac > 0.4f)
	{
		best_axis = best_cutoff_axis;
		best_div_val = best_cutoff_splitval;
		best_push_right = best_cutoff_push_right;
		if(best_cutoff_push_right)
		{
			best_num_in_neg = 0;
			best_num_in_pos = numtris;
		}
		else
		{
			best_num_in_neg = numtris;
			best_num_in_pos = 0;
		}
		tree.num_empty_space_cutoffs++;
	}

	if(best_axis == -1)
	{
		// If the least cost is to not split the node, then make this node a leaf node
		nodes[cur] = KDTreeNode(
			(unsigned int)leaf_tri_indices_out.size(),
			numtris
			);

		for(unsigned int i=0; i<numtris; ++i)
			leaf_tri_indices_out.push_back((layer.lower_bounds[0])[i].tri_index); //nodetris[i].tri_index);

		tree.num_cheaper_no_split_leafs++;
		return;
	}

	assert(best_axis >= 0 && best_axis <= 2);
	assert(best_div_val >= cur_aabb.min_.x[best_axis]);
	assert(best_div_val <= cur_aabb.max_.x[best_axis]);

	//------------------------------------------------------------------------
	//compute AABBs of child nodes
	//------------------------------------------------------------------------
	AABBox negbox(cur_aabb.min_, cur_aabb.max_);
	negbox.max_.x[best_axis] = best_div_val;

	AABBox posbox(cur_aabb.min_, cur_aabb.max_);
	posbox.min_.x[best_axis] = best_div_val;

	if(best_num_in_neg == (int)numtris && best_num_in_pos == (int)numtris)
	{
		// If we were unable to get a reduction in the number of tris in either of the children,
		// then splitting is pointless.  So make this a leaf node.
		nodes[cur] = KDTreeNode(
			(unsigned int)leaf_tri_indices_out.size(),
			numtris
			);

		for(unsigned int i=0; i<numtris; ++i)
			leaf_tri_indices_out.push_back((layer.lower_bounds[0])[i].tri_index); // nodetris[i].tri_index);

		tree.num_inseparable_tri_leafs++;
		return;
	}

	LayerInfo& childlayer = layers[depth + 1];

	//------------------------------------------------------------------------
	// Build bounds lists for left child
	//------------------------------------------------------------------------

	// Reserve space in our children bounds lists.
	for(int axis=0; axis<3; ++axis)
	{
		childlayer.lower_bounds[axis].reserve(myMax(/*1000, */best_num_in_neg, best_num_in_pos)); // We will use this array for both negative and positive children.
		childlayer.upper_bounds[axis].reserve(myMax(/*1000, */best_num_in_neg, best_num_in_pos));
	}

	const float split = best_div_val;
	const unsigned int split_axis = best_axis;

	for(int axis=0; axis<3; ++axis)
	{
		childlayer.lower_bounds[axis].resize(0);
		childlayer.upper_bounds[axis].resize(0);

		// Build lower bound list for left child
		for(unsigned int i=0; i<numtris; ++i) // for bound in bound list
		{
			// Get the triangle bounds along the splitting aixs
			const uint32 tri_index = (layer.lower_bounds[axis])[i].tri_index;
			const float tri_lower = this->tri_aabbs[tri_index].min_[split_axis];
			const float tri_upper = this->tri_aabbs[tri_index].max_[split_axis];

			if(tri_lower <= split) // Tri touches left volume, including splitting plane
			{
				if(tri_lower < split) // Tri touches left volume, not including splitting plane
				{
					// Tri is in left child
					childlayer.lower_bounds[axis].push_back((layer.lower_bounds[axis])[i]);
				}
				else
				{
					// else tri_lower == split
					if(tri_upper == split && !best_push_right) // If this tri lies on the splitting plane, and we are pushing splitting plane tris left...
						childlayer.lower_bounds[axis].push_back((layer.lower_bounds[axis])[i]);
				}
			}
			// Else tri_lower > split, so doesn't intersect left box of splitting plane.
		}
		// Build Upper bound list for left child
		for(unsigned int i=0; i<numtris; ++i) // for bound in bound list
		{
			// Get the triangle bounds along the splitting aixs
			const uint32 tri_index = (layer.upper_bounds[axis])[i].tri_index;
			const float tri_lower = this->tri_aabbs[tri_index].min_[split_axis];
			const float tri_upper = this->tri_aabbs[tri_index].max_[split_axis];

			if(tri_lower <= split) // Tri touches left volume, including splitting plane
			{
				if(tri_lower < split) // Tri touches left volume, not including splitting plane
				{
					// Tri is in left child
					childlayer.upper_bounds[axis].push_back((layer.upper_bounds[axis])[i]);
				}
				else
				{
					// else tri_lower == split
					if(tri_upper == split && !best_push_right) // If this tri lies on the splitting plane, and we are pushing splitting plane tris left...
						childlayer.upper_bounds[axis].push_back((layer.upper_bounds[axis])[i]);
				}
			}
			// Else tri_lower > split, so doesn't intersect left box of splitting plane.
		}
	}

	//conPrint("Finished binning neg tris, depth=" + toString(depth) + ", size=" + toString(child_tris.size()) + ", capacity=" + toString(child_tris.capacity()));

	//if(depth == 0)
	//	conPrint("Finished splitting tris, elapsed: " + timer.elapsedString());

	//------------------------------------------------------------------------
	//create negative child node, next in the array.
	//------------------------------------------------------------------------
	const unsigned int actual_num_neg_tris = (unsigned int)childlayer.lower_bounds[0].size();
	assert((int)actual_num_neg_tris == best_num_in_neg);

	//if(actual_num_neg_tris > 0)
	//{
		// Add left child node
		const unsigned int left_child_index = (unsigned int)nodes.size();
		assert(left_child_index == cur + 1);
		nodes.push_back(KDTreeNode());

		// Build left subtree
		doBuild(
			print_output,
			verbose,
			tree,
			left_child_index,
			depth + 1,
			maxdepth,
			negbox,
			nodes,
			leaf_tri_indices_out
			);
	//}


	//------------------------------------------------------------------------
	// Build lists for right child
	//------------------------------------------------------------------------
	for(int axis=0; axis<3; ++axis)
	{
		childlayer.lower_bounds[axis].resize(0);
		childlayer.upper_bounds[axis].resize(0);

		// Build lower bound list for right child
		for(unsigned int i=0; i<numtris; ++i) // For each tri
		{
			// Get the triangle bounds along the splitting aixs
			const uint32 tri_index = (layer.lower_bounds[axis])[i].tri_index;
			const float tri_lower = this->tri_aabbs[tri_index].min_[split_axis];
			const float tri_upper = this->tri_aabbs[tri_index].max_[split_axis]; 

			if(tri_upper >= split) // Tri touches right volume, including splitting plane
			{
				if(tri_upper > split) // Tri touches right volume, not including splitting plane
				{
					// Tri is in right child
					childlayer.lower_bounds[axis].push_back((layer.lower_bounds[axis])[i]);
				}
				else
				{
					// else tri_upper == split
					if(tri_lower == split && best_push_right) // If this tri lies on the splitting plane, and we are pushing splitting plane tris right...
						childlayer.lower_bounds[axis].push_back((layer.lower_bounds[axis])[i]);
				}
			}
			// Else tri_upper < split, so doesn't intersect right box of splitting plane.
		}
		// Build lower bound list for right child
		for(unsigned int i=0; i<numtris; ++i) // For each tri
		{
			// Get the triangle bounds along the splitting aixs
			const uint32 tri_index = (layer.upper_bounds[axis])[i].tri_index;
			const float tri_lower = this->tri_aabbs[tri_index].min_[split_axis];
			const float tri_upper = this->tri_aabbs[tri_index].max_[split_axis]; 

			if(tri_upper >= split) // Tri touches right volume, including splitting plane
			{
				if(tri_upper > split) // Tri touches right volume, not including splitting plane
				{
					// Tri is in right child
					childlayer.upper_bounds[axis].push_back((layer.upper_bounds[axis])[i]);
				}
				else
				{
					// else tri_upper == split
					if(tri_lower == split && best_push_right) // If this tri lies on the splitting plane, and we are pushing splitting plane tris right...
						childlayer.upper_bounds[axis].push_back((layer.upper_bounds[axis])[i]);
				}
			}
			// Else tri_upper < split, so doesn't intersect right box of splitting plane.
		}
	}

	//conPrint("Finished binning pos tris, depth=" + toString(depth) + ", size=" + toString(child_tris.size()) + ", capacity=" + toString(child_tris.capacity()));

	//------------------------------------------------------------------------
	//create positive child
	//------------------------------------------------------------------------
	const unsigned int actual_num_pos_tris = (unsigned int)childlayer.lower_bounds[0].size();
	assert((int)actual_num_pos_tris == best_num_in_pos);

	if(actual_num_pos_tris > 0)
	{
		const unsigned int right_child_index = (unsigned int)nodes.size();
		assert(right_child_index > cur);
		nodes.push_back(KDTreeNode());

		// Set details of current node
		nodes[cur] = KDTreeNode(
			//TreeNode::NODE_TYPE_INTERIOR,//actual_num_neg_tris > 0 ? TreeNode::NODE_TYPE_TWO_CHILDREN : TreeNode::NODE_TYPE_RIGHT_CHILD_ONLY,
			best_axis, // split axis
			best_div_val, // split value
			right_child_index // right child index
		);

		// Build right subtree
		doBuild(
			print_output,
			verbose,
			tree,
			right_child_index, // nodes[cur].getPosChildIndex(),
			depth + 1,
			maxdepth,
			posbox,
			nodes,
			leaf_tri_indices_out
			);

		/*if(actual_num_neg_tris > 0)
		{
			// The current node has two children
			nodes.push_back(TreeNode());

			// Set details of current node
			nodes[cur] = TreeNode(
				TreeNode::NODE_TYPE_TWO_CHILDREN,
				best_axis, // split axis
				best_div_val, // split value
				(unsigned int)nodes.size() - 1 // right child index
				);

			// Build right subtree
			doBuild(
				tree,
				nodes[cur].getPosChildIndex(),
				node_tri_layers,
				depth + 1,
				maxdepth,
				posbox,
				lower,
				upper,
				leaf_tri_indices_out,
				nodes
				);
		}
	else
		{
			// The current node only has the positive child
			nodes.push_back(TreeNode()); // Add child node, adjacent to current node
			assert((unsigned int)nodes.size() == cur + 2);

			// Set details of current node
			nodes[cur] = TreeNode(
				TreeNode::NODE_TYPE_RIGHT_CHILD_ONLY,
				best_axis, // split axis
				best_div_val, // split value
				(unsigned int)nodes.size() - 1 // right child index
				);

			// Build right subtree
			doBuild(
				tree,
				nodes[cur].getPosChildIndex(),
				node_tri_layers,
				depth + 1,
				maxdepth,
				posbox,
				lower,
				upper,
				leaf_tri_indices_out,
				nodes
				);
		}*/
	}
	else
	{
		// Right child is empty, so don't actually add it.

		// Set details of current node
		nodes[cur] = KDTreeNode(
			best_axis, // split axis
			best_div_val, // split value
			KDTree::DEFAULT_EMPTY_LEAF_NODE_INDEX // right child index
		);
	}

	if(actual_num_neg_tris + actual_num_pos_tris < numtris)
	{
		assert(0);
		print_output.print("UHOH, lost a tri.");
	}

	assert(actual_num_neg_tris + actual_num_pos_tris > 0);
}


} //end namespace js
