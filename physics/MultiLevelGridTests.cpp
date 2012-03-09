#include "MultiLevelGridTests.h"


#include "MultiLevelGrid.h"


namespace MultiLevelGridTests
{


void test()
{
	Vec4f min(0,0,0,1);
	Vec4f max(1,1,1,1);
	MultiLevelGrid grid(min, max);
	
	grid.nodes.push_back(MultiLevelGridNode());
	//grid.nodes.back().setCellChildIndex(1,0,0, 1);


}


}

