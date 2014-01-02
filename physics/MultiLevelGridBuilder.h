#pragma once


#include "MultiLevelGrid.h"

/*
Does a kind of standard SAH based build, using primitive AABBs, of a multi-level grid.
*/
template <class CellTest, class Result, class NodeData, class BuildCallBacks> 
class MultiLevelGridBuilder
{
public:

	inline MultiLevelGridBuilder(MultiLevelGrid<CellTest, Result, NodeData>& grid_) : grid(grid_) {}
	inline ~MultiLevelGridBuilder() {}

	inline void build(const js::Vector<js::AABBox, 64>& primitive_aabbs, BuildCallBacks& callbacks);

	const static int MAX_DEPTH = 64;

private:
	inline void doBuild(const js::Vector<js::AABBox, 64>& primitive_aabbs, const std::vector<uint32>& primitives, uint32 node_index,
		const Vec4f& node_orig, int depth, BuildCallBacks& callbacks);

	MultiLevelGrid<CellTest, Result, NodeData>& grid;
};


template <class CellTest, class Result, class NodeData, class BuildCallBacks>
void MultiLevelGridBuilder<CellTest, Result, NodeData, BuildCallBacks>::build(const js::Vector<js::AABBox, 64>& primitive_aabbs, BuildCallBacks& callbacks)
{
	std::vector<uint32> primitives(primitive_aabbs.size());
	for(size_t i=0; i<primitive_aabbs.size(); ++i)
		primitives[i] = (uint32)i;

	grid.nodes.push_back(MultiLevelGridNode<NodeData>());
	doBuild(primitive_aabbs, 
		primitives,
		0, // node index
		grid.aabb.min_, 
		0, // depth
		callbacks
	);
}


/*
process_node(primitives)

	// for each cell, a list of primitives
	list cell_primitives[64]

	For each primitive

		// Rasterise primitive:
		for(z = floor((prim.aabb.min.z - node.min.z) / cell_w) z <= floor(prim.aabb.max); ++z)
		for(y ...  )
		for(x ...  )
			add primitive to cell list

	for each cell
		if cell primitive list size > threshold
			process_node(cell primitive list)   // Recurse
		else
			// Make this cell a leaf
end

*/
template <class CellTest, class Result, class NodeData, class BuildCallBacks>
void MultiLevelGridBuilder<CellTest, Result, NodeData, BuildCallBacks>::doBuild(
				const js::Vector<js::AABBox, 64>& primitive_aabbs, // AABBs of all primitives
				const std::vector<uint32>& node_primitives, // Indices of primitives that have been assigned to this node
				uint32 node_index, // Index of the current node
				const Vec4f& node_orig, // Origin of the current node (min of AABB)
				int depth, // Current recursion depth.  0 means this is the root node.
				BuildCallBacks& callbacks) // callbacks for markNodeAsLeaf().
{
	const size_t INTERIOR_THRESHOLD = 2;

	if(node_primitives.size() <= INTERIOR_THRESHOLD)
	{
		// Mark this node as a leaf node
		callbacks.markNodeAsLeaf(primitive_aabbs, node_primitives, node_index, node_orig, depth);
	}
	else
	{
		// This node is an interior node
		std::vector<uint32> cell_primitives[64];

		for(size_t i=0; i<node_primitives.size(); ++i) // For each primitive assigned to this node:
		{
			const js::AABBox& prim_aabb = primitive_aabbs[node_primitives[i]]; // Get AABB of primitive.

			// Rasterise primitive:
			int z_begin = myMax(0, (int)((prim_aabb.min_.x[2] - node_orig.x[2]) * grid.recip_cell_width[depth]));
			int z_end = myMin(MLG_RES - 1, (int)((prim_aabb.max_.x[2] - node_orig.x[2]) * grid.recip_cell_width[depth]));

			for(int z=z_begin; z<=z_end; ++z)
			{
				int y_begin = myMax(0, (int)((prim_aabb.min_.x[1] - node_orig.x[1]) * grid.recip_cell_width[depth]));
				int y_end = myMin(MLG_RES - 1, (int)((prim_aabb.max_.x[1] - node_orig.x[1]) * grid.recip_cell_width[depth]));

				for(int y=y_begin; y<=y_end; ++y)
				{
					int x_begin = myMax(0, (int)((prim_aabb.min_.x[0] - node_orig.x[0]) * grid.recip_cell_width[depth]));
					int x_end = myMin(MLG_RES - 1, (int)((prim_aabb.max_.x[0] - node_orig.x[0]) * grid.recip_cell_width[depth]));

					for(int x=x_begin; x<=x_end; ++x)
					{
						int cell_i = x + MLG_RES * (y + MLG_RES * z);

						// Add primitive to list for cell.
						cell_primitives[cell_i].push_back(node_primitives[i]);
					}
				}
			}
		}

	


		int num_children = 0;

		// Create any child nodes first before recursion, as they have to be packed together.
		int child_indices[64];

		// For each cell
		for(int z=0; z<MLG_RES; ++z)
		for(int y=0; y<MLG_RES; ++y)
		for(int x=0; x<MLG_RES; ++x)
		{
			int cell_i = x + MLG_RES * (y + MLG_RES * z);
			if(!cell_primitives[cell_i].empty())
			{
				if(num_children == 0)
					grid.nodes[node_index].setBaseChildIndex((uint32)grid.nodes.size());
			
				// Remember the index of the node
				child_indices[cell_i] = (int)grid.nodes.size();

				grid.nodes.push_back(MultiLevelGridNode<NodeData>()); // Create child node
				num_children++;
			}
		}

		// Recurse
		// For each cell
		for(int z=0; z<MLG_RES; ++z)
		for(int y=0; y<MLG_RES; ++y)
		for(int x=0; x<MLG_RES; ++x)
		{
			int cell_i = x + MLG_RES * (y + MLG_RES * z);
			if(!cell_primitives[cell_i].empty()) //  >= INTERIOR_THRESHOLD)
			{
				const Vec4f child_origin = node_orig + Vec4f((float)x, (float)y, (float)z, 0) * grid.cell_width[depth];
				doBuild(primitive_aabbs, cell_primitives[cell_i], child_indices[cell_i], child_origin, depth + 1, callbacks);
			}
		}
	}
}
