/*=====================================================================
BVHNode.h
---------
File created by ClassTemplate on Sun Oct 26 17:19:33 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __BVHNODE_H_666_
#define __BVHNODE_H_666_


#include "jscol_aabbox.h"


namespace js
{




/*=====================================================================
BVHNode
-------

=====================================================================*/
class BVHNode
{
public:
	/*=====================================================================
	BVHNode
	-------
	
	=====================================================================*/
	inline BVHNode() { assert(sizeof(BVHNode) == 64); }

	inline ~BVHNode() {}

	inline static size_t requiredAlignment() { return 64; }


	/*union LeftType
	{
		AABBox left_aabb; // 32 bytes
		float left_data[8];
	};*/

	SSE_ALIGN AABBox left_aabb; // 32 bytes
	SSE_ALIGN AABBox right_aabb; // 32 bytes
	/*Vec3f left_aabb_min;
	int left_child_index; // If negative, then -left_child_index is the index into the geometry array.
	Vec3f right_aabb;
	int right_child_index; // If negative, then -right_child_index is the index into the geometry array.*/

	inline void setLeftChildIndex(unsigned int i)
	{
		*((unsigned int*)&left_aabb.min_.padding) = i;
	}

	inline void setRightChildIndex(unsigned int i)
	{
		*((unsigned int*)&right_aabb.min_.padding) = i;
	}

	inline unsigned int getLeftChildIndex() const
	{
		return *((unsigned int*)&left_aabb.min_.padding);
	}

	inline unsigned int getRightChildIndex() const
	{
		return *((unsigned int*)&right_aabb.min_.padding);
	}

	inline unsigned int getLeftGeomIndex() const
	{
		return getLeftChildIndex() & 0x7FFFFFFF;
	}

	inline unsigned int getRightGeomIndex() const
	{
		return getRightChildIndex() & 0x7FFFFFFF;
	}

	inline void setLeftGeomIndex(unsigned int x)
	{
		return setLeftChildIndex(x | 0x80000000);
	}

	inline void setRightGeomIndex(unsigned int x)
	{
		return setRightChildIndex(x | 0x80000000);
	}


	/*int leaf;
	union
	{
		int left_child_index;
		int num_geom;
	};
	int right_child_index;
	int geometry_index;
	//int padding2;

	int padding[4];*/
};



} //end namespace js


#endif //__BVHNODE_H_666_




