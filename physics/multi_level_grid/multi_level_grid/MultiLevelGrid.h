#pragma once


#include "MultiLevelGridNode.h"
#include "Vec3.h"
#include <vector>


class StackElement
{
public:
	unsigned int node_index;
	int i, j, k; // Current cell indices.
	// Should be 0 <= i < 4
	float cell_width;
	Vec3 grid_orig;
};


class MultiLevelGrid
{
public:
	MultiLevelGrid();
	~MultiLevelGrid();

	void trace(const Vec3& orig, const Vec3& dir);

//private:
	Vec3 grid_orig;
	//Vec3 grid_dims;
	float cell_width;
	StackElement stack[64];
	std::vector<MultiLevelGridNode> nodes;
};
