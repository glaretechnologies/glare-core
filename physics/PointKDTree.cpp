/*=====================================================================
PointKDTree.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Apr 07 12:56:37 +1200 2010
=====================================================================*/
#include "PointKDTree.h"


#include "../maths/vec3.h"
#include "../utils/Exception.h"
#include <algorithm>
#include <set>


PointKDTree::PointKDTree(const std::vector<Vec3f>& points)
{
	build(points);
}


PointKDTree::~PointKDTree()
{

}


static const uint32 NULL_NODE_INDEX = std::numeric_limits<uint32>::max();


uint32 PointKDTree::getNearestPoint(const Vec3f& p, ThreadContext& thread_context) const
{
	std::vector<uint32> stack(64);

	float smallest_dist2 = std::numeric_limits<float>::max();
	uint32 closest_point_index = notFoundIndex();
	float radius = 0.1f;

	while(1)
	{
		const float radius2 = radius * radius;

		int stacktop = 0; // Index of node on top of stack

		// Push root node onto top of stack
		stack[0] = 0;

		while(stacktop >= 0) // While stack is not empty...
		{
			// Pop node off stack
			uint32 current = stack[stacktop];
			assert(current < nodes.size());
			stacktop--;

			// Keep walking down a tree until we walk off the bottom of a leaf.
			while(current != NULL_NODE_INDEX)
			{
				const float d2 = nodes[current].point.getDist2(p);
				if(d2 < smallest_dist2 && d2 < radius2)
				{
					smallest_dist2 = nodes[current].point.getDist2(p);
					closest_point_index = nodes[current].point_index;
				}

				if(nodes[current].left == NULL_NODE_INDEX && nodes[current].right == NULL_NODE_INDEX)
				{
					// This is a leaf node
					break;
				}


				const unsigned int split_axis = nodes[current].split_axis;
				const float diff = p[split_axis] - nodes[current].split_val;
				if(diff > radius)
				{
					// Then the p sphere lies entirely in right child.
					// So traverse to right child
					current = nodes[current].right;
				}
				else if(diff < -radius)
				{
					// Then the p sphere lies entirely in left child.
					// So traverse to left child
					current = nodes[current].left;
				}
				else
				{
					// Else p sphere intersects both left and right child spaces.
					// So pop right child onto stack, and traverse to left child.

					stacktop++;
					stack[stacktop] = nodes[current].right;
					current = nodes[current].left;
				}
			} // End while(current != NULL_NODE_INDEX)
		} // End while(stacktop >= 0)

		// Expand search radius
		if(closest_point_index == notFoundIndex())
			radius *= 2.0f;
		else
			break;
	}
	return closest_point_index;
}


uint32 PointKDTree::getNearestPointDebug(const std::vector<Vec3f>& points, const Vec3f& p, ThreadContext& thread_context) const
{
	float smallest_dist2 = std::numeric_limits<float>::max();
	uint32 closest_point_index = notFoundIndex();
	for(uint32 i=0; i<(uint32)points.size(); ++i)
	{
		const float d = p.getDist2(points[i]);
		if(d < smallest_dist2)
		{
			smallest_dist2 = d;
			closest_point_index = i;
		}
	}
	return closest_point_index;
}


void PointKDTree::printTree(std::ostream& stream, unsigned int node_index, unsigned int depth) const
{
	for(unsigned int i=0; i<depth; ++i)
		stream << "    ";
	stream << "node index: " << node_index << " " /*<< nodes[node_index].point.toString()*/ << " left: " << nodes[node_index].left << ", right: " << nodes[node_index].right << "\n";

	//process neg child
	if(nodes[node_index].left != NULL_NODE_INDEX)
		this->printTree(stream, nodes[node_index].left, depth + 1);

	//process pos child
	if(nodes[node_index].right != NULL_NODE_INDEX)
		this->printTree(stream, nodes[node_index].right, depth + 1);
}


class LowerPred
{
public:
	inline bool operator()(const PointKDTree::AxisPoint& a, const PointKDTree::AxisPoint& b)
	{
		return a.val < b.val;
	}
};


