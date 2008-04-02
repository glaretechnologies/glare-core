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

float dividing_val or uint32 numtris
int data

data layout:
bits 30 - 31:
	0 = leafnode
	1 = interior node with left child 
	2 = interior node with right child
	3 = interior node with two children
bits 28 - 29: 
	splitting axis
bits 0 - 27: 
	if leaf node: index into leaf tri index array
	if interior node with 1 child: nothing
	if interior node with 2 children: index of right child
=====================================================================*/
class TreeNode
{
public:
	inline TreeNode();
	inline TreeNode(uint32 axis, float split, uint32 right_child_node_index); // Interior node constructor
	inline TreeNode(uint32 leaf_grom_index, uint32 num_leaf_geom); // Leaf constructor
	inline ~TreeNode();
	
	static const uint32 NODE_TYPE_LEAF = 0;
	//static const uint32 NODE_TYPE_LEFT_CHILD_ONLY = 1;
	//static const uint32 NODE_TYPE_RIGHT_CHILD_ONLY = 2;
	//static const uint32 NODE_TYPE_TWO_CHILDREN = 3;

	inline uint32 getNodeType() const;
	//inline uint32 isLeafNode() const;
	inline uint32 getSplittingAxis() const;
	inline uint32 getLeafGeomIndex() const;
	inline uint32 getPosChildIndex() const;
	inline uint32 getNumLeafGeom() const;
	//inline uint32 isSingleChildNode()

	/*inline void setNodeType(uint32 t);
	//inline void setLeafNode(bool leafnode);
	inline void setSplittingAxis(uint32 axis);
	inline void setLeafGeomIndex(uint32 index);
	inline void setPosChildIndex(uint32 index);
	inline void setNumLeafGeom(uint32 num);*/

	union
	{
		float dividing_val;
		uint32 numtris;//num triangles in leaf node
	} data2;

private:
	uint32 data;
};

#define TREENODE_MAIN_DATA_OFFSET 3
#define TREENODE_AXIS_OFFSET 1

TreeNode::TreeNode()
{}

TreeNode::TreeNode(uint32 axis, float split, uint32 right_child_node_index) // Interior node constructor
{
	data2.dividing_val = split;
	data = 0x00000001U | (axis << TREENODE_AXIS_OFFSET) | (right_child_node_index << TREENODE_MAIN_DATA_OFFSET);
}

TreeNode::TreeNode(uint32 leaf_grom_index, uint32 num_leaf_geom) // Leaf constructor
{
	data2.numtris = num_leaf_geom;
	data = leaf_grom_index << TREENODE_MAIN_DATA_OFFSET;
}


TreeNode::~TreeNode()
{}

uint32 TreeNode::getNodeType() const
{
	return data & 0x00000001;
}
/*uint32 TreeNode::isLeafNode() const
{ 
	return data & 0x00000001;
}*/

uint32 TreeNode::getSplittingAxis() const
{ 
	return (data & 0x00000006U) >> TREENODE_AXIS_OFFSET;
}

uint32 TreeNode::getLeafGeomIndex() const
{ 
	return data >> TREENODE_MAIN_DATA_OFFSET;
}

uint32 TreeNode::getPosChildIndex() const
{ 
	return data >> TREENODE_MAIN_DATA_OFFSET;
}

uint32 TreeNode::getNumLeafGeom() const
{
	return data2.numtris;
}

/*
void TreeNode::setLeafNode(bool leafnode)
{
	if(leafnode)
		data |= 0x00000001U;
	else
		data &= 0xFFFFFFFEU;
}*/


/*void TreeNode::setNodeType(uint32 t)
{
	//data &= 0xFFFFFFF9U;//zero type bits
	//data |= 0x00000001U;
	data = (data & 0xFFFFFFFCU) | t;
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
	data |= index << TREENODE_MAIN_DATA_OFFSET;
}
	
void TreeNode::setPosChildIndex(uint32 index)
{
	assert(index < (1 << 29));

	data &= 0x00000007U;//zero target bits
	data |= index << TREENODE_MAIN_DATA_OFFSET;
}

void TreeNode::setNumLeafGeom(uint32 num)
{
	data2.numtris = num;
}*/



} //end namespace js


#endif //__TREENODE_H_666_




