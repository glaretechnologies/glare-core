/*=====================================================================
PointMLG.h
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2012-08-18 21:52:10 +0100
=====================================================================*/
#pragma once


#include "MultiLevelGrid.h"
#include "../maths/vec3.h"
#include "../indigo/globals.h"
#include "../utils/stringutils.h"


// A point and some associated data
template <class PointData>
struct Point
{
	Vec3f pos;
	PointData data;
};


// Each MLG node has one of these.  Contains a few points.
template <class PointData>
struct PointMLGNodeData
{
	int num_points;
	Point<PointData> points[4];
};


class PointMLGCellTest
{};


class PointMLGResult
{};


void doPointMLGTests();


/*=====================================================================
PointMLG
-------------------


=====================================================================*/
template <class PointData>
class PointMLG
{
public:
	typedef MultiLevelGridNode<PointMLGNodeData<PointData> > NodeType;

	inline PointMLG(const Vec4f& min, const Vec4f& max);
	inline ~PointMLG();


	inline void build(const std::vector<Point<PointData> >& points);


	inline void getPointsInRadius(const Vec4f& query_p, float radius, std::vector<Point<PointData> >& points_out) const;


private:
	inline void doBuild(int node_i, const Vec4f& node_min, std::vector<Point<PointData> >& points, int begin, int end, std::vector<Point<PointData> >& temp, int depth);

	inline void doGetPointsInRadius(const Vec4f& query_p, float radius, int node_i, const Vec4f& node_min, int depth, std::vector<Point<PointData> >& points_out) const;
public:
	MultiLevelGrid<PointMLGCellTest, PointMLGResult, PointMLGNodeData<PointData> >* grid;

	// Build stats
	int max_depth;
};



template <class PointData>
PointMLG<PointData>::PointMLG(const Vec4f& min, const Vec4f& max)
{
	grid = new MultiLevelGrid<PointMLGCellTest, PointMLGResult, PointMLGNodeData<PointData> >(min, max, PointMLGCellTest());

	// Init build stats
	max_depth = 0;
}


template <class PointData>
PointMLG<PointData>::~PointMLG()
{
	delete grid;
}


template <class PointData>
void PointMLG<PointData>::build(const std::vector<Point<PointData> >& points_)
{
	// Form a copy of points
	std::vector<Point<PointData> > points = points_;

	std::vector<Point<PointData> > temp(points.size());

	// Create root node
	grid->nodes.push_back(MultiLevelGridNode<PointMLGNodeData<PointData> >());

	doBuild(0, grid->aabb.min_, points, 0, (int)points.size(), temp, 0);
}


template <class PointData>
void PointMLG<PointData>::doBuild(int node_i, const Vec4f& node_min, std::vector<Point<PointData> >& points, int begin, int end, std::vector<Point<PointData> >& temp, int depth)
{
	/*
	//TEMP: Disabled because SSE4 intrinsics are causing errors on Linux and OS X.

	// Update build stats
	max_depth = myMax(max_depth, depth);


	//TEMP: Check that all points allocated to this node are inside the node bounds
	for(int i=begin; i<end; ++i)
	{
		//TEMPassert(points[i].pos.x >= node_min.x[0] && points[i].pos.y >= node_min.x[1] && points[i].pos.z >= node_min.x[2]);
	}


	assert(depth < 64);

	if(end - begin <= 4)
	{
		// Don't create any more child nodes.
		grid->nodes[node_i].data.num_points = end - begin;

		for(int i=begin; i<end; ++i)
			grid->nodes[node_i].data.points[i - begin] = points[i];
		
		return;
	}

	// Don't store any points in this node directly
	grid->nodes[node_i].data.num_points = 0;


	// Compute bucket counts
	int bucket_counts[64];
	for(int i=0; i<64; ++i)
		bucket_counts[i] = 0;

	for(int i=begin; i!=end; ++i)
	{
		// Get bucket for point
		Vec4f offset = points[i].pos.toVec4fPoint() - node_min;

		//assert(offset.x[0] >= 0 && offset.x[1] >= 0 && offset.x[2] >= 0);

		Vec4i cell = floorToVec4i(offset * grid->recip_cell_width[depth]); // Get current cell indices from offset

		cell = clamp(cell, Vec4i(0), Vec4i(MLG_RES - 1)); // Clamp cell indices

		int cell_i = cell.x[2]*16 + cell.x[1]*4 + cell.x[0];

		bucket_counts[cell_i]++;
	}

	// Compute bucket offsets.
	// Note that we will map the range [begin, end) in points to [0, end - begin) in temp
	int bucket_offsets[64];
	int original_bucket_offsets[64];
	int offset = 0;
	for(int i=0; i<64; ++i)
	{
		bucket_offsets[i] = original_bucket_offsets[i] = offset;
		offset += bucket_counts[i];
	}

	// Copy data into temp at correct bucket location
	for(int i=begin; i!=end; ++i)
	{
		// Get bucket for point
		Vec4f offset = points[i].pos.toVec4fPoint() - node_min;

		Vec4i cell = floorToVec4i(offset * grid->recip_cell_width[depth]); // Get current cell indices from offset

		cell = clamp(cell, Vec4i(0), Vec4i(MLG_RES - 1)); // Clamp cell indices

		int cell_i = cell.x[2]*16 + cell.x[1]*4 + cell.x[0];

		// Copy i to the correct place in temp
		temp[bucket_offsets[cell_i]] = points[i];

		// Increment offset so next point gets placed one place to the right
		bucket_offsets[cell_i]++;
	}

	// Copy temp back to relevant range of points
	for(int i=begin; i!=end; ++i)
		points[i] = temp[i - begin];

	bool have_created_child = false;

	int child_indices[64];

	// Create any child nodes first before recursion, as they have to be packed together.
	for(int c=0; c<64; ++c)
	{
		int cell_begin = original_bucket_offsets[c];
		int cell_end = bucket_offsets[c];

		if(cell_end - cell_begin > 0)
		{
			int child_node_i = (int)grid->nodes.size();
			child_indices[c] = child_node_i;
			grid->nodes.push_back(NodeType());

			if(!have_created_child)
			{
				grid->nodes[node_i].setBaseChildIndex(child_node_i);
				have_created_child = true;
			}
		}
	}
				


	// For each set of points allocated to each bucket, recurse.
	for(int c=0; c<64; ++c)
	{
		int cell_begin = original_bucket_offsets[c] + begin;
		int cell_end = bucket_offsets[c] + begin;

		if(cell_end - cell_begin > 0)
		{
			// If there was at least one point in this cell:

			// Compute child node position
			int z = (c & 0x00000030) >> 4;
			int y = (c & 0x0000000C) >> 2;
			int x = (c & 0x00000003) >> 0;

			// TEMP: check that we got the indices correct
			assert(z*16 + y*4 + x == c);

			const float W = grid->cell_width[depth];
			Vec4f child_node_min = node_min + Vec4f(W * x, W * y, W * z, 0);

			// Mark the cell as an interior cell
			grid->nodes[node_i].markCellAsInterior(Vec4i(x, y, z, 0));

			doBuild(child_indices[c], child_node_min, points, cell_begin, cell_end, temp, depth + 1);
		}
	}*/
}