void PointKDTree::build(const std::vector<Vec3f>& points)
{	
	const int max_depth = 64;

	// Initialise axis points
	layers.resize(max_depth + 1);

	//axis_points.resize(3);

	for(int axis=0; axis<3; ++axis)
	{
		layers[0].axis_points[axis].resize(points.size());

		for(size_t i=0; i<points.size(); ++i)
		{
			(layers[0].axis_points[axis])[i].val = (points[i])[axis];
			(layers[0].axis_points[axis])[i].point_index = i;
		}
	}

	// Sort bounds
	#ifndef OSX
	#pragma omp parallel for
	#endif
	for(int axis=0; axis<3; ++axis)
	{
		std::sort(layers[0].axis_points[axis].begin(), layers[0].axis_points[axis].end(), LowerPred());
	}

	doBuild(
		points,
		0, // depth
		max_depth // max depth
	);
}


void PointKDTree::checkLayerLists(const std::vector<Vec3f>& points, LayerInfo& layer)
{
	// Check all our bounds are sorted correctly
	for(int axis=0; axis<3; ++axis)
	{
		// Check each axis has the same number of points
		assert(layer.axis_points[axis].size() == layer.axis_points[0].size());

		for(size_t z=1; z<layer.axis_points[axis].size(); ++z)
		{
			const uint32 prev_point_index = (layer.axis_points[axis])[z-1].point_index;
			const uint32 point_index = (layer.axis_points[axis])[z].point_index;

			assert((points[prev_point_index])[axis] <= (points[point_index])[axis]);
		}
	}

	std::set<uint32> p;
	for(size_t i=0; i<layer.axis_points[0].size(); ++i)
	{
		const uint32 point_index = (layer.axis_points[0])[i].point_index;

		if(p.find(point_index) != p.end())
		{
			assert(!"Point already in list.");
		}

		p.insert(point_index);
	}

	for(int axis=0; axis<3; ++axis)
	{
		for(size_t i=0; i<layer.axis_points[axis].size(); ++i)
		{
			const uint32 point_index = (layer.axis_points[axis])[i].point_index;

			if(p.find(point_index) == p.end())
			{
				assert(!"Point missing from list!");
			}
		}
	}
}


static int c = 0;


