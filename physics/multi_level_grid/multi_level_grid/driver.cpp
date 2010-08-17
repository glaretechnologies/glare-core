

#include "MultiLevelGrid.h"
#include <math.h>


int main(int argc, char** argv)
{

	MultiLevelGrid g;
	g.nodes.reserve(10);

	g.grid_orig = Vec3(0,0,0);
	g.cell_width = 1.f;
	// Node 0
	g.nodes.push_back(MultiLevelGridNode());
	g.nodes.back().setCellChildIndex(0, 1, 0, 1);
	// Node 1
	g.nodes.push_back(MultiLevelGridNode());
	g.nodes.back().setCellChildIndex(1, 0, 0, 2);
	g.nodes.back().setCellChildIndex(1, 0, 1, 2);
	// Node 2
	g.nodes.push_back(MultiLevelGridNode());

	g.trace(
		Vec3(0.625f, 1.125f, 0.001f), // orig
		Vec3(2.f / sqrt(5.f), 1.f / sqrt(5.f), 0.000001f) // dir
	);
}
