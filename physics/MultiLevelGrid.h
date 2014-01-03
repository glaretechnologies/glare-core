#pragma once


#include "MultiLevelGridNode.h"
#include "jscol_aabbox.h"
#include "../indigo/globals.h"
#include "../simpleraytracer/ray.h"
#include "../maths/Vec4f.h"
#include "../maths/Vec4i.h"
#include "../maths/SSE.h"
#include "../utils/stringutils.h"
#include "../utils/Vector.h"
#include <vector>


/*
Stores the node index,
as well as the cell indices within the node grid 

*/
class StackElement
{
public:
	unsigned int node_depth; // root node has depth zero.
	unsigned int node_index;
	Vec4i cell_i;
	Vec4f grid_orig;
	Vec4f next_t;
};


template <class CellTest, class Result, class NodeData> 
class MultiLevelGrid
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	inline MultiLevelGrid(const Vec4f& min, const Vec4f& max, const CellTest& cell_test_);
	inline ~MultiLevelGrid();

	inline void trace(const Ray& ray, float t_max, Result& result_out);

	const static int MAX_DEPTH = 64;


	js::AABBox aabb;
	CellTest cell_test;
	std::vector<MultiLevelGridNode<NodeData> > nodes;
	float cell_width[MAX_DEPTH]; // cell width for depth
	float recip_cell_width[MAX_DEPTH]; // cell width for depth
};


template <class CellTest, class Result, class NodeData>
MultiLevelGrid<CellTest, Result, NodeData>::MultiLevelGrid(const Vec4f& min, const Vec4f& max, const CellTest& cell_test_)
:	cell_test(cell_test_)
{
	js::AABBox cuboid_aabb(min, max);
	float max_side_w = cuboid_aabb.axisLength(cuboid_aabb.longestAxis());
	
	aabb = js::AABBox(min, min + Vec4f(max_side_w, max_side_w, max_side_w, 0));

	double cell_w = max_side_w / MLG_RES;
	for(size_t i=0; i<MAX_DEPTH; ++i)
	{
		cell_width[i] = (float)cell_w;
		recip_cell_width[i] = (float)(1 / cell_w);
		cell_w /= MLG_RES;
	}
}


template <class CellTest, class Result, class NodeData>
MultiLevelGrid<CellTest, Result, NodeData>::~MultiLevelGrid()
{
}


/*inline Vec3i cellIndices(const Vec4f& offset, float recip_cell_w)
{
	// Truncate offset_i / cell_w to get cell indices
	//return Vec3i(
	//	(int)(offset.x[0] * recip_cell_w),
	//	(int)(offset.x[1] * recip_cell_w),
	//	(int)(offset.x[2] * recip_cell_w)
	//	);

	return floor(offset * recip_cell_w);
}*/


/*
Compute cell origin, given the grid origin, and the cell indices.
*/
/*inline Vec4f cellOrigin(const Vec4f& grid_origin, const Vec3i& cell_i, float cell_width)
{
	return Vec4f(
		grid_origin.x[0] + cell_i.x * cell_width,
		grid_origin.x[1] + cell_i.y * cell_width,
		grid_origin.x[2] + cell_i.z * cell_width,
		1
	);
}*/


inline Vec4f toVec4f(const Vec4i& v)
{
	return Vec4f(_mm_cvtepi32_ps(v.v));
	//return Vec4f((float)v.x, (float)v.y, (float)v.z, 0);
}



/*
// Offset: vector from grid origin to p
// cell_orig: vector from grid origin to cell origin
// cell_w: cell width

offset_within_cell_x = offset_x - cell_orig.x[0]

if dir_x > 0:
	next_t_x = (cellw - offset_within_cell_x) / dir_x
else if dir_x <= 0:
	next_t_x = offset_within_cell_x / -dir_x
	 = -offset_within_cell_x / -dir_x
	 = 0 - offset_within_cell_x / -dir_x
*/
/*inline Vec4f nextT(const Vec4f& offset, const Vec4f& cell_orig, float cell_w, const Vec4f& recip_dir)
{
	return Vec4f(
		((recip_dir.x[0] > 0 ? cell_w : 0) - (offset.x[0] - cell_orig.x[0])) * recip_dir.x[0],
		((recip_dir.x[1] > 0 ? cell_w : 0) - (offset.x[1] - cell_orig.x[1])) * recip_dir.x[1],
		((recip_dir.x[2] > 0 ? cell_w : 0) - (offset.x[2] - cell_orig.x[2])) * recip_dir.x[2],
		0
	);
}*/

// NOTE: this can be optimised.
inline Vec4f nextT(const Vec4f& cell_orig_to_p, float cell_w, const Vec4f& recip_dir)
{
	return Vec4f(
		((recip_dir.x[0] > 0 ? cell_w : 0) - (cell_orig_to_p.x[0])) * recip_dir.x[0],
		((recip_dir.x[1] > 0 ? cell_w : 0) - (cell_orig_to_p.x[1])) * recip_dir.x[1],
		((recip_dir.x[2] > 0 ? cell_w : 0) - (cell_orig_to_p.x[2])) * recip_dir.x[2],
		0
	);
}


