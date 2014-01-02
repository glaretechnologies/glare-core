#include "MultiLevelGridTests.h"


#if BUILD_TESTS


#include "MultiLevelGrid.h"
#include "../indigo/TestUtils.h"
#include "../utils/MTwister.h"
#include "../maths/GeometrySampling.h"


namespace MultiLevelGridTests
{


class NodeCell
{
public:
	NodeCell();
	NodeCell(uint32 n, const Vec4i& c, float t_enter_, float t_exit_) : node(n), cell(c), t_enter(t_enter_), t_exit(t_exit_) {}

	bool operator == (const NodeCell& n) const { return node == n.node && cell == n.cell && (t_enter == -1.0f || n.t_enter == -1.0f || (epsEqual(t_enter, n.t_enter) && epsEqual(t_exit, n.t_exit))); }

	uint32 node;
	Vec4i cell;
	float t_enter;
	float t_exit;
};


class RecordCellResults
{
public:
	std::vector<NodeCell> cells;
};


class RecordCellData
{
public:
};


class RecordCellVisitor
{
public:
	void visit(const Ray& ray, uint32 node, const Vec4i& cell, const RecordCellData& data, float t_enter, float t_exit, float& max_t_in_out, RecordCellResults& result_out)
	{
		result_out.cells.push_back(NodeCell(node, cell, t_enter, t_exit));
	}
};



static const Vec4f randomDir(MTwister& rng)
{
	return Vec4f(
		-1 + 2*rng.unitRandom(),
		-1 + 2*rng.unitRandom(),
		-1 + 2*rng.unitRandom(),
		0);
}


void test()
{
	conPrint("MultiLevelGridTests::test()");

	testAssert(mlgPopCount(0) == 0);
	testAssert(mlgPopCount(0xFFFFull) == 16);
	testAssert(mlgPopCount(0xFFFFFFFFull) == 32);
	testAssert(mlgPopCount(0xFFFFFFFFFFFFFFFFull) == 64);


	testAssert(mlgPopCount(0) == 0);
	testAssert(mlgPopCount(0xFFFFull) == 16);
	testAssert(mlgPopCount(0xFFFFFFFFull) == 32);
	testAssert(mlgPopCount(0xFFFFFFFFFFFFFFFFull) == 64);
	


	// Test nextStepAxis
	testAssert(nextStepAxis(Vec4f(1,2,3,0)) == 0);
	testAssert(nextStepAxis(Vec4f(1,3,2,0)) == 0);
	testAssert(nextStepAxis(Vec4f(2,1,3,0)) == 1);
	testAssert(nextStepAxis(Vec4f(3,1,2,0)) == 1);
	testAssert(nextStepAxis(Vec4f(2,3,1,0)) == 2);
	testAssert(nextStepAxis(Vec4f(3,2,1,0)) == 2);

	// Test MultiLevelGridNode child node stuff
	{
		MultiLevelGridNode<RecordCellData> n;
		n.setBaseChildIndex(100);
		n.markCellAsInterior(Vec4i(0, 0, 0, 0));
		n.markCellAsInterior(Vec4i(1, 0, 0, 0));
		n.markCellAsInterior(Vec4i(2, 0, 0, 0));

		testAssert(n.cellIsInterior(Vec4i(0, 0, 0, 0)));
		testAssert(n.cellIsInterior(Vec4i(1, 0, 0, 0)));
		testAssert(n.cellIsInterior(Vec4i(2, 0, 0, 0)));

		testAssert(!n.cellIsInterior(Vec4i(3, 0, 0, 0)));

		testAssert(n.cellChildIndex(Vec4i(0,0,0, 0)) == 100);
		testAssert(n.cellChildIndex(Vec4i(1,0,0, 0)) == 101);
		testAssert(n.cellChildIndex(Vec4i(2,0,0, 0)) == 102);
	}

	// Test MultiLevelGridNode node with all interior cells
	{
		MultiLevelGridNode<RecordCellData> n;
		n.setBaseChildIndex(100);
		for(int z=0; z<MLG_RES; ++z)
		for(int y=0; y<MLG_RES; ++y)
		for(int x=0; x<MLG_RES; ++x)
		{
			n.markCellAsInterior(Vec4i(x, y, z, 0));
		}

		for(int z=0; z<MLG_RES; ++z)
		for(int y=0; y<MLG_RES; ++y)
		for(int x=0; x<MLG_RES; ++x)
		{
			testAssert(n.cellIsInterior(Vec4i(x, y, z, 0)));
			testAssert(n.cellChildIndex(Vec4i(x, y, z, 0)) == 100 + (z*16 + y*4 + x));
		}
	}


	// Test basic traversal through single node, along ray (1,0,0)
	{
		Vec4f min(0,0,0,1);
		Vec4f max(1,1,1,1);
		MultiLevelGrid<RecordCellVisitor, RecordCellResults, RecordCellData> grid(min, max, RecordCellVisitor());
	
		grid.nodes.push_back(MultiLevelGridNode<RecordCellData>()); // Root node

		Ray ray(
			Vec4f(0, 1.f/8.f, 1.f/8.f, 1),
			Vec4f(1, 0, 0, 0),
			0 // min_t
		);
		float max_t = 1;

		RecordCellResults results;
		grid.trace(ray, max_t, results);
		
		testAssert(results.cells.size() == 4);
		testAssert(results.cells[0] == NodeCell(0, Vec4i(0,0,0, 0), 0, 0.25f));
		testAssert(results.cells[1] == NodeCell(0, Vec4i(1,0,0, 0), 0.25f, 0.5f));
		testAssert(results.cells[2] == NodeCell(0, Vec4i(2,0,0, 0), 0.5f, 0.75f));
		testAssert(results.cells[3] == NodeCell(0, Vec4i(3,0,0, 0), 0.75f, 1.0f));
	}

	// Test basic traversal through single node, along ray pointing down a bit.
	{
		Vec4f min(0,0,0,1);
		Vec4f max(1,1,1,1);
		MultiLevelGrid<RecordCellVisitor, RecordCellResults, RecordCellData> grid(min, max, RecordCellVisitor());
	
		grid.nodes.push_back(MultiLevelGridNode<RecordCellData>()); // Root node

		Ray ray(
			Vec4f(0, 0, 1.f/8.f, 1),
			normalise(Vec4f(1, 3.f/8.f, 0, 0)),
			0 // min_t
		);
		float max_t = 1;

		RecordCellResults results;
		grid.trace(ray, max_t, results);
		
		testAssert(results.cells.size() == 5);
		testAssert(results.cells[0] == NodeCell(0, Vec4i(0,0,0, 0), -1.f, -1.f));
		testAssert(results.cells[1] == NodeCell(0, Vec4i(1,0,0, 0), -1.f, -1.f));
		testAssert(results.cells[2] == NodeCell(0, Vec4i(2,0,0, 0), -1.f, -1.f));
		testAssert(results.cells[3] == NodeCell(0, Vec4i(2,1,0, 0), -1.f, -1.f));
		testAssert(results.cells[4] == NodeCell(0, Vec4i(3,1,0, 0), -1.f, -1.f));
	}

	// Test basic traversal through single node with offset, along ray (1,0,0) with origin in the middle of the node
	{
		Vec4f min(1,1,1,1);
		Vec4f max(2,2,2,1);
		MultiLevelGrid<RecordCellVisitor, RecordCellResults, RecordCellData> grid(min, max, RecordCellVisitor());
	
		grid.nodes.push_back(MultiLevelGridNode<RecordCellData>()); // Root node

		Ray ray(
			Vec4f(11.f/8.f, 11.f/8.f, 9.f/8.f, 1),
			Vec4f(1, 0, 0, 0),
			0 // min_t
		);
		float max_t = 1;

		RecordCellResults results;
		grid.trace(ray, max_t, results);
		
		testAssert(results.cells.size() == 3);
		testAssert(results.cells[0] == NodeCell(0, Vec4i(1,1,0, 0), 0.f, 1.f/8));
		testAssert(results.cells[1] == NodeCell(0, Vec4i(2,1,0, 0), 1.f/8, 3.f/8));
		testAssert(results.cells[2] == NodeCell(0, Vec4i(3,1,0, 0), 3.f/8, 5.f/8));
	}


	// Test basic traversal through a node with a single child
	{
		Vec4f min(1,1,1,1);
		Vec4f max(2,2,2,1);
		MultiLevelGrid<RecordCellVisitor, RecordCellResults, RecordCellData> grid(min, max, RecordCellVisitor());
	
		grid.nodes.push_back(MultiLevelGridNode<RecordCellData>()); // Add Root node

		grid.nodes.back().setBaseChildIndex(1);
		grid.nodes.back().markCellAsInterior(Vec4i(1,0,0, 0));
		//grid.nodes.back().setCellChildIndex(1, 0, 0,    1);

		grid.nodes.push_back(MultiLevelGridNode<RecordCellData>()); // Add Child node

		Ray ray(
			Vec4f(1, 1 + 0.15625f, 1 + 0.0001f, 1),
			Vec4f(1, 0, 0, 0),
			0 // min_t
		);
		float max_t = 1;

		RecordCellResults results;
		grid.trace(ray, max_t, results);
		
		testAssert(results.cells.size() == 7);
		testAssert(results.cells[0] == NodeCell(0, Vec4i(0,0,0, 0), 0.f, 0.25f));

		testAssert(results.cells[1] == NodeCell(1, Vec4i(0,2,0, 0), 4.f/16, 5.f/16));
		testAssert(results.cells[2] == NodeCell(1, Vec4i(1,2,0, 0), 5.f/16, 6.f/16));
		testAssert(results.cells[3] == NodeCell(1, Vec4i(2,2,0, 0), 6.f/16, 7.f/16));
		testAssert(results.cells[4] == NodeCell(1, Vec4i(3,2,0, 0), 7.f/16, 8.f/16));

		testAssert(results.cells[5] == NodeCell(0, Vec4i(2,0,0, 0), 8.f/16, 12.f/16));
		testAssert(results.cells[6] == NodeCell(0, Vec4i(3,0,0, 0), 12.f/16, 16.f/16));
	}

	// Test basic traversal through a node with a single child, with a single child.
	{
		Vec4f min(0,0,0,1);
		Vec4f max(1,1,1,1);
		MultiLevelGrid<RecordCellVisitor, RecordCellResults, RecordCellData> grid(min, max, RecordCellVisitor());
	
		grid.nodes.push_back(MultiLevelGridNode<RecordCellData>()); // Root node
		
		grid.nodes.back().setBaseChildIndex(1);
		grid.nodes.back().markCellAsInterior(Vec4i(1,0,0, 0));
		//grid.nodes.back().setCellChildIndex(1, 0, 0,    1);
		grid.nodes.push_back(MultiLevelGridNode<RecordCellData>()); // Child node
		
		grid.nodes.back().setBaseChildIndex(2);
		grid.nodes.back().markCellAsInterior(Vec4i(0,2,0, 0));
		//grid.nodes.back().setCellChildIndex(0, 2, 0,    2);
		grid.nodes.push_back(MultiLevelGridNode<RecordCellData>()); // Child node (depth 2)

		Ray ray(
			Vec4f(0, 9.5f / 64, 0.0001f, 1),
			Vec4f(1, 0, 0, 0),
			0 // min_t
		);
		float max_t = 1;

		RecordCellResults results;
		grid.trace(ray, max_t, results);
		
		testAssert(results.cells.size() == 10);
		testAssert(results.cells[0] == NodeCell(0, Vec4i(0,0,0, 0), 0.f/4, 1.f/4));

		testAssert(results.cells[1] == NodeCell(2, Vec4i(0,1,0, 0), 16.f/64, 17.f/64));
		testAssert(results.cells[2] == NodeCell(2, Vec4i(1,1,0, 0), 17.f/64, 18.f/64));
		testAssert(results.cells[3] == NodeCell(2, Vec4i(2,1,0, 0), 18.f/64, 19.f/64));
		testAssert(results.cells[4] == NodeCell(2, Vec4i(3,1,0, 0), 19.f/64, 20.f/64));

		testAssert(results.cells[5] == NodeCell(1, Vec4i(1,2,0, 0), 20.f/64, 24.f/64));
		testAssert(results.cells[6] == NodeCell(1, Vec4i(2,2,0, 0), 24.f/64, 28.f/64));
		testAssert(results.cells[7] == NodeCell(1, Vec4i(3,2,0, 0), 28.f/64, 32.f/64));

		testAssert(results.cells[8] == NodeCell(0, Vec4i(2,0,0, 0), 2.f/4, 3.f/4));
		testAssert(results.cells[9] == NodeCell(0, Vec4i(3,0,0, 0), 3.f/4, 4.f/4));
	}



	
	// Test traversal through a node with 64 children.
	{
		Vec4f min(0,0,0,1);
		Vec4f max(1,1,1,1);
		MultiLevelGrid<RecordCellVisitor, RecordCellResults, RecordCellData> grid(min, max, RecordCellVisitor());
	
		grid.nodes.push_back(MultiLevelGridNode<RecordCellData>()); // Root node
		grid.nodes.back().setBaseChildIndex(1);

		// Create 64 child nodes
		for(int z=0; z<MLG_RES; ++z)
		for(int y=0; y<MLG_RES; ++y)
		for(int x=0; x<MLG_RES; ++x)
		{
			grid.nodes[0].markCellAsInterior(Vec4i(x,y,z, 0));
			grid.nodes.push_back(MultiLevelGridNode<RecordCellData>()); // Child node
		}
		

		Vec4f p;
		Ray ray(
			Vec4f(0, 0.0001f, 0.0001f, 1),
			Vec4f(1, 0, 0, 0),
			0 // min_t
		);
		float max_t = 1;

		RecordCellResults results;
		grid.trace(ray, max_t, results);

		testAssert(results.cells.size() == 16);
		for(int i=0; i<16; ++i)
		{
			testAssert(results.cells[i] == NodeCell(1 + i / 4, Vec4i(i % 4,0,0, 0), i / 16.f, (i + 1) / 16.f));
		}
	}

	// Test random rays
	{
		Vec4f min(0,0,0,1);
		Vec4f max(1,1,1,1);
		MultiLevelGrid<RecordCellVisitor, RecordCellResults, RecordCellData> grid(min, max, RecordCellVisitor());
	
		grid.nodes.push_back(MultiLevelGridNode<RecordCellData>()); // Root node
		grid.nodes.back().setBaseChildIndex(1);

		// Create 64 child nodes
		for(int z=0; z<MLG_RES; ++z)
		for(int y=0; y<MLG_RES; ++y)
		for(int x=0; x<MLG_RES; ++x)
		{
			grid.nodes[0].markCellAsInterior(Vec4i(x,y,z, 0));
			grid.nodes.push_back(MultiLevelGridNode<RecordCellData>()); // Child node
		}

		{
			Ray ray(
				Vec4f(0.59099758f, 0.49468884f, 0.76081836f, 1),
				Vec4f(-0.46163452f, -0.44623438f, 0.76666057f, 0),
				0);

			float max_t = 1;

			RecordCellResults results;
			grid.trace(ray, max_t, results);
		}








		MTwister rng(1);
		int NUM_RAYS = 100000;
		for(int i=0; i<NUM_RAYS; ++i)
		{
			Vec4f orig = Vec4f(0,0,0,1) + randomDir(rng) * 2;

			Vec4f dir = normalise(randomDir(rng));

			Ray ray(
				orig,
				dir,
				0 // min_t
			);
			float max_t = rng.unitRandom() * 2;

			RecordCellResults results;
			grid.trace(ray, max_t, results);

			float last_t_exit = -std::numeric_limits<float>::max();
			for(size_t z=0; z<results.cells.size(); ++z)
			{
				testAssert(results.cells[z].t_enter >= 0);
				testAssert(results.cells[z].t_enter <= results.cells[z].t_exit);

				if(z >= 1)
					testAssert(epsEqual(last_t_exit, results.cells[z].t_enter)); // Last exit should be equal to current entry point.

				last_t_exit = results.cells[z].t_exit;
			}
		}
	}

	conPrint("MultiLevelGridTests::test() done.");
}


}


#endif
