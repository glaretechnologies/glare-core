/*=====================================================================
ObjectTreeNode.h
----------------
File created by ClassTemplate on Tue Apr 01 21:34:28 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __OBJECTTREENODE_H_666_
#define __OBJECTTREENODE_H_666_

#include "../utils/Platform.h"

namespace js
{



/*=====================================================================
ObjectTreeNode
--------

=====================================================================*/
class ObjectTreeNode
{
public:
	inline ObjectTreeNode();
	inline ObjectTreeNode(uint32 axis, float split, uint32 right_child_node_index); // Interior node constructor
	inline ObjectTreeNode(uint32 leaf_grom_index, uint32 num_leaf_geom); // Leaf constructor
	inline ~ObjectTreeNode();
	
	static const uint32 NODE_TYPE_LEAF = 0;

	inline uint32 getNodeType() const;
	inline uint32 getSplittingAxis() const;
	inline uint32 getLeafGeomIndex() const;
	inline uint32 getPosChildIndex() const;
	inline uint32 getNumLeafGeom() const;

	union
	{
		float dividing_val; // Splitting plane coordinate
		uint32 numtris; // num triangles in leaf node
	} data2;

private:
	uint32 data;
};

#define OBJECT_TREE_NODE_MAIN_DATA_OFFSET 3
#define OBJECT_TREE_NODE_AXIS_OFFSET 1

ObjectTreeNode::ObjectTreeNode()
{}

ObjectTreeNode::ObjectTreeNode(uint32 axis, float split, uint32 right_child_node_index) // Interior node constructor
{
	data2.dividing_val = split;
	data = 0x00000001U | (axis << OBJECT_TREE_NODE_AXIS_OFFSET) | (right_child_node_index << OBJECT_TREE_NODE_MAIN_DATA_OFFSET);
}

ObjectTreeNode::ObjectTreeNode(uint32 leaf_grom_index, uint32 num_leaf_geom) // Leaf constructor
{
	data2.numtris = num_leaf_geom;
	data = leaf_grom_index << OBJECT_TREE_NODE_MAIN_DATA_OFFSET;
}

ObjectTreeNode::~ObjectTreeNode()
{}

uint32 ObjectTreeNode::getNodeType() const
{
	return data & 0x00000001;
}

uint32 ObjectTreeNode::getSplittingAxis() const
{ 
	return (data & 0x00000006U) >> OBJECT_TREE_NODE_AXIS_OFFSET;
}

uint32 ObjectTreeNode::getLeafGeomIndex() const
{ 
	return data >> OBJECT_TREE_NODE_MAIN_DATA_OFFSET;
}

uint32 ObjectTreeNode::getPosChildIndex() const
{ 
	return data >> OBJECT_TREE_NODE_MAIN_DATA_OFFSET;
}

uint32 ObjectTreeNode::getNumLeafGeom() const
{
	return data2.numtris;
}





} //end namespace js


#endif //__OBJECTTREENODE_H_666_




