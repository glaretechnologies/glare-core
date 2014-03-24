/*=====================================================================
kdtreenode.h
----------
File created by ClassTemplate on Fri Nov 05 01:54:56 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __KDTREENODE_H_666_
#define __KDTREENODE_H_666_


#include <assert.h>
#include "../utils/Platform.h"


namespace js
{


/*=====================================================================
KDTreeNode
--------
negative child is implicitly defined as the next node in the array.




Layout
------

Interior Node Layout
--------------------
float dividing_val

data layout:
bit 31:
	1 = interior node
bits 29 - 30: 
	splitting axis
bits 0 - 28: 
	index of right child node


Leaf Node Layout
----------------
uint32 numtris

data layout:
bit 31:
	0 = leafnode
bits 0 - 31:
	index into leaf tri index array




OLD:
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
class KDTreeNode
{
public:
	inline KDTreeNode();
	inline KDTreeNode(/*uint32 node_type, */uint32 axis, float split, uint32 right_child_node_index); // Interior node constructor
	inline KDTreeNode(uint32 leaf_geom_index, uint32 num_leaf_geom); // Leaf constructor

	static const uint32 NODE_TYPE_LEAF = 0;
	static const uint32 NODE_TYPE_INTERIOR = 1;
	//static const uint32 NODE_TYPE_LEFT_CHILD_ONLY = 1;
	//static const uint32 NODE_TYPE_RIGHT_CHILD_ONLY = 2;
	//static const uint32 NODE_SINGLE_CHILD = 2;
	//static const uint32 NODE_TYPE_TWO_CHILDREN = 3;

	inline uint32 getNodeType() const;


	inline uint32 getSplittingAxis() const;
	inline uint32 getPosChildIndex() const;

	inline uint32 getLeafGeomIndex() const;
	inline uint32 getNumLeafGeom() const;


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
#define TREENODE_LEAF_DATA_OFFSET 1

KDTreeNode::KDTreeNode()
{}

KDTreeNode::KDTreeNode(/*uint32 node_type, */uint32 axis, float split, uint32 right_child_node_index) // Interior node constructor
{
	assert(axis < 3);

	data2.dividing_val = split;
	//data = 0x00000001U | (axis << TREENODE_AXIS_OFFSET) | (right_child_node_index << TREENODE_MAIN_DATA_OFFSET);

	//assert(node_type == NODE_TYPE_LEFT_CHILD_ONLY || node_type == NODE_TYPE_RIGHT_CHILD_ONLY || node_type == NODE_TYPE_TWO_CHILDREN);
	//assert(node_type == NODE_TYPE_INTERIOR);

	data = NODE_TYPE_INTERIOR | (axis << TREENODE_AXIS_OFFSET) | (right_child_node_index << TREENODE_MAIN_DATA_OFFSET);
	//data = node_type | (axis << TREENODE_AXIS_OFFSET) | (right_child_node_index << TREENODE_MAIN_DATA_OFFSET);
}

KDTreeNode::KDTreeNode(uint32 leaf_geom_index, uint32 num_leaf_geom) // Leaf constructor
{
	data2.numtris = num_leaf_geom;
	assert(NODE_TYPE_LEAF == 0);
	data = leaf_geom_index << TREENODE_LEAF_DATA_OFFSET;
}


uint32 KDTreeNode::getNodeType() const
{
	return data & 0x00000001;
}



uint32 KDTreeNode::getSplittingAxis() const
{ 
	//return (data & 0x00000006U) >> TREENODE_AXIS_OFFSET;
	return (data >> TREENODE_AXIS_OFFSET) & 0x00000003; 
}

uint32 KDTreeNode::getPosChildIndex() const
{ 
	return data >> TREENODE_MAIN_DATA_OFFSET;
}




uint32 KDTreeNode::getLeafGeomIndex() const
{ 
	return data >> TREENODE_LEAF_DATA_OFFSET;
}

uint32 KDTreeNode::getNumLeafGeom() const
{
	return data2.numtris;
}




} //end namespace js


#endif //__KDTREENODE_H_666_




