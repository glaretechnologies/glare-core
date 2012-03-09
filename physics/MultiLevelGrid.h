#pragma once


#include "MultiLevelGridNode.h"
#include "../simpleraytracer/ray.h"
#include "../maths/Vec4f.h"
#include "../maths/vec3.h"
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
	Vec3<int> cell_i;
	//int i, j, k; // Current cell indices.
	// Should be 0 <= i < 4
//	float cell_width;
	Vec4f grid_orig;
	Vec4f next_t;
	//float t; // Distance along original r for point where ray enters (node, cell).
};


class MultiLevelGrid
{
public:
	MultiLevelGrid(const Vec4f& min, const Vec4f& max);
	~MultiLevelGrid();

	void trace(const Ray& ray);

//private:
	Vec4f grid_orig;
	//Vec3 grid_dims;
	//float cell_width;
	//float recip_cell_width;
	std::vector<MultiLevelGridNode> nodes;
	std::vector<float> cell_width; // cell width for depth
	std::vector<float> recip_cell_width; // cell width for depth
};
