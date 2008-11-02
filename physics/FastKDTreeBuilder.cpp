/*=====================================================================
FastKDTreeBuilder.cpp
---------------------
File created by ClassTemplate on Sun Mar 23 23:33:20 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "FastKDTreeBuilder.h"

#if 0
#include "KDTree.h"
#include <vector>
#include <algorithm>
#include "../graphics/TriBoxIntersection.h"


namespace js
{

class Node 
{
  TriTree::TRI_INDEX tri_index;

  //Node *other;
  //Node *previous; // on the x/y/z-axis the previous boundary when sorted
  //Node *next; // on the x/y/z-axis the next boundary when sorted

  uint32 other; // index of other boundary
  uint32 prev;
  uint32 next;
  bool left;
};




FastKDTreeBuilder::FastKDTreeBuilder()
{
	
}


FastKDTreeBuilder::~FastKDTreeBuilder()
{
	
}



static inline void getMinAndMax(float a, float b, float c, float& min_out, float& max_out)
{
	if(a <= b)
	{
		if(a <= c)
		{
			min_out = a;
			max_out = b >= c ? b : c;
		}
		else //else c < a
		{
			min_out = c;
			max_out = b;
		}
	}
	else // else a > b == b < a
	{
		if(b <= c)
		{
			min_out = b;
			max_out = a >= c ? a : c;
		}
		else //else c < b
		{
			min_out = c;
			max_out = a;
		}
	}
	assert(min_out == myMin(a, myMin(b, c)));
	assert(max_out == myMax(a, myMax(b, c)));
}


bool BoundaryLowerPred(const Boundary& a, const Boundary& b)
{
   return a.lower < b.lower;
}

bool BoundaryUpperPred(const Boundary& a, const Boundary& b)
{
   return a.upper < b.upper;
}


inline static void triBoundsAlongAxis(TriTree& tree, TriTree::TRI_INDEX tri_index, unsigned int axis, float& lower_out, float& upper_out)
{
	getMinAndMax(
		tree.triVertPos(tri_index, 0)[axis],
		tree.triVertPos(tri_index, 1)[axis],
		tree.triVertPos(tri_index, 2)[axis], 
		lower_out, 
		upper_out
		);
}


void FastKDTreeBuilder::build(TriTree& tree, const AABBox& cur_aabb, TriTree::NODE_VECTOR_TYPE& nodes_out, js::Vector<TriTree::TRI_INDEX>& leaf_tri_indices_out)
{
	// Build boundaries
	std::vector<std::vector<Boundary> > min_bounds(3);
	std::vector<std::vector<Boundary> > max_bounds(3);

	for(int axis=0; axis<3; ++axis)
	{
		min_bounds[axis].resize(tree.numTris());
		max_bounds[axis].resize(tree.numTris());

		for(unsigned int i=0; i<tree.numTris(); ++i)
		{
			float lower, upper;
			getMinAndMax(
				tree.triVertPos(i, 0)[axis],
				tree.triVertPos(i, 1)[axis],
				tree.triVertPos(i, 2)[axis], 
				lower, upper);

			Boundary new_bound;
			new_bound.tri_index = i;
			new_bound.lower = lower;
			new_bound.upper = upper;

			min_bounds[axis][i] = new_bound;
			max_bounds[axis][i] = new_bound;
		}
	}

	buildSubTree(
		tree, 
		cur_aabb,
		min_bounds, 
		max_bounds, 
		0,
		64, // TEMP
		nodes_out,
		leaf_tri_indices_out
		);

	// Sort bounds along each axis according to Boundary.val
	for(int axis=0; axis<3; ++axis)
	{
		std::sort(min_bounds[axis].begin(), min_bounds[axis].end(), BoundaryLowerPred); // According to lower
		std::sort(max_bounds[axis].begin(), max_bounds[axis].end(), BoundaryUpperPred); // According to upper
	}
}





static void clippedTriBounds(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2, const AABBox& aabb, float* lower_out, float* upper_out)
{
//	assert(axis < 3 && axis1 < 3 && axis2 < 3);

	//early out:
	/*if(	v0[0] >= aabb.min_[0] && v0[0] <= aabb.max_[0] &&
		v1[0] >= aabb.min_[0] && v1[0] <= aabb.max_[0] &&
		v2[0] >= aabb.min_[0] && v2[0] <= aabb.max_[0] &&
		v0[1] >= aabb.min_[1] && v0[1] <= aabb.max_[1] &&
		v1[1] >= aabb.min_[1] && v1[1] <= aabb.max_[1] &&
		v2[1] >= aabb.min_[1] && v2[1] <= aabb.max_[1]
	{
		getMinAndMax(
			v0[axis],
			v1[axis],
			v2[axis], 
			min_out, max_out);
	}*/

	Vec3f v_a[16] = {v0, v1, v2};
	
	Vec3f v_b[16];

	unsigned int current_num_verts = 3;
	for(unsigned int axis=0; axis<3; ++axis)
	{
		TriBoxIntersection::clipPolyAgainstPlane(v_a, current_num_verts, axis, -aabb.min_[axis], -1.0f, v_b, current_num_verts);
		assert(current_num_verts < 8);
		TriBoxIntersection::clipPolyAgainstPlane(v_b, current_num_verts, axis, aabb.max_[axis], 1.0f, v_a, current_num_verts);
		assert(current_num_verts < 8);
	}
	
	assert(current_num_verts < 8);

	// Ok, so final polygon vertices are in v_a.
	lower_out[0] = lower_out[1] = lower_out[2] = std::numeric_limits<float>::max();
	upper_out[0] = upper_out[1] = upper_out[2] = -std::numeric_limits<float>::max();

	for(unsigned int i=0; i<current_num_verts; ++i)
		for(unsigned int axis=0; axis<3; ++axis)
		{
			lower_out[axis] = myMin(lower_out[axis], v_a[i][axis]);
			upper_out[axis] = myMax(upper_out[axis], v_a[i][axis]);
		}
}









