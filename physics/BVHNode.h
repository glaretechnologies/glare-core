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

	inline BVHNode() {}

	//inline static size_t requiredAlignment() { return 64; }
	static const int REQUIRED_ALIGNMENT = 64;

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
	unsigned int left; // left child index, or left geometry index
	unsigned int right; // right child index, or right geometry index
	unsigned int num_geom;
};


} //end namespace js


#endif //__BVHNODE_H_666_
