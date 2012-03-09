#include "MultiLevelGridTests.h"


#if BUILD_TESTS


#include "MultiLevelGrid.h"
#include "../indigo/TestUtils.h"


namespace MultiLevelGridTests
{


class NodeCell
{
public:
	NodeCell();
	NodeCell(uint32 n, const Vec3i& c) : node(n), cell(c) {}

	bool operator == (const NodeCell& n) const { return node == n.node && cell == n.cell; }

	uint32 node;
	Vec3i cell;
};


class RecordCellResults
{
public:
	std::vector<NodeCell> cells;
};


class RecordCellVisitor
{
public:
	void visit(const Ray& ray, uint32 node, const Vec3i& cell, float& max_t_in_out, RecordCellResults& result_out)
	{
		result_out.cells.push_back(NodeCell(node, cell));
	}
};


void test()
{
	// Test basic traversal through single node, along ray (1,0,0)
	{
		Vec4f min(0,0,0,1);
		Vec4f max(1,1,1,1);
		MultiLevelGrid<RecordCellVisitor, RecordCellResults> grid(min, max, RecordCellVisitor());
	
		grid.nodes.push_back(MultiLevelGridNode()); // Root node

		Ray ray(
			Vec4f(0, 1.f/8.f, 1.f/8.f, 1),
			Vec4f(1, 0, 0, 0)
		);
		float max_t = 1;

		RecordCellResults results;
		grid.trace(ray, max_t, results);
		
		testAssert(results.cells.size() == 4);
		testAssert(results.cells[0] == NodeCell(0, Vec3i(0,0,0)));
		testAssert(results.cells[1] == NodeCell(0, Vec3i(1,0,0)));
		testAssert(results.cells[2] == NodeCell(0, Vec3i(2,0,0)));
		testAssert(results.cells[3] == NodeCell(0, Vec3i(3,0,0)));
	}

	// Test basic traversal through single node, along ray pointing down a bit.
	{
		Vec4f min(0,0,0,1);
		Vec4f max(1,1,1,1);
		MultiLevelGrid<RecordCellVisitor, RecordCellResults> grid(min, max, RecordCellVisitor());
	
		grid.nodes.push_back(MultiLevelGridNode()); // Root node

		Ray ray(
			Vec4f(0, 0, 1.f/8.f, 1),
			normalise(Vec4f(1, 3.f/8.f, 0, 0))
		);
		float max_t = 1;

		RecordCellResults results;
		grid.trace(ray, max_t, results);
		
		testAssert(results.cells.size() == 5);
		testAssert(results.cells[0] == NodeCell(0, Vec3i(0,0,0)));
		testAssert(results.cells[1] == NodeCell(0, Vec3i(1,0,0)));
		testAssert(results.cells[2] == NodeCell(0, Vec3i(2,0,0)));
		testAssert(results.cells[3] == NodeCell(0, Vec3i(2,1,0)));
		testAssert(results.cells[4] == NodeCell(0, Vec3i(3,1,0)));
	}

	// Test basic traversal through single node with offset, along ray (1,0,0) with origin in the middle of the node
	{
		Vec4f min(1,1,1,1);
		Vec4f max(2,2,2,1);
		MultiLevelGrid<RecordCellVisitor, RecordCellResults> grid(min, max, RecordCellVisitor());
	
		grid.nodes.push_back(MultiLevelGridNode()); // Root node

		Ray ray(
			Vec4f(11.f/8.f, 11.f/8.f, 9.f/8.f, 1),
			Vec4f(1, 0, 0, 0)
		);
		float max_t = 1;

		RecordCellResults results;
		grid.trace(ray, max_t, results);
		
		testAssert(results.cells.size() == 3);
		testAssert(results.cells[0] == NodeCell(0, Vec3i(1,1,0)));
		testAssert(results.cells[1] == NodeCell(0, Vec3i(2,1,0)));
		testAssert(results.cells[2] == NodeCell(0, Vec3i(3,1,0)));
	}


	// Test basic traversal through a node with a single child
	{
		Vec4f min(1,1,1,1);
		Vec4f max(2,2,2,1);
		MultiLevelGrid<RecordCellVisitor, RecordCellResults> grid(min, max, RecordCellVisitor());
	
		grid.nodes.push_back(MultiLevelGridNode()); // Root node
		grid.nodes.back().setCellChildIndex(1, 0, 0,    1);
		grid.nodes.push_back(MultiLevelGridNode()); // Child node

		Ray ray(
			Vec4f(1, 1 + 0.15625f, 1 + 0.0001f, 1),
			Vec4f(1, 0, 0, 0)
		);
		float max_t = 1;

		RecordCellResults results;
		grid.trace(ray, max_t, results);
		
		testAssert(results.cells.size() == 7);
		testAssert(results.cells[0] == NodeCell(0, Vec3i(0,0,0)));

		testAssert(results.cells[1] == NodeCell(1, Vec3i(0,2,0)));
		testAssert(results.cells[2] == NodeCell(1, Vec3i(1,2,0)));
		testAssert(results.cells[3] == NodeCell(1, Vec3i(2,2,0)));
		testAssert(results.cells[4] == NodeCell(1, Vec3i(3,2,0)));

		testAssert(results.cells[5] == NodeCell(0, Vec3i(2,0,0)));
		testAssert(results.cells[6] == NodeCell(0, Vec3i(3,0,0)));
	}

	// Test basic traversal through a node with a single child, with a single child.
	{
		Vec4f min(0,0,0,1);
		Vec4f max(1,1,1,1);
		MultiLevelGrid<RecordCellVisitor, RecordCellResults> grid(min, max, RecordCellVisitor());
	
		grid.nodes.push_back(MultiLevelGridNode()); // Root node
		grid.nodes.back().setCellChildIndex(1, 0, 0,    1);
		grid.nodes.push_back(MultiLevelGridNode()); // Child node
		grid.nodes.back().setCellChildIndex(0, 2, 0,    2);
		grid.nodes.push_back(MultiLevelGridNode()); // Child node (depth 2)

		Ray ray(
			Vec4f(0, 9.5f / 64, 0.0001f, 1),
			Vec4f(1, 0, 0, 0)
		);
		float max_t = 1;

		RecordCellResults results;
		grid.trace(ray, max_t, results);
		
		testAssert(results.cells.size() == 10);
		testAssert(results.cells[0] == NodeCell(0, Vec3i(0,0,0)));

		testAssert(results.cells[1] == NodeCell(2, Vec3i(0,1,0)));
		testAssert(results.cells[2] == NodeCell(2, Vec3i(1,1,0)));
		testAssert(results.cells[3] == NodeCell(2, Vec3i(2,1,0)));
		testAssert(results.cells[4] == NodeCell(2, Vec3i(3,1,0)));

		testAssert(results.cells[5] == NodeCell(1, Vec3i(1,2,0)));
		testAssert(results.cells[6] == NodeCell(1, Vec3i(2,2,0)));
		testAssert(results.cells[7] == NodeCell(1, Vec3i(3,2,0)));

		testAssert(results.cells[8] == NodeCell(0, Vec3i(2,0,0)));
		testAssert(results.cells[9] == NodeCell(0, Vec3i(3,0,0)));
	}


}


}


#endif
