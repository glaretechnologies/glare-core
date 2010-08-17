#pragma once


const int RES = 2;


class MultiLevelGridNode
{
public:
	MultiLevelGridNode();
	~MultiLevelGridNode();

	/*
		0 = interior
		1 = empty leaf
		2 = leaf containing >= 1 object.
	*/
	inline bool cellIsInterior(int i, int j, int k) const;
	//bool cellIsEmpty(int i, int j, int k);
	//bool cellIsNonEmptyLeaf(int i, int j, int k);

	//unsigned int cellIsInteriorType() const { return 0; }
	//unsigned int type;
	//unsigned int child_index;

	inline int cellChildIndex(int i, int j, int k) const;

	inline void setCellChildIndex(int i, int j, int k, int index)
	{
		cells[i*16 + j*4 + k] = index;
	}

	int cells[RES * RES * RES];
};


bool MultiLevelGridNode::cellIsInterior(int i, int j, int k) const
{
	return cells[i*16 + j*4 + k] >= 1;
}


int MultiLevelGridNode::cellChildIndex(int i, int j, int k) const
{
	return cells[i*16 + j*4 + k];
}
