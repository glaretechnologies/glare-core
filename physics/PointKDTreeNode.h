/*=====================================================================
PointKDTreeNode.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Apr 07 12:57:48 +1200 2010
=====================================================================*/
#pragma once


#include "../utils/platform.h"
#include "../maths/vec3.h"


/*=====================================================================
PointKDTreeNode
-------------------
NOTE: completely unoptimised.
=====================================================================*/
class PointKDTreeNode
{
public:
	PointKDTreeNode(uint32 left_, uint32 right_) : left(left_), right(right_) {}
	~PointKDTreeNode(){}

	uint32 left;
	uint32 right;
	Vec3f point;
	uint32 point_index;
	uint32 split_axis;
	float split_val;
	uint32 is_leaf;
private:
};
