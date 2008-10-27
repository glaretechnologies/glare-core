/*=====================================================================
NBVHNode.h
----------
File created by ClassTemplate on Sun Oct 26 01:59:58 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __NBVHNODE_H_666_
#define __NBVHNODE_H_666_


#include "jscol_aabbox.h"


namespace js
{


/*=====================================================================
NBVHNode
--------

=====================================================================*/
class NBVHNode
{
public:
	/*=====================================================================
	NBVHNode
	--------
	
	=====================================================================*/
	NBVHNode();
	~NBVHNode();

	//float aabb_bounds[24]; // 4 bounding boxes * 3 dimensions * 2 boundaries per dimension.
	//int child[4]; // indices of child nodes, or if negative, index into leaf geometry array.
	//int axis0, axis1, axis2;
	//int fill;
};


class NBVHBuildNode
{
public:
	Vec3f min;
	Vec3f max;

	int left_child_index;
	int right_child_index;
	
	int geometry_index;
	int num_geom;

	int leaf;
};


} //end namespace js


#endif //__NBVHNODE_H_666_
