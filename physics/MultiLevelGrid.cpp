#include "MultiLevelGrid.h"


#include "../maths/vec3.h"
#include "../indigo/globals.h"
#include "../utils/stringutils.h"


typedef Vec3<int> Vec3i;


MultiLevelGrid::MultiLevelGrid(const Vec4f& min, const Vec4f& max)
{
	grid_orig = min;

	cell_width.resize(64);
	recip_cell_width.resize(64);

	double cell_w = 1;
	for(size_t i=0; i<64; ++i)
	{
		cell_width[i] = (float)cell_w;
		recip_cell_width[i] = (float)(1 / cell_w);
		cell_w *= 0.5;
	}
}


MultiLevelGrid::~MultiLevelGrid()
{
}


static inline Vec3i cellIndices(const Vec4f& offset, float recip_cell_w)
{
	// Truncate offset_i / cell_w to get cell indices
	return Vec3i(
		(int)(offset.x[0] * recip_cell_w),
		(int)(offset.x[1] * recip_cell_w),
		(int)(offset.x[2] * recip_cell_w)
		);
}


/*
Compute cell origin, given the grid origin, and the cell indices.
*/
static inline Vec4f cellOrigin(const Vec4f& grid_origin, const Vec3i& cell_i, float cell_width)
{
	return Vec4f(
		grid_origin.x[0] + cell_i.x * cell_width,
		grid_origin.x[1] + cell_i.y * cell_width,
		grid_origin.x[2] + cell_i.z * cell_width,
		1
	);
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
static inline Vec4f nextT(const Vec4f& offset, const Vec4f& cell_orig, float cell_w, const Vec4f& recip_dir)
{
	return Vec4f(
		((recip_dir.x[0] > 0 ? cell_w : 0) - (offset.x[0] - cell_orig.x[0])) * recip_dir.x[0],
		((recip_dir.x[1] > 0 ? cell_w : 0) - (offset.x[1] - cell_orig.x[1])) * recip_dir.x[1],
		((recip_dir.x[2] > 0 ? cell_w : 0) - (offset.x[2] - cell_orig.x[2])) * recip_dir.x[2],
		0
	);
}


// Returns true if next cell is in bounds
static inline bool nextCellIndex(const Vec3i& cell_i, const Vec3i& sign, const Vec3i& out, const Vec4f& next_t, const Vec4f& t_delta,
	Vec3i& next_cell_out, Vec4f& new_next_t/*, float& t_out*/)
{
	next_cell_out = cell_i;
	new_next_t = next_t;

	if(next_t.x[0] < next_t.x[1])
	{
		if(next_t.x[0] <  next_t.x[2])
		{
			// x intersect is nearest
			next_cell_out.x += sign.x; // Advance one cell in the x direction.
			if(next_cell_out.x == out.x) // If we have traversed out of the grid:
				return false;
			//t_out = new_next_t.x[0];
			new_next_t.x[0] += t_delta.x[0];
		}
		else
		{
			// z is nearest
			next_cell_out.z += sign.z; // Advance one cell in the z direction.
			if(next_cell_out.z == out.z) // If we have traversed out of the grid:
				return false;
			//t_out = new_next_t.x[0];
			new_next_t.x[2] += t_delta.x[2];
		}
	}
	else // else y <= x
	{
		if(next_t.x[1] < next_t.x[2])
		{
			// y intersect is nearest
			next_cell_out.y += sign.y; // Advance one cell in the y direction.
			if(next_cell_out.y == out.y) // If we have traversed out of the grid:
				return false;
			//t_out = new_next_t.x[1];
			new_next_t.x[1] += t_delta.x[1];
		}
		else // else t_z <= t_y
		{
			// z is nearest
			next_cell_out.z += sign.z; // Advance one cell in the z direction.
			if(next_cell_out.z == out.z) // If we have traversed out of the grid:
				return false;
			//t_out = new_next_t.x[2];
			new_next_t.x[2] += t_delta.x[2];
		}
	}
	return true;
}

/*
	// setup initial state
	node = root node
	cell = cellForOrigin(node)

	while cell is interior cell
		compute next adjacent cell index
		if(next is in bounds) push next cell index onto to-visit stack

		node = interior cell node
		cell = cellForOrigin(node)

	while(1)
	{

		// visit cell - trace against geometry etc..
		vistCell(node, cell)

		advance to adjacent cell

		if cell is out of bounds
			if to-visit stack is empty
				goto finished
			else
				(node, cell) = pop from to-visit stack

		while cell is interior cell
			compute next adjacent cell index
			if(next is in bounds) push next cell index onto to-visit stack

			node = interior cell node
			cell = cellForOrigin(node)
*/
void MultiLevelGrid::trace(const Ray& ray)
{
	StackElement stack[64];

	// Compute step increments, based on the sign of the ray direction.
	const int sign_x = ray.unitDir().x[0] >= 0.f ? 1 : -1;
	const int sign_y = ray.unitDir().x[1] >= 0.f ? 1 : -1;
	const int sign_z = ray.unitDir().x[2] >= 0.f ? 1 : -1;

	Vec3i sign(sign_x, sign_y, sign_z);

	// Compute the indices of the cell that we will traverse to when we step out of the grid bounds.
	const int out_x = ray.unitDir().x[0] >= 0.f ? MLG_RES : -1;
	const int out_y = ray.unitDir().x[1] >= 0.f ? MLG_RES : -1;
	const int out_z = ray.unitDir().x[2] >= 0.f ? MLG_RES : -1;

	Vec3i out(out_x, out_y, out_z);

	/*const Vec4f recip_dir(
		1 / dir.x[0],
		1 / dir.x[1],
		1 / dir.x[2],
		0
	);*/

	const Vec4f recip_dir = ray.getRecipRayDirF();


	// Init
	int node = 0; // Current node - set to root node
	int depth = 0; // depth of current node.
	int stack_size = 0; // Size of stack.
	float t = 0;
	Vec4f p = ray.startPos(); // Current point
	Vec4f cur_grid_orig = this->grid_orig; // Origin of current node.	
	Vec4f offset = ray.startPos() - cur_grid_orig; // Find the offset relative to the grid origin
	Vec3i cell = cellIndices(offset, recip_cell_width[depth]); // Get current cell indices from offset
	Vec4f cell_orig = cellOrigin(cur_grid_orig, cell, cell_width[depth]); // Get origin of current cell

	Vec4f next_t = nextT(offset, cell_orig, cell_width[depth], recip_dir); // distance along ray to next cell in each axis direction,
	// from the original ray origin.

	Vec4f t_delta = recip_dir * cell_width[depth]; // Step in t for a cell step for each axis.

	while(nodes[node].cellIsInterior(cell.x, cell.y, cell.z))
	{	
		// Get next cell
		Vec3i next_cell;
		Vec4f new_next_t;
		//float new_t;
		if(nextCellIndex(cell, sign, out, next_t, t_delta, next_cell, new_next_t))
		{
			// Push next cell onto stack
			stack[stack_size].node_index = node;
			stack[stack_size].node_depth = depth;
			stack[stack_size].cell_i = next_cell;
			stack[stack_size].grid_orig = cur_grid_orig;
			stack[stack_size].next_t = new_next_t;
			//stack[stack_size].t = new_t;
		}

		// Traverse to child node
		node = nodes[node].cellChildIndex(cell.x, cell.y, cell.z); // Set node index to child index.			
		cur_grid_orig = cellOrigin(cur_grid_orig, cell, cell_width[depth]); // Get child node grid origin
		depth++;
		offset = ray.startPos() - cur_grid_orig; // Find the offset relative to the grid origin
		cell = cellIndices(offset, recip_cell_width[depth]); // Get current cell indices from offset
		cell_orig = cellOrigin(cur_grid_orig, cell, cell_width[depth]); 
		next_t = nextT(offset, cell_orig, cell_width[depth], recip_dir);
		t_delta = recip_dir * cell_width[depth]; // Compute step in t for a cell step for each axis.
	}

	while(1)
	{
		// Visit cell
		conPrint("Visited cell " + cell.toString());

		// Advance to adjacent cell
		if(next_t.x[0] < next_t.x[1])
		{
			if(next_t.x[0] <  next_t.x[2])
			{
				// x intersect is nearest
				cell.x += sign_x; // Advance one cell in the x direction.
				if(cell.x == out_x) // If we have traversed out of the grid:
					goto out_of_bounds;
				t = next_t.x[0];
				next_t.x[0] += t_delta.x[0];
			}
			else
			{
				// z is nearest
				cell.z += sign_z; // Advance one cell in the z direction.
				if(cell.z == out_z) // If we have traversed out of the grid:
					goto out_of_bounds;
				t = next_t.x[2];
				next_t.x[2] += t_delta.x[2];
			}
		}
		else // else y <= x
		{
			if(next_t.x[1] < next_t.x[2])
			{
				// y intersect is nearest
				cell.y += sign_y; // Advance one cell in the y direction.
				if(cell.y == out_y) // If we have traversed out of the grid:
					goto out_of_bounds;
				t = next_t.x[1];
				next_t.x[1] += t_delta.x[1];
			}
			else // else t_z <= t_y
			{
				// z is nearest
				cell.z += sign_z; // Advance one cell in the z direction.
				if(cell.z == out_z) // If we have traversed out of the grid:
					goto out_of_bounds;
				t = next_t.x[2];
				next_t.x[2] += t_delta.x[2];
			}
		}
		goto valid_node_cell;

out_of_bounds:
		if(stack_size == 0)
			break;
		else
		{
			// pop from to-visit stack
			stack_size--;
			node = stack[stack_size].node_index;
			cell = stack[stack_size].cell_i;
			depth = stack[stack_size].node_depth;
			cur_grid_orig = stack[stack_size].grid_orig;
			next_t = stack[stack_size].next_t;
		}

valid_node_cell:

		// Compute current position
		p = ray.pointf(t);

		while(nodes[node].cellIsInterior(cell.x, cell.y, cell.z))
		{	
			// Get next cell
			Vec3i next_cell;
			Vec4f new_next_t;
			if(nextCellIndex(cell, sign, out, next_t, t_delta, next_cell, new_next_t))
			{
				// Push next cell onto stack
				stack[stack_size].node_index = node;
				stack[stack_size].node_depth = depth;
				stack[stack_size].cell_i = next_cell;
				stack[stack_size].grid_orig = cur_grid_orig;
				stack[stack_size].next_t = new_next_t;
			}

			// Traverse to child node
			node = nodes[node].cellChildIndex(cell.x, cell.y, cell.z); // Set node index to child index.			
			cur_grid_orig = cellOrigin(cur_grid_orig, cell, cell_width[depth]); // Get node grid origin
			depth++;
			offset = p - cur_grid_orig; // Find the offset relative to the grid origin
			cell = cellIndices(offset, recip_cell_width[depth]); // Get current cell indices from offset
			cell_orig = cellOrigin(cur_grid_orig, cell, cell_width[depth]); 
			next_t = nextT(offset, cell_orig, cell_width[depth], recip_dir);
			t_delta = recip_dir * cell_width[depth]; // Compute step in t for a cell step for each axis.
		} // End while cell is interior cell
	}
}
