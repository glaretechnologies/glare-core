/*=====================================================================
SimpleBVHNode.h
---------------
File created by ClassTemplate on Mon Oct 27 20:35:13 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __SIMPLEBVHNODE_H_666_
#define __SIMPLEBVHNODE_H_666_


#include "jscol_aabbox.h"


namespace js
{


/*=====================================================================
SimpleBVHNode
-------------

=====================================================================*/
class SimpleBVHNode
{
public:
	inline SimpleBVHNode() { assert(sizeof(SimpleBVHNode) == 96); }
	inline ~SimpleBVHNode() {}

	inline static size_t requiredAlignment() { return 128; }

	SSE_ALIGN AABBox left_aabb; // 32 bytes
	SSE_ALIGN AABBox right_aabb; // 32 bytes

	int left_child_index;
	int right_child_index;

	int leaf;
	int num_geom;
	int geometry_index;

	int padding[3];
};


} //end namespace js


#endif //__SIMPLEBVHNODE_H_666_