inline uint32 nextStepAxis(const Vec4f& next_t)
{
	uint32 mask =	(next_t.x[0] < next_t.x[1] ? 0x1 : 0) |
					(next_t.x[0] < next_t.x[2] ? 0x2 : 0) |
					(next_t.x[1] < next_t.x[2] ? 0x4 : 0);

	assert(mask < 8);
	/*
	mask value: condition						->	smallest axis
	0:	x >= y	&&	x >= z	&& y >= z			->	z
	1:	x < y	&&	x >= z	&& y >= z			->	z
	2:	x >= y	&&	x < z	&& y >= z			->	contradiction
	3:	x < y	&&	x < z	&& y >= z			->	x
	4:	x >= y	&&	x >= z	&& y < z			->	y
	5:	x < y	&&	x >= z	&& y < z			->	contradiction
	6:	x >= y	&&	x < z	&& y < z			->	y
	7:	x < y	&&	x < z	&& y < z			->	x
	*/
	uint32 table[8] = {2, 2, 0, 0, 1, 0, 1, 0};

/*	uint32 axis = table[mask];

	assert(	mask != 2 && mask != 5 &&
			next_t[axis] <= next_t.x[0] &&
			next_t[axis] <= next_t.x[1] &&
			next_t[axis] <= next_t.x[2]
	);*/

	return table[mask];
}
				

inline bool isCellIndexValid(const Vec4i& cell)
{
	return	cell.x[0] >= 0 && cell.x[0] < MLG_RES &&
			cell.x[1] >= 0 && cell.x[1] < MLG_RES &&
			cell.x[2] >= 0 && cell.x[2] < MLG_RES;
}