void FastKDTreeBuilder::buildSubTree(
		TriTree& tree, 
		const AABBox& cur_aabb,
		const std::vector<std::vector<Boundary> >& min_bounds, 
		const std::vector<std::vector<Boundary> >& max_bounds, 
		unsigned int depth,
		unsigned int maxdepth,
		TriTree::NODE_VECTOR_TYPE& nodes_out,
		js::Vector<TriTree::TRI_INDEX>& leaf_tri_indices_out
		)
{
	assert(min_bounds[0].size() == min_bounds[1].size() && min_bounds[0].size() == min_bounds[2].size());
	nodes_out.push_back(TreeNode());

	// Check bounds are still sorted properly
	for(int axis=0; axis<3; ++axis)
	{
		//assert(isSorted(min_bounds)); // According to lower
		//assert(isSorted(max_bounds)); // According to upper
	}

	// Check for termination.
	const int SPLIT_THRESHOLD = 2;
	if(min_bounds[0].size() <= SPLIT_THRESHOLD || depth >= maxdepth)
	{
		// Make this node a leaf node.
		/*nodes_out.back().setLeafNode(true);
		nodes_out.back().setLeafGeomIndex((unsigned int)leaf_tri_indices_out.size());
		nodes_out.back().setNumLeafGeom((unsigned int)min_bounds[0].size());*/
		nodes_out.back() = TreeNode(
			(unsigned int)leaf_tri_indices_out.size(),
			(unsigned int)min_bounds[0].size()
			);


		for(unsigned int i=0; i<min_bounds[0].size(); ++i)
			leaf_tri_indices_out.push_back(min_bounds[0][i].tri_index);
		return;
	}

	// Find cost function Minima
	const float traversal_cost = 1.0f;
	const float intersection_cost = 4.0f;
	const float aabb_surface_area = cur_aabb.getSurfaceArea();
	const float recip_aabb_surf_area = 1.0f / aabb_surface_area;

	float lowest_cost = (float)min_bounds[0].size() * intersection_cost; // Initialise to non-split cost

	const unsigned int other_axes[3][2] = {{1, 2}, {0, 2}, {0, 1}};

	int best_axis = -1;
	float best_split;
	int best_num_in_neg, best_num_in_pos;
	for(int axis=0; axis<3; ++axis)
	{
		const unsigned int axis1 = other_axes[axis][0];
		const unsigned int axis2 = other_axes[axis][1];
		const float two_cap_area = (cur_aabb.axisLength(axis1) * cur_aabb.axisLength(axis2)) * 2.0f;
		const float circum = (cur_aabb.axisLength(axis1) + cur_aabb.axisLength(axis2)) * 2.0f;

		// For each lower bound
		int upper_index = 0;
		for(unsigned int i=0; i<min_bounds[axis].size(); ++i)
		{
			// Split value is lower bound
			const float split = min_bounds[axis][i].lower;

			// Advance upper_index until max_bounds[axis][i].upper >= split

			// Number of tris in left child = i
			// Number of tris in right child = num - upper_index

			const int num_in_neg = i;//upper_index;
			const int num_in_pos = 666;//TEMP //num_coplanar_tris + numtris - upper_index;//numtris - i;

			const float negchild_surface_area = two_cap_area + (split - cur_aabb.min_[axis]) * circum;
			const float poschild_surface_area = two_cap_area + (cur_aabb.max_[axis] - split) * circum;

			const float cost = traversal_cost + 
						((float)num_in_neg * negchild_surface_area + (float)num_in_pos * poschild_surface_area) * recip_aabb_surf_area * intersection_cost;

			if(cost < lowest_cost)
			{
				lowest_cost = cost;
				best_axis = axis;
				best_split = split;
				best_num_in_neg = num_in_neg;
				best_num_in_pos = num_in_pos;
			}
		}
	}

	// Check for termination
	if(best_axis == -1)
	{
		// Make this node a leaf node.
		//nodes_out.back().setLeafNode(true);
		//nodes_out.back().setLeafGeomIndex((unsigned int)leaf_tri_indices_out.size());
		//TEMP nodes_out.back().setNumLeafGeom((unsigned int)min_bounds[0].size());

		for(unsigned int i=0; i<min_bounds[0].size(); ++i)
			leaf_tri_indices_out.push_back(min_bounds[0][i].tri_index);
		return;
	}

	// Ok, we have the best split position and axis
	// Build children min and max bound arrays
	std::vector<std::vector<Boundary> > leftchild_min_bounds(3);
	std::vector<std::vector<Boundary> > leftchild_max_bounds(3);
	std::vector<std::vector<Boundary> > rightchild_min_bounds(3);
	std::vector<std::vector<Boundary> > rightchild_max_bounds(3);


	// Divide along split axis
	for(unsigned int i=0; i<min_bounds[best_axis].size(); ++i)
	{
		// If lower < split, then tri goes in left child list
		if(min_bounds[best_axis][i].lower < best_split)
		{
			Boundary new_bound;
			new_bound.tri_index = min_bounds[best_axis][i].tri_index;
			new_bound.lower = min_bounds[best_axis][i].lower;
			new_bound.upper = myMin(best_split, min_bounds[best_axis][i].upper);

			leftchild_min_bounds[best_axis].push_back(new_bound); // Add lower bound
			leftchild_max_bounds[best_axis].push_back(new_bound); // Add upper bound
		}
	}


	//------------------------------------------------------------------------
	//compute AABBs of child nodes
	//------------------------------------------------------------------------
	AABBox negbox(cur_aabb.min_, cur_aabb.max_);
	negbox.max_[best_axis] = best_split;

	AABBox posbox(cur_aabb.min_, cur_aabb.max_);
	posbox.min_[best_axis] = best_split;

	for(unsigned int z=0; z<2; ++z)// For each other axis
	{
		const unsigned int axis = other_axes[best_axis][z];

		for(unsigned int i=0; i<min_bounds[axis].size(); ++i) // For each boundary
		{
			// Clip triangle to parent AABB			
			//float tri_lower[3];
			//float tri_upper[3];
			//clippedTriBounds(tree.triVertPos(min_bounds[axis][i].tri_index, 0)
				// If triangle bounding box intersects left child
					// Then add the bound to the left child list

			float lower, upper;
			triBoundsAlongAxis(tree, min_bounds[axis][i].tri_index, axis, lower, upper);
			//if(lower < best_split)


			
			
			// If triangle bounding box is contained in right child
				// Then add the bound to the right child list
		}
	}

	// Recurse...

	buildSubTree(tree, negbox, leftchild_min_bounds, leftchild_max_bounds, depth + 1, maxdepth, nodes_out, leaf_tri_indices_out);

	buildSubTree(tree, posbox, rightchild_min_bounds, rightchild_max_bounds, depth + 1, maxdepth, nodes_out, leaf_tri_indices_out);
}




} // End namespace js


#endif
