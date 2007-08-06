/*=====================================================================
treenode.h
----------
File created by ClassTemplate on Fri Nov 05 01:54:56 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TREENODE_H_666_
#define __TREENODE_H_666_

#include <assert.h>

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

	inline int isLeafNode() const;
	inline unsigned int getSplittingAxis() const;
	inline unsigned int getLeafGeomIndex() const;
	inline unsigned int getPosChildIndex() const;
	inline unsigned int getNumLeafGeom() const;

	inline void setLeafNode(bool leafnode);
	inline void setSplittingAxis(unsigned int axis);
	inline void setLeafGeomIndex(unsigned int index);
	inline void setPosChildIndex(unsigned int index);
	inline void setNumLeafGeom(unsigned int num);

	union
	{
		float dividing_val;
		unsigned int numtris;//num triangles in leaf node
	} data2;

private:
	unsigned int data;
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

int TreeNode::isLeafNode() const
{ 
	return data & 0x00000001;
}

unsigned int TreeNode::getSplittingAxis() const
{ 
	return (data & 0x00000006U) >> 1;
}

unsigned int TreeNode::getLeafGeomIndex() const
{ 
	return data >> 3;
}

unsigned int TreeNode::getPosChildIndex() const
{ 
	return data >> 3;
}

unsigned int TreeNode::getNumLeafGeom() const
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

void TreeNode::setSplittingAxis(unsigned int axis)
{
	data &= 0xFFFFFFF9U;//zero axis bits
	data |= (axis << 1); 
}

void TreeNode::setLeafGeomIndex(unsigned int index)
{
	assert(index < (1 << 29));

	data &= 0x00000007U;//zero target bits
	data |= index << 3;
}
	
void TreeNode::setPosChildIndex(unsigned int index)
{
	assert(index < (1 << 29));

	data &= 0x00000007U;//zero target bits
	data |= index << 3;
}

void TreeNode::setNumLeafGeom(unsigned int num)
{
	data2.numtris = num;
}



} //end namespace js


#endif //__TREENODE_H_666_




