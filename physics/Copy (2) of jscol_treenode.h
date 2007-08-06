/*=====================================================================
treenode.h
----------
File created by ClassTemplate on Fri Nov 05 01:54:56 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TREENODE_H_666_
#define __TREENODE_H_666_



#include "jscol_triangle.h"
#include "jscol_edgetri.h"
#include <vector>
class Ray;

namespace js
{

class AABBox;



/*=====================================================================
TreeNode
--------

=====================================================================*/
class TreeNode
{
public:
	/*=====================================================================
	TreeNode
	--------
	
	=====================================================================*/
	TreeNode();

	~TreeNode();

	void draw() const;

	//debugging/diagnosis stuff
	int getNumLeafTrisInSubtree() const;

//	static int nodes_touched;

	inline bool isLeafNode() const { return num_leaf_tris != -1; }
	/*inline bool isLeafNode() const { return data & 0x000001; }
	inline int getSplittingAxis() const { return (data & 0x000006) >> 1; }
	inline unsigned int getLeafGeomIndex() const { return data >> 3; }

	inline void setLeafNode(bool leafnode);
	inline void setSplittingAxis(int axis);
	inline void setLeafGeomIndex(unsigned int index);*/
	

	//negative child is implicitly defined as the next node in the array.

	//unsigned int data;
	int positive_child;//OR index into leaf geometry, 4 bytes

	int num_leaf_tris;//==-1 if this is an interior node.

	int dividing_axis;//4
	float dividing_val;//4

private:
};

/*void TreeNode::setLeafNode(bool leafnode)
{
	if(leafnode)
		data |= 0x000001f;
	else
		data &= 0xFFFFF7f;
}

void TreeNode::setSplittingAxis(int axis)
{
	data |= (axis << 1); 
}

void TreeNode::setLeafGeomIndex(unsigned int index)
{
	assert(index < (1 << 29));
	data |= index << 3;
}*/
	

/*

8 Byte layout:
float dividing_val

31st (rightmost) bit of 'data': if leafnode or not.
bits 29+30: splitting axis
bits 0 - 28: index into leaf geom 



*/





} //end namespace js


#endif //__TREENODE_H_666_




