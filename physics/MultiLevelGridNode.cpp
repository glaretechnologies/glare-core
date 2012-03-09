#include "MultiLevelGridNode.h"


MultiLevelGridNode::MultiLevelGridNode()
{
	for(int i=0; i<MLG_RES * MLG_RES * MLG_RES; ++i)
	{
		cells[i] = 0;
	}
}


MultiLevelGridNode::~MultiLevelGridNode()
{
}