template <class PointData>
void PointMLG<PointData>::getPointsInRadius(const Vec4f& query_p, float radius, std::vector<Point<PointData> >& points_out) const
{
	//conPrint("getPointsInRadius(), query_p: " + query_p.toString() + ", radius: " + toString(radius) + "");

	points_out.resize(0);

	doGetPointsInRadius(
		query_p, 
		radius, 
		0, // node_i
		grid->aabb.min_, // node_min
		0, // depth
		points_out
	);
}


template <class PointData>
void PointMLG<PointData>::doGetPointsInRadius(const Vec4f& p, float r, int node_i, const Vec4f& node_min, int depth, std::vector<Point<PointData> >& points_out) const
{
#if 0
	//TEMP: Disabled because SSE4 intrinsics are causing errors on Linux and OS X.

	//conPrint("doGetPointsInRadius(), node_i: " + toString(node_i) + ", depth: " + toString(depth) + "");

	int num_node_points = grid->nodes[node_i].data.num_points;

	if(num_node_points > 0)
	{
		// Check all the points
		const float r2 = r*r;

		const Vec3f query_p(p.x[0], p.x[1], p.x[2]);

		for(int i=0; i<num_node_points; ++i) // For each point in the node
			if(query_p.getDist2(grid->nodes[node_i].data.points[i].pos) <= r2) // If it has distance <= r to query point
				points_out.push_back(grid->nodes[node_i].data.points[i]); // Add it to list of points found within query radius.6
	}
	else
	{

		/*
			start_x = [(p.x - node_min.x) - r] / W
			start_xi = max(0, floor(start_x))

			end_x = (p.x - node_min.x) + r] / W
			end_xi = min(RES - 1, floor(end_x) + 1)

		*/

		const float W = grid->cell_width[depth];
		const float recip_W = grid->recip_cell_width[depth];

		Vec4f start = ((p - node_min) - Vec4f(r, r, r, 0)) * recip_W;
		Vec4i start_i = max(Vec4i(0), floorToVec4i(start));

		Vec4f end = ((p - node_min) + Vec4f(r, r, r, 0)) * recip_W;
		Vec4i end_i = min(Vec4i(MLG_RES), floorToVec4i(end) + Vec4i(1,1,1,0));

		//conPrint("doGetPointsInRadius(), node_i: " + toString(node_i) + ", depth: " + toString(depth) + "");
		/*printVar(start_i.x);
		printVar(start_i.y);
		printVar(start_i.z);
		printVar(end_i.x);
		printVar(end_i.y);
		printVar(end_i.z);*/

		for(int z=start_i.x[2]; z<end_i.x[2]; ++z)
		for(int y=start_i.x[1]; y<end_i.x[1]; ++y)
		for(int x=start_i.x[0]; x<end_i.x[0]; ++x)
		{
			const Vec4i cell_i(x, y, z, 0);
			if(grid->nodes[node_i].cellIsInterior(cell_i))
			{
				int child_index = grid->nodes[node_i].cellChildIndex(cell_i);

				// Compute child node min
				Vec4f child_node_min = node_min + Vec4f(W * x, W * y, W * z, 0);

				// Recurse
				//conPrint("Recursing..");
				//printVar(depth);
				doGetPointsInRadius(p, r, child_index, child_node_min, depth + 1, points_out);
			}
		}
	}
#endif
}
