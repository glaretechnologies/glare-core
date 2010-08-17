#include "MultiLevelGridNode.h"

MultiLevelGridNode::MultiLevelGridNode(void)
{
	for(int i=0;i<sizeof(cells) / sizeof(int); ++i)
	{
		cells[i] = 0.f;
	}
}

MultiLevelGridNode::~MultiLevelGridNode(void)
{
}
