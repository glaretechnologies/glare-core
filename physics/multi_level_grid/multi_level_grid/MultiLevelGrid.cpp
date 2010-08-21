#include "MultiLevelGrid.h"


#include <iostream>


MultiLevelGrid::MultiLevelGrid(void)
{

	
}


MultiLevelGrid::~MultiLevelGrid(void)
{
}






const float recip_res = 1.f / (float)RES;


void MultiLevelGrid::trace(const Vec3& orig, const Vec3& dir)
{
	//stack.resize(0);
	//stack.reserve(10);
	int stack_size = 0;

	const Vec3 recip_dir(
		1.f / dir.x,
		1.f / dir.y,
		1.f / dir.z
	);
	const int sign_x = dir.x >= 0.f ? 1 : -1;
	const int sign_y = dir.y >= 0.f ? 1 : -1;
	const int sign_z = dir.z >= 0.f ? 1 : -1;

	// Build stack of current position.
	int node = 0;
	Vec3 cur_grid_orig = this->grid_orig;
	float cur_cell_width = this->cell_width;
	int cell_i, cell_j, cell_k; // Indices of current cell in parent cell.
	Vec3 offset; // offset into current cell (i, j, k)
	float t_x, t_y, t_z;
	float t = 0.f;
	Vec3 cur_orig = orig;

	// Walk down grid tree until we hit a non-interior cell.
	while(1)
	{
		// Compute cell indices at this level
		Vec3 parent_offset = orig - cur_grid_orig;

		cell_i = (int)(parent_offset.x / cur_cell_width);
		cell_j = (int)(parent_offset.y / cur_cell_width);
		cell_k = (int)(parent_offset.z / cur_cell_width);

		offset.x = parent_offset.x - cell_i*cur_cell_width;
		offset.y = parent_offset.y - cell_j*cur_cell_width;
		offset.z = parent_offset.z - cell_k*cur_cell_width;


		if(nodes[node].cellIsInterior(cell_i, cell_j, cell_k))
		{
			// Push this cell coords onto the stack.
			//	stack.push_back(StackElement());
			stack[stack_size].node_index = node;
			stack[stack_size].i = cell_i;
			stack[stack_size].j = cell_j;
			stack[stack_size].k = cell_k;
			stack[stack_size].cell_width = cur_cell_width;
			stack[stack_size].grid_orig = cur_grid_orig;
			stack_size++;


			// Compute child cell origin
			cur_grid_orig.x += (float)cell_i * cur_cell_width;
			cur_grid_orig.y += (float)cell_j * cur_cell_width;
			cur_grid_orig.z += (float)cell_k * cur_cell_width;

			//  Compute child cell width
			cur_cell_width /= RES;

			// Set node index to child index.
			node = nodes[node].cellChildIndex(cell_i, cell_j, cell_k);

			std::cout << "Traversed to interior node " << node << std::endl;
		}
		else
			break;
	}


	while(1)
	{
popped:
		// Walk down grid tree until we hit a non-interior cell.
		// Compute cell indices at this level
		cur_orig = orig + dir * t;
		//Vec3 parent_offset = cur_orig - cur_grid_orig;

		//cell_i = (int)(parent_offset.x / cur_cell_width);
		//cell_j = (int)(parent_offset.y / cur_cell_width);
		//cell_k = (int)(parent_offset.z / cur_cell_width);

		//offset.x = parent_offset.x - cell_i*cur_cell_width;
		//offset.y = parent_offset.y - cell_j*cur_cell_width;
		//offset.z = parent_offset.z - cell_k*cur_cell_width;


		while(nodes[node].cellIsInterior(cell_i, cell_j, cell_k))
		{
			// Push this cell coords onto the stack.
			//	stack.push_back(StackElement());
			stack[stack_size].node_index = node;
			stack[stack_size].i = cell_i;
			stack[stack_size].j = cell_j;
			stack[stack_size].k = cell_k;
			stack[stack_size].cell_width = cur_cell_width;
			stack[stack_size].grid_orig = cur_grid_orig;
			stack_size++;


			// Compute child cell origin
			cur_grid_orig.x += (float)cell_i * cur_cell_width;
			cur_grid_orig.y += (float)cell_j * cur_cell_width;
			cur_grid_orig.z += (float)cell_k * cur_cell_width;

			//  Compute child cell width
			cur_cell_width /= RES;

			// Set node index to child index.
			node = nodes[node].cellChildIndex(cell_i, cell_j, cell_k);

			Vec3 parent_offset = orig - cur_grid_orig;

			cell_i = (int)(parent_offset.x / cur_cell_width);
			cell_j = (int)(parent_offset.y / cur_cell_width);
			cell_k = (int)(parent_offset.z / cur_cell_width);

			offset.x = parent_offset.x - cell_i*cur_cell_width;
			offset.y = parent_offset.y - cell_j*cur_cell_width;
			offset.z = parent_offset.z - cell_k*cur_cell_width;

			std::cout << "Traversed to interior node " << node << std::endl;
		}

		//if(nodes[node].cellIsNonEmptyLeaf(cell_i, cell_j, cell_k))
		//{
		//	// Intersect against objects in cell (i, j, k)
		//}
		//else
		//{
		//	// This cell is empty. So do nothing with this cell, and
		//	// continue traversing along ray.
		//}

		
		t_x = dir.x >= 0.f ? 
			(cur_cell_width - offset.x) * recip_dir.x :
			( -offset.x) * recip_dir.x;
		t_y = dir.y >= 0.f ? 
			(cur_cell_width - offset.y) * recip_dir.y :
			( -offset.y) * recip_dir.y;
		t_z = dir.z >= 0.f ? 
			(cur_cell_width - offset.z) * recip_dir.z :
			( -offset.z) * recip_dir.z;

advance_cell:
		// Advance to next adjacent cell. 
		int last_move_axis;
		//float last_move_t;
		if(t_x < t_y)
		{
			if(t_x < t_z)
			{
				// x intersect is nearest
				cell_i += sign_x; // Advance one cell in the x direction.
				t += t_x; //last_move_t = t_x;
				t_y -= t_x;
				t_z -= t_x;
				t_x = cur_cell_width * recip_dir.x;

				last_move_axis = 0;

				//if(cell_i < 0 || cell_i >= RES)
				//{
				//}
			}
			else
			{
				// z is nearest
				cell_k += sign_z; // Advance one cell in the z direction.
				t += t_z; // last_move_t = t_z;
				t_x -= t_z;
				t_y -= t_z;
				t_z = cur_cell_width * recip_dir.z;

				last_move_axis = 2;
				//if(cell_k < 0 || cell_k >= RES)
				//	goto pop_grid;
			}
		}
		else
		{
			// y <= x
			if(t_y < t_z)
			{
				// y intersect is nearest
				cell_j += sign_y; // Advance one cell in the y direction.
				t += t_y; // last_move_t = t_y;
				t_x -= t_y;
				t_z -= t_y;
				t_y = cur_cell_width * recip_dir.y;

				last_move_axis = 1;
				//if(cell_j < 0 || cell_j >= RES)
				//	goto pop_grid;
			}
			else // else t_z <= t_y
			{
				// z intersect is nearest
				cell_k += sign_z; // Advance one cell in the z direction.
				t += t_z; // last_move_t = t_z;
				t_x -= t_z;
				t_y -= t_z;
				t_z = cur_cell_width * recip_dir.z;

				//if(cell_k < 0 || cell_k >= RES)
				//	goto pop_grid;

				last_move_axis = 2;
			}
		}

		
		std::cout << "Advanced to node " << node << ", cell (" << cell_i << ", " << cell_j << ", " << cell_k << ")" << std::endl; 

		// Walk up tree
		// While cell is out of bounds...
		bool out_of_bounds = false;
		while(	cell_i < 0 || cell_i >= RES ||
				cell_j < 0 || cell_j >= RES ||
				cell_k < 0 || cell_k >= RES)
		{
			std::cout << "Out of bounds." << std::endl;

			// Pop node off stack
			if(stack_size == 0)//stack.empty())
				goto finished; 
			stack_size--;
			node = stack[stack_size].node_index;
			cell_i = stack[stack_size].i;
			cell_j = stack[stack_size].j;
			cell_k = stack[stack_size].k;

			std::cout << "Popped node " << node << ", cell (" << cell_i << ", " << cell_j << ", " << cell_k << ") off stack." << std::endl;
			
			if(last_move_axis == 0)
				cell_i += sign_x;
			else if(last_move_axis == 1)
				cell_j += sign_y;
			else if(last_move_axis == 2)
				cell_k += sign_z;

			cur_cell_width = stack[stack_size].cell_width;
			cur_grid_orig = stack[stack_size].grid_orig;
			out_of_bounds = true;
		}

		if(out_of_bounds)
		{
			// Advance ray origin
			goto popped;
		}
		else
		{
			//t += last_move_t;
			goto advance_cell;
		}



		//// We are at a new cell in the current grid.
		//if(nodes[node].cellIsInterior(cell_i, cell_j, cell_k))
		//{
		//	// Push this grid onto the stack.
		//	stack.push_back();
		//	stack.back().node_index = node;
		//	stack.back().i = cell_i;
		//	stack.back().j = cell_i;
		//	stack.back().k = cell_k;

		//	// Traverse to the child cell grid.
		//	node = nodes[node].childIndex(cell_i, cell_j, cell_k);
		//}
		//else 
	}	

finished:
	return;
}


/*

	Get cell (i, j, k) at top level grid
	while(cell(i, j, k) is an interior grid cell))
	{
		push(i, j, k) onto stack
		compute child i, j, k
		current = child (i, j, k)
	}

	// 

	// Initialise stack

	while(stack is not empty)
	{
		current_grid = pop top node off stack

		while(inside current grid)
		{
			// step to next cell
		



		
	while(1)

		Walk down tree:
		while cell is interior
			Push this grid onto stack
			goto child grid

		if cell is leaf
			Process leaf grom
		else if cell is empty
			Do nothing

		step to next cell in the grid

		Walk up tree
			while cell is out of grid bounds
				pop top node off stack, advance to adjacent cell due to last move

			





*/