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

float left_min[3]
float left_min_padding    // left child index if interior, geom index if leaf.
float left_max[3]
float left_max_padding    // right child index if interior, num geom if leaf
float right_min[3]
float right_min_padding   // 1 if leaf node, 0 otherwise
float right_max[3]
float right_max_padding

=====================================================================*/
#if 0
class SimpleBVHNode
{
public:
	inline SimpleBVHNode() {}
	inline ~SimpleBVHNode() {}

	inline static size_t requiredAlignment() { return 64; }

	//SSE_ALIGN AABBox left_aabb; // 32 bytes
	//SSE_ALIGN AABBox right_aabb; // 32 bytes
	float left_min[3];
	unsigned int left_child_index;    // left child index if interior, geom index if leaf.
	float left_max[3];
	unsigned int right_child_index;    // right child index if interior, num geom if leaf
	float right_min[3];
	unsigned int leaf;   // 1 if leaf node, 0 otherwise
	float right_max[3];
	unsigned int padding; 

	//int padding2[8];

	inline void setLeftAABB(const AABBox& x)
	{
		*(AABBox*)(&left_min) = x;
	}

	inline void setRightAABB(const AABBox& x)
	{
		*(AABBox*)(&right_min) = x;
	}

	
	/*inline unsigned int getLeftChildIndex() const
	{
		return left;
		//return *((unsigned int*)&left_aabb.min_.padding);
	}

	inline void setLeftChildIndex(unsigned int i)
	{
		left = i;
		//*((unsigned int*)&left_aabb.min_.padding) = i;
	}

	inline unsigned int getRightChildIndex() const
	{
		return right;
		//return *((unsigned int*)&left_aabb.max_.padding);
	}
	
	inline void setRightChildIndex(unsigned int i)
	{
		right = i;
		//*((unsigned int*)&left_aabb.max_.padding) = i;
	}*/


	inline void setLeaf(bool leaf_)
	{
		leaf = leaf_ ? 1 : 0;
		//*((unsigned int*)&right_aabb.min_.padding) = leaf ? 1 : 0;
	}

	inline unsigned int getLeaf() const
	{
		return leaf;
		//return *((unsigned int*)&right_aabb.min_.padding);
	}


	inline unsigned int getGeomIndex() const
	{
		return left_child_index;
		//return getLeftChildIndex();
	}

	inline void setGeomIndex(unsigned int x)
	{
		left_child_index = x;
		//setLeftChildIndex(x);
	}

	inline unsigned int getNumGeom() const
	{
		return right_child_index;
		//return getRightChildIndex();
	}

	inline void setNumGeom(unsigned int x)
	{
		right_child_index = x;
		//setRightChildIndex(x);
	}

	//int left_child_index;
	//int right_child_index;

	//int leaf;
	//int num_geom;
	//int geometry_index;

	//int padding[3];
};
#endif
//#if 0
class SimpleBVHNode
{
public:
	inline SimpleBVHNode() {}
	inline ~SimpleBVHNode() {}

	inline static size_t requiredAlignment() { return 64; }

	/*inline const float* leftMin() const { return &left_aabb.min_.x; }
	inline const float* leftMax() const { return &left_aabb.max_.x; }
	inline const float* rightMin() const { return &right_aabb.min_.x; }
	inline const float* rightMax() const { return &right_aabb.max_.x; }*/
	/*inline const float* leftMin() const { return left_min; }
	inline const float* leftMax() const { return left_max; }
	inline const float* rightMin() const { return right_min; }
	inline const float* rightMax() const { return right_max; }*/

	//inline void setLeftAABB(const AABBox& x) { left_aabb = x; }
	//inline void setRightAABB(const AABBox& x) {	right_aabb = x;	}
	//inline void setLeftAABB(const AABBox& x) { *(AABBox*)left_min = x; }
	//inline void setRightAABB(const AABBox& x) {	*(AABBox*)right_min = x; }
	inline void setLeftAABB(const AABBox& b)
	{
		box[0] = b.min_.x;
		box[1] = b.max_.x;
		box[4] = b.min_.y;
		box[5] = b.max_.y;
		box[8] = b.min_.z;
		box[9] = b.max_.z;
	}
	inline void setRightAABB(const AABBox& b)
	{
		box[2] = b.min_.x;
		box[3] = b.max_.x;
		box[6] = b.min_.y;
		box[7] = b.max_.y;
		box[10] = b.min_.z;
		box[11] = b.max_.z;
	}


	/*inline unsigned int getLeftChildIndex() const { return left_child_index; }
	inline void setLeftChildIndex(unsigned int i) { left_child_index = i; }

	inline unsigned int getRightChildIndex() const { return right_child_index; }
	inline void setRightChildIndex(unsigned int i) { right_child_index = i; }*/
	inline unsigned int getLeftChildIndex() const { return left; }
	inline void setLeftChildIndex(unsigned int i) { left = i; }

	inline unsigned int getRightChildIndex() const { return right; }
	inline void setRightChildIndex(unsigned int i) { right = i; }

	inline unsigned int getLeaf() const { return leaf; }
	inline void setLeaf(bool leaf_) { leaf = leaf_ ? 1 : 0; }

	inline unsigned int getGeomIndex() const { return getLeftChildIndex(); }
	inline void setGeomIndex(unsigned int x) { setLeftChildIndex(x); }

	inline unsigned int getNumGeom() const { return getRightChildIndex(); }
	inline void setNumGeom(unsigned int x) { setRightChildIndex(x); }

	float box[12]; // 48 bytes

private:

	unsigned int leaf;
	unsigned int left;
	unsigned int right;
	unsigned int dummy;



	//SSE_ALIGN AABBox left_aabb; // 32 bytes
	//SSE_ALIGN AABBox right_aabb; // 32 bytes

	/*union {
		float left_min[4];
		int data0[4];
	};
	//union {
		float left_max[4];
	//	int not_used;
	//};

	union {
		float right_min[4];
		unsigned int data2[4];
	};
	union {
		float right_max[4];
		unsigned int data3[4];
	};*/



	/*float left_min[3];
	unsigned int data0; 
	float left_max[3];
	int data1; 

	float right_min[3];
	unsigned int data2;
	float right_max[3];
	unsigned int data3;*/


	/*int padding[3];
	int left_child_index;
	int right_child_index;

	int leaf;
	int num_geom;
	int geometry_index;*/

};
//#endif




} //end namespace js


#endif //__SIMPLEBVHNODE_H_666_
