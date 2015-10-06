/*=====================================================================
BVHNode.h
---------
Copyright Glare Technologies Limited 2015 -
File created by ClassTemplate on Sun Oct 26 17:19:33 2008
=====================================================================*/
#pragma once


#include "jscol_aabbox.h"


namespace js
{


/*=====================================================================
BVHNode
-------
Used by BVH class.
64 bytes.
=====================================================================*/
class BVHNode
{
public:
	inline BVHNode() {}

	inline static unsigned int maxNumGeom() { return 0xFFFF; }

	inline void setLeftAABB(const AABBox& b)
	{
		box[0] = b.min_.x[0];
		box[1] = b.max_.x[0];
		box[4] = b.min_.x[1];
		box[5] = b.max_.x[1];
		box[8] = b.min_.x[2];
		box[9] = b.max_.x[2];

		// [l_min_x, r_min_x, l_max_x, r_max_x, l_min_y, r_min_y, l_max_y, r_max_y, l_min_z, r_min_z, l_max_z, r_max_z]

		/*box[0] = b.min_.x;
		box[2] = b.max_.x;
		box[4] = b.min_.y;
		box[6] = b.max_.y;
		box[8] = b.min_.z;
		box[10] = b.max_.z;*/

	}
	inline void setRightAABB(const AABBox& b)
	{
		box[2] = b.min_.x[0];
		box[3] = b.max_.x[0];
		box[6] = b.min_.x[1];
		box[7] = b.max_.x[1];
		box[10] = b.min_.x[2];
		box[11] = b.max_.x[2];

		/*box[1] = b.min_.x;
		box[3] = b.max_.x;
		box[5] = b.min_.y;
		box[7] = b.max_.y;
		box[9] = b.min_.z;
		box[11] = b.max_.z;*/
	}

	inline unsigned int getLeftChildIndex() const { return left; }
	inline void setLeftChildIndex(unsigned int i) { left = i; }
	inline unsigned int getRightChildIndex() const { return right; }
	inline void setRightChildIndex(unsigned int i) { right = i; }

	inline void setToInterior() { leaf = 0; }
	inline unsigned int isLeftLeaf() const { return leaf & 0x00000001; }
	inline void setLeftToLeaf() { leaf |= 0x00000001; }
	inline unsigned int isRightLeaf() const { return leaf & 0x00000002; }
	inline void setRightToLeaf() { leaf |= 0x00000002; }

	inline unsigned int getLeftGeomIndex() const { return left; }
	inline void setLeftGeomIndex(unsigned int x) { left = x; }
	inline unsigned int getRightGeomIndex() const { return right; }
	inline void setRightGeomIndex(unsigned int x) { right = x; }

	inline unsigned int getLeftNumGeom() const { return num_geom >> 16; }
	inline void setLeftNumGeom(unsigned int x) { num_geom = (x << 16) | (num_geom & 0x0000FFFF); }
	inline unsigned int getRightNumGeom() const { return num_geom & 0x0000FFFF; }
	inline void setRightNumGeom(unsigned int x) { num_geom = (num_geom & 0xFFFF0000) | x; }

	float box[12]; // 48 bytes
private:
	unsigned int leaf; // bit 0: left is leaf, bit 1: right is leaf.
	unsigned int left; // left child node index, or left geometry index
	unsigned int right; // right child node index, or right geometry index
	unsigned int num_geom; // left 16 bits are the number of triangles for left child/AABB, right 16 bits are number of tris for right child/AABB.
};


} //end namespace js
