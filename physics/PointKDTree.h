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
#include <limits>
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

	inline static uint32 notFoundIndex() { return std::numeric_limits<uint32>::max(); }

	uint32 getNearestPoint(const Vec3f& p, ThreadContext& thread_context) const;

	uint32 getNearestPointDebug(const std::vector<Vec3f>& points, const Vec3f& p, ThreadContext& thread_context) const;


	float getNearestNPointsRadius(ThreadContext& thread_context, int N, const Vec3f& p, float min_radius, float max_radius, int& actual_num_out) const;



	void printTree(std::ostream& stream, unsigned int node_index, unsigned int depth) const;

private:
	class LayerInfo
	{
	public:
		std::vector<AxisPoint> axis_points[3];
	};

	void checkLayerLists(const std::vector<Vec3f>& points, LayerInfo& layer);
	void build(const std::vector<Vec3f>& points);

	void doBuild(const std::vector<Vec3f>& points, int depth, int max_depth);
	

	
	std::vector<LayerInfo> layers;

	typedef std::vector<PointKDTreeNode> NODE_VECTOR_TYPE;

	NODE_VECTOR_TYPE nodes;
};
