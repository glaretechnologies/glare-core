/*=====================================================================
OldKDTreeBuilder.cpp
--------------------
File created by ClassTemplate on Sun Mar 23 23:42:55 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "OldKDTreeBuilder.h"

#include <algorithm>
#include "../graphics/TriBoxIntersection.h"


namespace js
{

#define CLIP_TRIANGLES 1
const static bool DO_EMPTY_SPACE_CUTOFF = true;


OldKDTreeBuilder::OldKDTreeBuilder()
{
	num_cheaper_no_split_leafs = 0;
	num_inseparable_tri_leafs = 0;
	num_maxdepth_leafs = 0;
	num_under_thresh_leafs = 0;
	numnodesbuilt = 0;
	num_empty_space_cutoffs = 0;
}


OldKDTreeBuilder::~OldKDTreeBuilder()
{
	
}


void OldKDTreeBuilder::build(TriTree& tree, const AABBox& root_aabb, TriTree::NODE_VECTOR_TYPE& nodes_out, js::Vector<TriTree::TRI_INDEX>& leaf_tri_indices_out)
{

	unsigned int max_depth = tree.calcMaxDepth();

	{ // Scope for various arrays
		std::vector<std::vector<TriInfo> > node_tri_layers(TriTree::MAX_KDTREE_DEPTH); // One list of tris for each depth

		//for(int t=0; t<MAX_KDTREE_DEPTH; ++t)
		//	node_tri_layers[t].reserve(1000);
		
		node_tri_layers[0].resize(tree.numTris());
		for(unsigned int i=0; i<tree.numTris(); ++i)
		{
			node_tri_layers[0][i].tri_index = i;

			// Get tri AABB
			SSE_ALIGN AABBox tri_aabb(tree.triVertPos(i, 0), tree.triVertPos(i, 0));
			const SSE_ALIGN PaddedVec3 v1 = tree.triVertPos(i, 1);
			tri_aabb.enlargeToHoldAlignedPoint(v1);
			const SSE_ALIGN PaddedVec3 v2 = tree.triVertPos(i, 2);
			tri_aabb.enlargeToHoldAlignedPoint(v2);

			node_tri_layers[0][i].lower = tri_aabb.min_;
			node_tri_layers[0][i].upper = tri_aabb.max_;
		}


		// Create thread pool
		// Add X threads (X = number of cores)


		std::vector<SortedBoundInfo> lower(tree.numTris());
		std::vector<SortedBoundInfo> upper(tree.numTris());
		doBuild(
			tree, 
			TriTree::ROOT_NODE_INDEX, 
			node_tri_layers, 
			0,
			max_depth, root_aabb, lower, upper, leaf_tri_indices_out, nodes_out);
	}
}

static void triTreeDebugPrint(const std::string& s)
{
	conPrint("\t" + s);
}

static inline bool SortedBoundInfoLowerPred(const OldKDTreeBuilder::SortedBoundInfo& a, const OldKDTreeBuilder::SortedBoundInfo& b)
{
   return a.lower < b.lower;
}

static inline bool SortedBoundInfoUpperPred(const OldKDTreeBuilder::SortedBoundInfo& a, const OldKDTreeBuilder::SortedBoundInfo& b)
{
   return a.upper < b.upper;
}


void OldKDTreeBuilder::doBuild(TriTree& tree, unsigned int cur, // index of current node getting built
						std::vector<std::vector<TriInfo> >& node_tri_layers,
						unsigned int depth, // depth of current node
						unsigned int maxdepth, // max permissible depth
						const AABBox& cur_aabb, // AABB of current node
						std::vector<SortedBoundInfo>& upper, 
						std::vector<SortedBoundInfo>& lower,
						js::Vector<TriTree::TRI_INDEX>& leaf_tri_indices_out,
						std::vector<TreeNode>& nodes)
{
	// Get the list of tris intersecting this volume
	assert(depth < (unsigned int)node_tri_layers.size());
	const std::vector<TriInfo>& nodetris = node_tri_layers[depth];

	assert(cur < (int)nodes.size());

	// Print progress message
	if(depth == 5)
	{
		triTreeDebugPrint(::toString(numnodesbuilt) + "/" + ::toString(1 << 5) + " nodes at depth 5 built.");
		numnodesbuilt++;
	}
	
	//------------------------------------------------------------------------
	//test for termination of splitting
	//------------------------------------------------------------------------
	const int SPLIT_THRESHOLD = 1;
	if(nodetris.size() <= SPLIT_THRESHOLD || depth >= maxdepth)
	{
		// Make this node a leaf node.
		nodes[cur] = TreeNode(
			(unsigned int)leaf_tri_indices_out.size(), // Leaf geom index
			(unsigned int)nodetris.size() // num leaf geom
			);

		for(unsigned int i=0; i<nodetris.size(); ++i)
			leaf_tri_indices_out.push_back(nodetris[i].tri_index);

		// Do stats
		if(depth >= maxdepth)
			num_maxdepth_leafs++;
		else
			num_under_thresh_leafs++;
		return;
	}

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
	float smallest_cost = (float)nodetris.size() * intersection_cost;

	const unsigned int numtris = (unsigned int)nodetris.size();

	const unsigned int nonsplit_axes[3][2] = {{1, 2}, {0, 2}, {0, 1}};

	//------------------------------------------------------------------------
	//Find Best axis and split plane value
	//------------------------------------------------------------------------
	for(int axis=0; axis<3; ++axis)
	{
		const unsigned int axis1 = nonsplit_axes[axis][0];
		const unsigned int axis2 = nonsplit_axes[axis][1];
		const float two_cap_area = (cur_aabb.axisLength(axis1) * cur_aabb.axisLength(axis2)) * 2.0f;
		const float circum = (cur_aabb.axisLength(axis1) + cur_aabb.axisLength(axis2)) * 2.0f;

		//if(two_cap_area == 0.0)
		//	continue;
		if(cur_aabb.axisLength(axis) == 0.0f)
			continue; // Don't try to split a zero-width bounding box
		
		//------------------------------------------------------------------------
		//Sort lower and upper tri AABB bounds into ascending order
		//------------------------------------------------------------------------
		for(unsigned int i=0; i<numtris; ++i)
		{
//			assert(nodetris[i].lower[axis] >= cur_aabb.min_[axis] && nodetris[i].upper[axis] <= cur_aabb.max_[axis]);
			assert(nodetris[i].lower[axis] <= nodetris[i].upper[axis]);

			lower[i].lower = nodetris[i].lower[axis];
			lower[i].upper = nodetris[i].upper[axis];
			upper[i].lower = nodetris[i].lower[axis];
			upper[i].upper = nodetris[i].upper[axis];
		}

		//Timer sort_timer;

		//Note that we don't want to sort the whole buffers here, as we're only using the first numtris elements.
		std::sort(lower.begin(), lower.begin() + numtris, SortedBoundInfoLowerPred); //lower.end());
		std::sort(upper.begin(), upper.begin() + numtris, SortedBoundInfoUpperPred); //upper.end());

		//conPrint("sort took " + toString(sort_timer.getSecondsElapsed()));

		// Try explicit 'Early empty-space cutoff', see section 4.4 of Havran's thesis
		// 'Construction of Kd-Trees with Utilization of Empty Spatial Regions'
		if(DO_EMPTY_SPACE_CUTOFF)
		{
			const float lower_cutoff_frac = myClamp((lower[0].lower - cur_aabb.min_[axis]) / cur_aabb.axisLength(axis), 0.f, 1.f);
			const float upper_cutoff_frac = myClamp((cur_aabb.max_[axis] - upper[numtris-1].upper) / cur_aabb.axisLength(axis), 0.f, 1.f);
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


		// Tris in contact with splitting plane will not imply tri is in either volume,
		// except for tris lying totally on splitting plane which are considered to be in positive volume.

		unsigned int upper_index = 0;
		//Index of first triangle in upper volume
		//Index of first tri with upper bound >= then current split val.
		//Also the number of tris with upper bound <= current split val.
		//which is the number of tris *not* in pos volume

		float last_splitval = -std::numeric_limits<float>::max();
		for(unsigned int i=0; i<numtris; ++i)
		{
			const float splitval = lower[i].lower;
			//assert(splitval >= cur_aabb.min_[axis] && splitval <= cur_aabb.max_[axis]);

			/*int num_on_splitting_plane = 0;
			while(i<numtris && lower[i].lower == splitval)
			{
				// If the tri has zero extent along the current axis
				if(lower[i].upper == splitval)
					num_on_splitting_plane++;

				i++;
			}*/

			if(splitval != last_splitval)//only consider first tri seen with a given lower bound.
			{
				if(splitval >= cur_aabb.min_[axis] && splitval <= cur_aabb.max_[axis]) // If split val is actually in AABB
				{
				// Get number of tris on splitting plane
				int num_on_splitting_plane = 0;
				for(unsigned int z=i; z<numtris && lower[z].lower == splitval; ++z) // for all other triangles that share the current splitting plane as a lower bound
					if(lower[z].upper == splitval) // If the tri has zero extent along the current axis
						num_on_splitting_plane++;

				//if(splitval > cur_aabb.min_[axis] && splitval < cur_aabb.max_[axis]) // If split val is actually in AABB
				//{
					//advance upper index to maintain invariant above
					//while(upper_index < numtris && upper[upper_index] < splitval) //<= splitval)
					//	upper_index++;

					//advance upper_index while it points to triangles with upper < split (definitely in negative volume)
					/*while(upper_index < numtris && upper[upper_index] < splitval)
						upper_index++;

					unsigned int num_coplanar_tris = 0;
					//advance upper_index while it points to triangles with upper == split, which may be coplanar and hence in positive volume, if lower == split as well.
					while(upper_index < numtris && upper[upper_index] == splitval)
					{
						if(lower[upper_index] == splitval)
							num_coplanar_tris++;
						upper_index++;
					}

					assert(upper_index == numtris || upper[upper_index] > splitval);

					const int num_in_neg = i;//upper_index;
					const int num_in_pos = num_coplanar_tris + numtris - upper_index;//numtris - i;
					assert(num_in_neg >= 0 && num_in_neg <= (int)numtris);
					assert(num_in_pos >= 0 && num_in_pos <= (int)numtris);
					assert(num_in_neg + num_in_pos >= (int)numtris);*/

				while(upper_index < numtris && upper[upper_index].upper <= splitval)
					upper_index++;

				assert(upper_index == numtris || upper[upper_index].upper > splitval);

				const int num_in_neg = i;
				const int num_in_pos = numtris - upper_index;
				assert(num_in_neg >= 0 && num_in_neg <= (int)numtris);
				assert(num_in_pos >= 0 && num_in_pos <= (int)numtris);
				assert(num_in_neg + num_in_pos + num_on_splitting_plane >= (int)numtris);

				//const int num_on_splitting_plane = numtris - (num_in_neg + num_in_pos);



				const float negchild_surface_area = two_cap_area + (splitval - cur_aabb.min_[axis]) * circum;
				const float poschild_surface_area = two_cap_area + (cur_aabb.max_[axis] - splitval) * circum;
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
			//assert(splitval >= cur_aabb.min_[axis] && splitval <= cur_aabb.max_[axis]);
			
			if(splitval >= cur_aabb.min_[axis] && splitval <= cur_aabb.max_[axis])
			{

			int num_on_splitting_plane = 0;
			for(int z=i; z>=0 && upper[z].upper == splitval; --z) // For each triangle sharing an upper bound with the current triangle
				if(upper[z].lower == splitval) // If tri has zero extent along this axis
					num_on_splitting_plane++;

			/*unsigned int num_coplanar_tris = lower[i] == upper[i] ? 1 : 0;
			//if tri i and tri i+1 share upper bounds, advance to largest index of tris sharing same upper bound
			while(i<numtris-1 && upper[i] == upper[i+1])
			{
				++i;
				if(lower[i] == upper[i])
					num_coplanar_tris++;
			}


			assert(i < upper.size());
			const float splitval = upper[i];
			
			//if(splitval != last_splitval)
			//{
				//if(splitval > cur_aabb.min_[axis] && splitval < cur_aabb.max_[axis])
				//{
					//advance lower index to maintain invariant above
					while(lower_index < numtris && lower[lower_index] < splitval)
						lower_index++;

					assert(lower_index == numtris || lower[lower_index] >= splitval);

					const int num_in_neg = lower_index;//i;
					const int num_in_pos = num_coplanar_tris + numtris - (i + 1);//numtris - lower_index;
					assert(num_in_neg >= 0 && num_in_neg <= (int)numtris);
					assert(num_in_pos >= 0 && num_in_pos <= (int)numtris);
					assert(num_in_neg + num_in_pos >= (int)numtris);*/

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

			const float negchild_surface_area = two_cap_area + (splitval - cur_aabb.min_[axis]) * circum;
			const float poschild_surface_area = two_cap_area + (cur_aabb.max_[axis] - splitval) * circum;
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

					/*const float negchild_surface_area = two_cap_area + (splitval - cur_aabb.min_[axis]) * circum;
					const float poschild_surface_area = two_cap_area + (cur_aabb.max_[axis] - splitval) * circum;
					assert(negchild_surface_area >= 0.f && negchild_surface_area <= aabb_surface_area + NICKMATHS_EPSILON);
					assert(poschild_surface_area >= 0.f && negchild_surface_area <= aabb_surface_area + NICKMATHS_EPSILON);
					assert(::epsEqual(negchild_surface_area + poschild_surface_area - two_cap_area, aabb_surface_area, aabb_surface_area * (float)1.0e-6));

					const float cost = traversal_cost + 
						((float)num_in_neg * negchild_surface_area + (float)num_in_pos * poschild_surface_area) * recip_aabb_surf_area * intersection_cost;

					assert(cost >= 0.0);

					if(cost < smallest_cost)
					{
						smallest_cost = cost;
						best_axis = axis;
						best_div_val = splitval;
						best_num_in_neg = num_in_neg;
						best_num_in_pos = num_in_pos;
					}*/
				//}
				//last_splitval = splitval;
			//}
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
		num_empty_space_cutoffs++;
	}

	if(best_axis == -1)
	{
		// If the least cost is to not split the node, then make this node a leaf node
		nodes[cur] = TreeNode(
			(unsigned int)leaf_tri_indices_out.size(),
			(unsigned int)nodetris.size()
			);

		for(unsigned int i=0; i<nodetris.size(); ++i)
			leaf_tri_indices_out.push_back(nodetris[i].tri_index);

		num_cheaper_no_split_leafs++;
		return;
	}

	assert(best_axis >= 0 && best_axis <= 2);
	assert(best_div_val >= cur_aabb.min_[best_axis]);
	assert(best_div_val <= cur_aabb.max_[best_axis]);
	
	//------------------------------------------------------------------------
	//compute AABBs of child nodes
	//------------------------------------------------------------------------
	SSE_ALIGN AABBox negbox(cur_aabb.min_, cur_aabb.max_);
	negbox.max_[best_axis] = best_div_val;

	SSE_ALIGN AABBox posbox(cur_aabb.min_, cur_aabb.max_);
	posbox.min_[best_axis] = best_div_val;

	if(best_num_in_neg == numtris && best_num_in_pos == numtris)
	{
		// If we were unable to get a reduction in the number of tris in either of the children,
		// then splitting is pointless.  So make this a leaf node.
		nodes[cur] = TreeNode(
			(unsigned int)leaf_tri_indices_out.size(),
			(unsigned int)nodetris.size()
			);

		for(unsigned int i=0; i<(unsigned int)nodetris.size(); ++i)
			leaf_tri_indices_out.push_back(nodetris[i].tri_index);

		num_inseparable_tri_leafs++;
		return;
	}

	assert(depth + 1 < (int)node_tri_layers.size());
	std::vector<TriInfo>& child_tris = node_tri_layers[depth + 1];

	//------------------------------------------------------------------------
	// Build list of neg tris
	//------------------------------------------------------------------------
	child_tris.resize(0);
	child_tris.reserve(myMax(best_num_in_neg, best_num_in_pos));

	assert(numtris == nodetris.size());
	const float split = best_div_val;
	for(unsigned int i=0; i<numtris; ++i)//for each tri
	{
		const float tri_lower = nodetris[i].lower[best_axis];
		const float tri_upper = nodetris[i].upper[best_axis];

		/*if(tri_upper <= split)//does tri lie entirely in min volume (including splitting plane)?
		{
			// Tri is either in neg box, or on splitting plane
			if(tri_lower < split)
				child_tris.push_back(nodetris[i]);
		}
		else // else tri_upper > split
		{
			if(tri_lower < split)
			{
				// Tri lies in both boxes
				child_tris.push_back(TriInfo());
				child_tris.back().tri_index = nodetris[i].tri_index;

				SSE_ALIGN AABBox clipped_tri_aabb;
				TriBoxIntersection::slowGetClippedTriAABB(
					triVertPos(nodetris[i].tri_index, 0), triVertPos(nodetris[i].tri_index, 1), triVertPos(nodetris[i].tri_index, 2),
					negbox,
					clipped_tri_aabb
					);
				assert(negbox.containsAABBox(clipped_tri_aabb));
				child_tris.back().lower = clipped_tri_aabb.min_;
				child_tris.back().upper = clipped_tri_aabb.max_;
			}
		}*/
		if(tri_lower <= split) // Tri touches left volume, including splitting plane
		{
			if(tri_lower < split) // Tri touches left volume, not including splitting plane
			{
				if(tri_upper > split)
				{
					// Tri straddles split plane
#ifdef CLIP_TRIANGLES
					child_tris.push_back(TriInfo());
					child_tris.back().tri_index = nodetris[i].tri_index;

					SSE_ALIGN AABBox clipped_tri_aabb;
					TriBoxIntersection::slowGetClippedTriAABB(
						tree.triVertPos(nodetris[i].tri_index, 0), tree.triVertPos(nodetris[i].tri_index, 1), tree.triVertPos(nodetris[i].tri_index, 2),
						negbox,
						clipped_tri_aabb
						);
					assert(negbox.containsAABBox(clipped_tri_aabb));
					assert(clipped_tri_aabb.invariant());
					child_tris.back().lower = clipped_tri_aabb.min_;
					child_tris.back().upper = clipped_tri_aabb.max_;
#else
					child_tris.push_back(nodetris[i]);
#endif
				}
				else // else tri_upper <= split
					child_tris.push_back(nodetris[i]); // Tri is only in left child
			}
			else
			{
				// else tri_lower == split
				if(tri_upper == split && !best_push_right)
					child_tris.push_back(nodetris[i]);
			}
		}
		// Else tri_lower > split, so doesn't intersect left box of splitting plane.

	}

	//conPrint("Finished binning neg tris, depth=" + toString(depth) + ", size=" + toString(child_tris.size()) + ", capacity=" + toString(child_tris.capacity()));
	assert(child_tris.size() == best_num_in_neg);

	//------------------------------------------------------------------------
	//create negative child node, next in the array.
	//------------------------------------------------------------------------
	const unsigned int actual_num_neg_tris = (unsigned int)child_tris.size();
	//if(actual_num_neg_tris > 0)
	//{
		// Add left child node
		const unsigned int left_child_index = (unsigned int)nodes.size();
		assert(left_child_index == cur + 1);
		nodes.push_back(TreeNode());

		// Build left subtree
		doBuild(
			tree,
			left_child_index,
			node_tri_layers, 
			depth + 1, 
			maxdepth, 
			negbox, 
			lower, 
			upper,
			leaf_tri_indices_out,
			nodes
			);
	//}


	//------------------------------------------------------------------------
	// Build list of pos tris
	//------------------------------------------------------------------------
	child_tris.resize(0);

	assert(numtris == nodetris.size());
	//assert(best_axis == nodes[cur].getSplittingAxis());
	for(unsigned int i=0; i<numtris; ++i) // For each tri
	{
		const float tri_lower = nodetris[i].lower[best_axis];
		const float tri_upper = nodetris[i].upper[best_axis];

		/*if(tri_upper <= split) // Does tri lie entirely in min volume (including splitting plane)?
		{
			// Tri is either in neg box, or on splitting plane
			if(tri_lower >= split)
				child_tris.push_back(nodetris[i]);//tri lies entirely on splitting plane.  so assign it to positive half.
		}
		else // else tri_upper > split
		{
			if(tri_lower < split)
			{
				// tri straddles splitting plane
				child_tris.push_back(TriInfo());
				child_tris.back().tri_index = nodetris[i].tri_index;

				SSE_ALIGN AABBox clipped_tri_aabb;
				TriBoxIntersection::slowGetClippedTriAABB(
					triVertPos(nodetris[i].tri_index, 0), triVertPos(nodetris[i].tri_index, 1), triVertPos(nodetris[i].tri_index, 2),
					posbox,
					clipped_tri_aabb
					);
				assert(posbox.containsAABBox(clipped_tri_aabb));
				child_tris.back().lower = clipped_tri_aabb.min_;
				child_tris.back().upper = clipped_tri_aabb.max_;
			}
			else
				child_tris.push_back(nodetris[i]); // Tri lies totally in positive half
		}*/

		if(tri_upper >= split) // Tri touches right volume, including splitting plane
		{
			if(tri_upper > split) // Tri touches right volume, not including splitting plane
			{
				if(tri_lower < split)
				{
					// Tri straddles splitting plane
#ifdef CLIP_TRIANGLES
					child_tris.push_back(TriInfo());
					child_tris.back().tri_index = nodetris[i].tri_index;

					SSE_ALIGN AABBox clipped_tri_aabb;
					TriBoxIntersection::slowGetClippedTriAABB(
						tree.triVertPos(nodetris[i].tri_index, 0), tree.triVertPos(nodetris[i].tri_index, 1), tree.triVertPos(nodetris[i].tri_index, 2),
						posbox,
						clipped_tri_aabb
						);
					assert(posbox.containsAABBox(clipped_tri_aabb));
					assert(clipped_tri_aabb.invariant());
					child_tris.back().lower = clipped_tri_aabb.min_;
					child_tris.back().upper = clipped_tri_aabb.max_;
#else

					child_tris.push_back(nodetris[i]);
#endif
				}
				else // else tri_lower >= split
					child_tris.push_back(nodetris[i]); // Tri is only in right child
			}
			else
			{
				// else tri_upper == split
				if(tri_lower == split && best_push_right)
					child_tris.push_back(nodetris[i]);
			}
		}
		// Else tri_upper < split, so doesn't intersect right box of splitting plane.
	}

	assert(child_tris.size() == best_num_in_pos);

	//conPrint("Finished binning pos tris, depth=" + toString(depth) + ", size=" + toString(child_tris.size()) + ", capacity=" + toString(child_tris.capacity()));

	//------------------------------------------------------------------------
	//create positive child
	//------------------------------------------------------------------------
	const unsigned int actual_num_pos_tris = (unsigned int)child_tris.size();
	if(actual_num_pos_tris > 0)
	{
		const unsigned int right_child_index = (unsigned int)nodes.size();
		assert(right_child_index > cur);
		nodes.push_back(TreeNode());
	
		// Set details of current node
		nodes[cur] = TreeNode(
			//TreeNode::NODE_TYPE_INTERIOR,//actual_num_neg_tris > 0 ? TreeNode::NODE_TYPE_TWO_CHILDREN : TreeNode::NODE_TYPE_RIGHT_CHILD_ONLY,
			best_axis, // split axis
			best_div_val, // split value
			right_child_index // right child index
		);

		// Build right subtree
		doBuild(
			tree,
			right_child_index, // nodes[cur].getPosChildIndex(), 
			node_tri_layers, 
			depth + 1, 
			maxdepth, 
			posbox, 
			lower, 
			upper,
			leaf_tri_indices_out,
			nodes
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
		nodes[cur] = TreeNode(
			best_axis, // split axis
			best_div_val, // split value
			TriTree::DEFAULT_EMPTY_LEAF_NODE_INDEX // right child index
		);
	}

	assert(actual_num_neg_tris + actual_num_pos_tris > 0);
}





} //end namespace js






