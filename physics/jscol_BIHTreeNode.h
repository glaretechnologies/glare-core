/*=====================================================================
BIHTreeNode.h
-------------
File created by ClassTemplate on Sun Nov 26 22:00:26 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __BIHTREENODE_H_666_
#define __BIHTREENODE_H_666_





namespace js
{




/*=====================================================================
BIHTreeNode
-----------

=====================================================================*/
class BIHTreeNode
{
public:
	/*=====================================================================
	BIHTreeNode
	-----------
	
	=====================================================================*/
	inline BIHTreeNode()
	{
		data = 0;
	}

	inline ~BIHTreeNode(){}


	inline bool isLeafNode() const
	{
		return data & 0x00000001;
	}
	inline void setIsLeafNode(bool isleaf)
	{
		if(isleaf)
			data |= 0x00000001U;
		else
			data &= 0xFFFFFFFEU;
	}

	inline unsigned int getSplittingAxis() const
	{ 
		return data >> 1;
	}

	inline void setSplittingAxis(unsigned int axis)
	{
		data &= 0xFFFFFFF9U;//zero axis bits
		data |= (axis << 1); 
	}
	inline unsigned int childIndex() const { return index; }
	inline void setChildIndex(unsigned int index_){ index = index_; }
	inline unsigned int getLeafGeomIndex() const { return index; }
	inline unsigned int getNumLeafGeom() const { return num_intersect_items; }


	unsigned int index;//index of left child if internal node, or index into leafgeom array if leaf node
	
	unsigned int data;
	//31st (rightmost) bit of 'data': if leafnode or not.
	//bits 29+30: splitting axis

	union
	{
		unsigned int num_intersect_items; //leaf only: number of triangle items
		float clip[2];
	};

};



} //end namespace js


#endif //__BIHTREENODE_H_666_