/*
	// setup initial state
	node = root node
	cell = cellForOrigin(node)

	while 1
		while cell is interior cell
			compute next adjacent cell index
			if(next is in bounds) push next cell index onto to-visit stack

			node = interior cell node
			cell = cellForOrigin(node)

		// visit cell - trace against geometry etc..
		vistCell(node, cell)

		advance to adjacent cell

		if cell is out of bounds
			if to-visit stack is empty
				goto finished
			else
				(node, cell) = pop from to-visit stack
*/
template <class CellTest, class Result, class NodeData>
void MultiLevelGrid<CellTest, Result, NodeData>::trace(const Ray& ray, float ray_max_t, Result& result_out)
{
/*	//TEMP: Disabled because SSE4 intrinsics are causing errors on Linux and OS X.

	const bool VERBOSE_TRAVERSAL = false;

	StackElement stack[MAX_DEPTH];

	// Trace ray against grid AABB
	float min_t, max_t;
	aabb.rayAABBTrace(ray.startPos(), ray.getRecipRayDirF(), min_t, max_t);

	min_t = myMax(min_t, 0.f);
	max_t = myMin(max_t, ray_max_t);

	if(VERBOSE_TRAVERSAL) printVar(min_t);
	if(VERBOSE_TRAVERSAL) printVar(max_t);

	if(min_t > max_t)
		return;

	// Compute step increments, based on the sign of the ray direction.
	const Vec4i sign(
		ray.unitDir().x[0] >= 0.f ? 1 : -1, 
		ray.unitDir().x[1] >= 0.f ? 1 : -1,
		ray.unitDir().x[2] >= 0.f ? 1 : -1,
		1
	);

	// Compute the indices of the cell that we will traverse to when we step out of the grid bounds.
	const Vec4i out(
		ray.unitDir().x[0] >= 0.f ? MLG_RES : -1, 
		ray.unitDir().x[1] >= 0.f ? MLG_RES : -1, 
		ray.unitDir().x[2] >= 0.f ? MLG_RES : -1,
		1
	);

	const Vec4f recip_dir = ray.getRecipRayDirF();
	const Vec4f abs_recip_dir(std::fabs(recip_dir.x[0]), std::fabs(recip_dir.x[1]), std::fabs(recip_dir.x[2]), 0);

	// Init
	int node = 0; // Current node - set to root node
	int depth = 0; // depth of current node.
	int stack_size = 0; // Size of stack.
	float t = min_t;
	Vec4f p = ray.pointf(min_t); // Current point
	Vec4f cur_grid_orig = this->aabb.min_; // Origin of current node.
	Vec4i cell = floorToVec4i(Vec4f(p - cur_grid_orig) * recip_cell_width[depth]); // Get current cell indices from offset

	cell = clamp(cell, Vec4i(0), Vec4i(MLG_RES - 1)); // Clamp cell indices

	Vec4f cell_orig = cur_grid_orig + toVec4f(cell) * cell_width[depth]; // Get origin of current cell
	Vec4f next_t = nextT(p - cell_orig, cell_width[depth], recip_dir); // distance along ray to next cell in each axis direction, from the original ray origin.

	Vec4f t_delta = abs_recip_dir * cell_width[depth]; // Step in t for a cell step for each axis.

	if(VERBOSE_TRAVERSAL) conPrint("------------------------Doing traversal------------------------");
	if(VERBOSE_TRAVERSAL) conPrint("aabb min: " + aabb.min_.toString());
	if(VERBOSE_TRAVERSAL) conPrint("aabb max: " + aabb.max_.toString());
	
	while(1)
	{
		assert(isCellIndexValid(cell));

		p = ray.pointf(t);

		if(VERBOSE_TRAVERSAL) conPrint("y: " + toString(t));
		if(VERBOSE_TRAVERSAL) conPrint("p: " + p.toString());
		if(VERBOSE_TRAVERSAL) conPrint("depth: " + toString(depth));
		if(VERBOSE_TRAVERSAL) conPrint("cur_grid_orig: " + cur_grid_orig.toString());
		if(VERBOSE_TRAVERSAL) conPrint("recip_cell_width: " + toString(recip_cell_width[depth]));
		if(VERBOSE_TRAVERSAL) { conPrint("node " + toString(node) + ", cell " + cell.toString()); }

		while(nodes[node].cellIsInterior(cell))
		{	
			// Get next cell
			uint32 next_axis = nextStepAxis(next_t);
			Vec4i next_cell = cell;
			next_cell[next_axis] += sign[next_axis];
			Vec4f new_next_t = next_t;
			new_next_t.x[next_axis] += t_delta.x[next_axis];

			if(next_cell[next_axis] != out[next_axis])
			{
				// Push next cell onto stack
				stack[stack_size].node_index = node;
				stack[stack_size].node_depth = depth;
				stack[stack_size].cell_i = next_cell;
				stack[stack_size].grid_orig = cur_grid_orig;
				stack[stack_size].next_t = new_next_t;
				stack_size++;

				if(VERBOSE_TRAVERSAL) { conPrint("\nPushed on to to-visit stack: node " + toString(node) + ", cell " + next_cell.toString()); }
			}

			// Traverse to child node
			node = nodes[node].cellChildIndex(cell); // Go to child node.
			cur_grid_orig = cur_grid_orig + toVec4f(cell) * cell_width[depth]; // Get child node grid origin
			depth++;
			cell = floorToVec4i(Vec4f(p - cur_grid_orig) * recip_cell_width[depth]); // Get current cell indices from offset

			cell = clamp(cell, Vec4i(0), Vec4i(MLG_RES - 1)); // Clamp cell indices

			cell_orig = cur_grid_orig + toVec4f(cell) * cell_width[depth]; // Get origin of current cell
			next_t = Vec4f(t) + nextT(p - cell_orig, cell_width[depth], recip_dir);
			t_delta = abs_recip_dir * cell_width[depth]; // Compute step in t for a cell step for each axis.

			if(VERBOSE_TRAVERSAL) { conPrint("\nTraversed to child cell: node " + toString(node) + ", cell " + cell.toString()); }
			if(VERBOSE_TRAVERSAL) conPrint("cell_orig: " + cell_orig.toString());
			if(VERBOSE_TRAVERSAL) conPrint("next_t: " + next_t.toString());
			if(VERBOSE_TRAVERSAL) conPrint("t_delta: " + t_delta.toString());

			assert(isCellIndexValid(cell));
		} // End while(interior cell)

		uint32 next_axis = nextStepAxis(next_t);
		
		// Visit leaf cell
		float t_enter = t;
		float t_exit = next_t.x[next_axis];
		
		if(VERBOSE_TRAVERSAL) { printVar(t_enter); printVar(t_exit); conPrint("Visiting node " + toString(node) + ", cell " + cell.toString()); }

		cell_test.visit(ray, node, cell, nodes[node].data, t_enter, t_exit, max_t, result_out);

		// Advance to adjacent cell
		t = next_t.x[next_axis];
		cell[next_axis] += sign[next_axis];
		next_t.x[next_axis] += t_delta.x[next_axis];
		

		if(VERBOSE_TRAVERSAL) { conPrint("Advanced to adjacent cell: node " + toString(node) + ", cell " + cell.toString()); }

		if(t > max_t) // If the t value at which we enter the cell is greater than the ray max t, then we are finished.
		{
			if(VERBOSE_TRAVERSAL) { conPrint("t > max_t"); }
			break;
		}

		if(cell[next_axis] == out[next_axis]) // If we have traversed out of the grid:
		{
			if(VERBOSE_TRAVERSAL) { conPrint("out of bounds"); }

			if(stack_size == 0) // If to-visit stack is empty:
				break; // Terminate traversal.
			else
			{
				// Pop from to-visit stack
				stack_size--;
				node = stack[stack_size].node_index;
				cell = stack[stack_size].cell_i;
				depth = stack[stack_size].node_depth;
				cur_grid_orig = stack[stack_size].grid_orig;
				next_t = stack[stack_size].next_t;
				
				t_delta = abs_recip_dir * cell_width[depth]; // Compute step in t for a cell step for each axis.

				assert(isCellIndexValid(cell));

				if(VERBOSE_TRAVERSAL) { conPrint("Popped cell from stack: node " + toString(node) + ", cell " + cell.toString()); }
			}
		}
	} // End while(1)
	*/
}
