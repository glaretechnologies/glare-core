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
	inline SimpleBVHNode() {}
	inline ~SimpleBVHNode() {}

	inline static size_t requiredAlignment() { return 64; }

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
};


} //end namespace js


#endif //__SIMPLEBVHNODE_H_666_
