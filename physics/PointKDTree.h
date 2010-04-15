/*=====================================================================
PointKDTree.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Apr 07 12:56:37 +1200 2010
=====================================================================*/
#pragma once


#include "PointKDTreeNode.h"
#include "../utils/platform.h"
#include "../maths/vec3.h"
#include <vector>
class ThreadContext;


/*=====================================================================
PointKDTree
-------------------

=====================================================================*/
class PointKDTree
{
public:
	PointKDTree(const std::vector<Vec3f>& points);
	~PointKDTree();

	class AxisPoint
	{
	public:
		float val;
		uint32 point_index;
	};


	uint32 getNearestPoint(const Vec3f& p, ThreadContext& thread_context) const;


	void printTree(std::ostream& stream, unsigned int node_index, unsigned int depth) const;

private:
	void build(const std::vector<Vec3f>& points);

	void doBuild(const std::vector<Vec3f>& points, int depth, int max_depth);
	

	class LayerInfo
	{
	public:
		std::vector<AxisPoint> axis_points[3];
	};
	std::vector<LayerInfo> layers;

	typedef std::vector<PointKDTreeNode> NODE_VECTOR_TYPE;

	NODE_VECTOR_TYPE nodes;
};
