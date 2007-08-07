/*=====================================================================
treenode.h
----------
File created by ClassTemplate on Fri Nov 05 01:54:56 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TREENODE_H_666_
#define __TREENODE_H_666_

#include <assert.h>
#include "../utils/platform.h"

namespace js
{

/*=====================================================================
TreeNode
--------
negative child is implicitly defined as the next node in the array.
8 Byte layout:

float dividing_val / numtris
int data

data layout:
31st (rightmost) bit of 'data': if leafnode or not.
bits 29+30: splitting axis
bits 0 - 28: index into leaf geom OR positive child node index.
=====================================================================*/
class TreeNode
{
public:
	inline TreeNode();
	inline ~TreeNode();

	inline uint32 isLeafNode() const;
	inline uint32 getSplittingAxis() const;
	inline uint32 getLeafGeomIndex() const;
	inline uint32 getPosChildIndex() const;
	inline uint32 getNumLeafGeom() const;

	inline void setLeafNode(bool leafnode);
	inline void setSplittingAxis(uint32 axis);
	inline void setLeafGeomIndex(uint32 index);
	inline void setPosChildIndex(uint32 index);
	inline void setNumLeafGeom(uint32 num);

	union
	{
		float dividing_val;
		uint32 numtris;//num triangles in leaf node
	} data2;

private:
	uint32 data;
};

TreeNode::TreeNode()
{
	data = 0;
	data2.dividing_val = 0.0f;

	this->setLeafGeomIndex(0);
	this->setLeafNode(false);
	this->setSplittingAxis(0);
}

TreeNode::~TreeNode()
{}

uint32 TreeNode::isLeafNode() const
{ 
	return data & 0x00000001;
}

uint32 TreeNode::getSplittingAxis() const
{ 
	return (data & 0x00000006U) >> 1;
}

uint32 TreeNode::getLeafGeomIndex() const
{ 
	return data >> 3;
}

uint32 TreeNode::getPosChildIndex() const
{ 
	return data >> 3;
}

uint32 TreeNode::getNumLeafGeom() const
{
	return data2.numtris;
}


void TreeNode::setLeafNode(bool leafnode)
{
	if(leafnode)
		data |= 0x00000001U;
	else
		data &= 0xFFFFFFFEU;
}

void TreeNode::setSplittingAxis(uint32 axis)
{
	data &= 0xFFFFFFF9U;//zero axis bits
	data |= (axis << 1); 
}

void TreeNode::setLeafGeomIndex(uint32 index)
{
	assert(index < (1 << 29));

	data &= 0x00000007U;//zero target bits
	data |= index << 3;
}
	
void TreeNode::setPosChildIndex(uint32 index)
{
	assert(index < (1 << 29));

	data &= 0x00000007U;//zero target bits
	data |= index << 3;
}

void TreeNode::setNumLeafGeom(uint32 num)
{
	data2.numtris = num;
}



} //end namespace js


#endif //__TREENODE_H_666_