void PointKDTree::doBuild(const std::vector<Vec3f>& points, int depth, int max_depth)
{	
	c++;

	if(c == 121)
		int a = 9;

	if(depth >= max_depth)
	{
		assert(0);
		throw Indigo::Exception("Reached max depth while building point kd tree.");
	}

	LayerInfo& layer = this->layers[depth];

	const unsigned int num_points = (unsigned int)layer.axis_points[0].size();

#ifdef DEBUG
	checkLayerLists(points, layer);
#endif

	assert(num_points >= 1);

	// Add new node
	const unsigned int current = nodes.size();
	nodes.push_back(PointKDTreeNode(NULL_NODE_INDEX, NULL_NODE_INDEX));

	if(num_points == 1)
	{
		nodes[current].point = points[(layer.axis_points[0])[0].point_index];
		nodes[current].point_index = (layer.axis_points[0])[0].point_index;
		// We're done!
		return;
	}

	int best_axis = -1;
	float best_div_val = -std::numeric_limits<float>::max();
	float smallest_cost = std::numeric_limits<float>::max();
	float largest_axis_length = -1.0f;
	uint32 best_point_index = std::numeric_limits<uint32>::max();

	// TEMP: Just do median split along widest axis for now.
	for(int axis=0; axis<3; ++axis)
	{
		const float axis_length = (layer.axis_points[axis])[num_points - 1].val - (layer.axis_points[axis])[0].val;

		if(axis_length > largest_axis_length)
		{
			largest_axis_length = axis_length;
			best_axis = axis;

			const unsigned int median = num_points / 2;

			best_div_val = (layer.axis_points[axis])[median].val;
			best_point_index = (layer.axis_points[axis])[median].point_index;
		}
	}

	//------------------------------------------------------------------------
	// Build point lists for left child
	//------------------------------------------------------------------------
	LayerInfo& childlayer = layers[depth + 1];

	const float split = best_div_val;
	const unsigned int split_axis = best_axis;

	std::set<uint32> pushed_left;

	for(int axis=0; axis<3; ++axis)
	{
		childlayer.axis_points[axis].resize(0);

		// Get num points on splitting plane
		int num_on_split = 0;

		for(unsigned int i=0; i<num_points; ++i) // for bound in bound list
		{
			const uint32 point_index = (layer.axis_points[axis])[i].point_index;
			const float point_val = (points[point_index])[split_axis];
			if(point_val == split)
				num_on_split++;
		}

		int num_on_split_pushed_left = 0;

		for(unsigned int i=0; i<num_points; ++i) // for bound in bound list
		{
			const uint32 point_index = (layer.axis_points[axis])[i].point_index; // Get original point index.
			const float point_val = (points[point_index])[split_axis]; // Get point coordinate along splitting axis.

			if(point_val < split) // If point lies to left of splitting plane...
			{
				// Point is in left child.
				childlayer.axis_points[axis].push_back((layer.axis_points[axis])[i]);
			}
			else if(point_val == split) // else if point lies on splitting plane
			{
				// Push the first num_on_split/2 points on the split plane to the left
				if(axis == 0)
				{
					if(num_on_split_pushed_left < num_on_split / 2)
					{
						childlayer.axis_points[axis].push_back((layer.axis_points[axis])[i]);
						num_on_split_pushed_left++;

						pushed_left.insert((layer.axis_points[axis])[i].point_index);
					}
				}
				else
				{
					if(pushed_left.find((layer.axis_points[axis])[i].point_index) != pushed_left.end()) // If in pushed_left
						childlayer.axis_points[axis].push_back((layer.axis_points[axis])[i]);
				}
			}

			// Else point is in right child
		}
	}

#ifdef DEBUG
	checkLayerLists(points, childlayer);
#endif

	//------------------------------------------------------------------------
	//Create left child node.
	//------------------------------------------------------------------------
	unsigned int left_child_index;
	
	if(childlayer.axis_points[0].size() > 0)
	{
		left_child_index = (unsigned int)nodes.size();

		// Build left subtree
		doBuild(
			points,
			depth + 1,
			max_depth
		);
	}
	else
		left_child_index = NULL_NODE_INDEX;


	//------------------------------------------------------------------------
	// Build point lists for right child
	//------------------------------------------------------------------------
	for(int axis=0; axis<3; ++axis)
	{
		childlayer.axis_points[axis].resize(0);

		// Get num points on split plane
		/*int num_on_split = 0;

		for(unsigned int i=0; i<num_points; ++i) // for bound in bound list
		{
			const uint32 point_index = (layer.axis_points[axis])[i].point_index;
			const float point_val = (points[point_index])[split_axis];
			if(point_val == split)
				num_on_split++;
		}

		int num_on_split_pushed_left = 0;*/

		for(unsigned int i=0; i<num_points; ++i) // for bound in bound list
		{
			const uint32 point_index = (layer.axis_points[axis])[i].point_index;

			const float point_val = (points[point_index])[split_axis];

			// The splitting point doesn't go in either left or right child.
			if(point_val > split)			//if(point_val >= split && point_index != best_point_index)
			{
				// Point is in right child.
				childlayer.axis_points[axis].push_back((layer.axis_points[axis])[i]);
			}
			else if(point_val == split) // else if point lies on splitting plane
			{
				// Push the first num_on_split/2 points on the split plane to the left, the rest to the right.
				/*if(num_on_split_pushed_left >= num_on_split / 2)
					childlayer.axis_points[axis].push_back((layer.axis_points[axis])[i]);
				else
					num_on_split_pushed_left++;*/

				if(pushed_left.find((layer.axis_points[axis])[i].point_index) == pushed_left.end()) // If not in pushed_left
					childlayer.axis_points[axis].push_back((layer.axis_points[axis])[i]); // Push right

			}
			// Else point is in left child
		}
	}

#ifdef DEBUG
	checkLayerLists(points, childlayer);
#endif

	//------------------------------------------------------------------------
	//Create right child node.
	//------------------------------------------------------------------------
	unsigned int right_child_index;
	if(childlayer.axis_points[0].size() > 0)
	{
		right_child_index = (unsigned int)nodes.size();
		
		// Build right subtree
		doBuild(
			points,
			depth + 1,
			max_depth
		);
	}
	else
		right_child_index = NULL_NODE_INDEX;

	nodes[current].left = left_child_index;
	nodes[current].right = right_child_index;
	nodes[current].split_axis = split_axis;
	nodes[current].split_val = split;
	nodes[current].point = points[best_point_index];
	nodes[current].point_index = best_point_index;

}



