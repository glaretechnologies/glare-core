/*=====================================================================
BVHBuilderTests.cpp
-------------------
Copyright Glare Technologies Limited 2015 -
Generated at 2015-09-28 16:25:21 +0100
=====================================================================*/
#include "BVHBuilderTests.h"


#include "NonBinningBVHBuilder.h"
#include "BinningBVHBuilder.h"
#include "SBVHBuilder.h"
#include "EmbreeBVHBuilder.h"
#include "jscol_aabbox.h"
#include "../utils/TestUtils.h"
#include "../maths/PCG32.h"
#include "../utils/ShouldCancelCallback.h"
#include "../utils/StandardPrintOutput.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Vector.h"
#include "../utils/TaskManager.h"
#include "../utils/Timer.h"
#include "../utils/Plotter.h"
#include "../utils/FileUtils.h"
#include "../utils/Exception.h"
#include "../utils/PlatformUtils.h"
#include "../utils/IncludeXXHash.h"
#include "../dll/include/IndigoMesh.h"
#include "../dll/IndigoStringUtils.h"


#if BUILD_TESTS


namespace BVHBuilderTests
{


// Recursively walk down tree, making sure that node AABBs are correct.
// Returns reference node AABB
#if 0
static js::AABBox checkNode(const js::Vector<ResultNode, 64>& result_nodes, int node_index, const BVHBuilder::ResultObIndicesVec& result_indices, const js::Vector<js::AABBox, 16>& aabbs)
{
	testAssert(node_index >= 0 && node_index < (int)result_nodes.size());
	const ResultNode& node = result_nodes[node_index];
	if(node.interior)
	{
		const js::AABBox left_aabb  = checkNode(result_nodes, node.left,  result_indices, aabbs);
		const js::AABBox right_aabb = checkNode(result_nodes, node.right, result_indices, aabbs);

		js::AABBox combined_aabb = left_aabb;
		combined_aabb.enlargeToHoldAABBox(right_aabb);

		testAssert(combined_aabb == node.aabb);

		return combined_aabb;
	}
	else // Else if leaf:
	{
		// Compute AABB of leaf from leaf object AABBs
		js::AABBox leaf_geom_aabb = js::AABBox::emptyAABBox();
		for(int i=node.left; i<node.right; ++i)
			leaf_geom_aabb.enlargeToHoldAABBox(aabbs[result_indices[i]]);

		//testAssert(aabb == node.aabb);
		testAssert(leaf_geom_aabb.containsAABBox(node.aabb));

		return leaf_geom_aabb;
	}
}
#endif


void testResultsValid(const BVHBuilder::ResultObIndicesVec& result_ob_indices, const js::Vector<ResultNode, 64>& result_nodes, size_t num_obs/*const js::Vector<js::AABBox, 16>& aabbs*/, bool duplicate_prims_allowed)
{
	//checkNode(result_nodes, /*node_index=*/0, result_ob_indices, aabbs);

	// Test that the resulting object indices are a permutation of the original indices.
	std::vector<bool> seen(num_obs, false);
	for(int z=0; z<(int)result_ob_indices.size(); ++z)
	{
		const uint32 ob_i = result_ob_indices[z];
		testAssert(ob_i < (uint32)num_obs);
		if(!duplicate_prims_allowed)
			testAssert(!seen[ob_i]);
		seen[ob_i] = true;
	}

	for(int z=0; z<(int)num_obs; ++z)
		testAssert(seen[z]);
	
	// Test that the result nodes object ranges cover all leaf objects, e.g. test that each object is in a leaf node.
	std::vector<bool> ob_in_leaf(result_ob_indices.size(), false);
	for(int n=0; n<(int)result_nodes.size(); ++n)
	{
		const ResultNode& node = result_nodes[n];
		if(node.interior) 
		{
			// TODO: check AABB
			testAssert(node.left >= 0  && node.left  < (int)result_nodes.size());
			testAssert(node.right >= 0 && node.right < (int)result_nodes.size());
		}
		else // Else if leaf node:
		{
			testAssert(node.left >= 0);
			testAssert(node.left <= node.right);
			testAssert(node.right <= (int)result_ob_indices.size());

			for(int z = node.left; z < node.right; ++z)
			{
				if(!duplicate_prims_allowed)
					testAssert(!ob_in_leaf[z]);
				ob_in_leaf[z] = true;
			}
		}
	}

	for(int z=0; z<(int)result_ob_indices.size(); ++z)
		testAssert(ob_in_leaf[z]);
}


#ifndef NO_EMBREE
static void testResultsValid(const BVHBuilder::ResultObIndicesVec& result_ob_indices, const js::Vector<ResultInteriorNode, 64>& result_nodes, const js::Vector<js::AABBox, 16>& aabbs, bool duplicate_prims_allowed)
{
	// Test that the resulting object indices are a permutation of the original indices.
	std::vector<bool> seen(aabbs.size(), false);
	for(int z=0; z<(int)result_ob_indices.size(); ++z)
	{
		const uint32 ob_i = result_ob_indices[z];
		testAssert(ob_i < (uint32)aabbs.size());
		if(!duplicate_prims_allowed)
			testAssert(!seen[ob_i]);
		seen[ob_i] = true;
	}

	for(int z=0; z<(int)aabbs.size(); ++z)
		testAssert(seen[z]);

	// Test that the result nodes object ranges cover all leaf objects, e.g. test that each object is in a leaf node.
	std::vector<bool> ob_in_leaf(result_ob_indices.size(), false);
	for(int n=0; n<(int)result_nodes.size(); ++n)
	{
		const ResultInteriorNode& node = result_nodes[n];
		if(node.leftChildIsInterior())
		{
			// TODO: check AABB
			testAssert(node.left >= 0  && node.left  < (int)result_nodes.size());
		}
		else
		{
			testAssert(node.left >= 0);
			testAssert(node.left_num_prims > 0);
			
			for(int z = node.left; z < node.left + node.left_num_prims; ++z)
			{
				if(!duplicate_prims_allowed)
					testAssert(!ob_in_leaf[z]);
				ob_in_leaf[z] = true;
			}
		}

		if(node.rightChildIsInterior())
		{
			// TODO: check AABB
			testAssert(node.right >= 0  && node.right  < (int)result_nodes.size());
		}
		else
		{
			testAssert(node.right >= 0);
			testAssert(node.right_num_prims > 0);

			for(int z = node.right; z < node.right + node.right_num_prims; ++z)
			{
				if(!duplicate_prims_allowed)
					testAssert(!ob_in_leaf[z]);
				ob_in_leaf[z] = true;
			}
		}
	}

	for(int z=0; z<(int)result_ob_indices.size(); ++z)
		testAssert(ob_in_leaf[z]);
}
#endif // #ifndef NO_EMBREE


class TestShouldCancelCallback : public ShouldCancelCallback
{
public:
	virtual bool shouldCancel() { return should_cancel != 0; }

	IndigoAtomic should_cancel;
};


class CancelAfterNSecondsTask : public glare::Task
{
public:
	CancelAfterNSecondsTask(double wait_time_, TestShouldCancelCallback* callback_) : wait_time(wait_time_), callback(callback_) {}

	virtual void run(size_t thread_index)
	{
		PlatformUtils::Sleep((int)(wait_time * 1000.0));

		callback->should_cancel = 1;
	}

	double wait_time; // in seconds
	TestShouldCancelCallback* callback;
};


static void buildBuilders(const js::Vector<js::AABBox, 16>& aabbs, int num_objects, const js::Vector<BVHBuilderTri, 16>& tris, std::vector<BVHBuilderRef>& builders)
{
	const int max_num_objects_per_leaf = 16;

	{
		Reference<NonBinningBVHBuilder> builder = new NonBinningBVHBuilder(1, max_num_objects_per_leaf, 4.0f,
			aabbs.data(), // aabbs
			num_objects // num objects
		);

		// Set these multi-threading thresholds lower than normal, in order to flush out any multi-threading bugs.
		builder->axis_parallel_num_ob_threshold = 32;
		builder->new_task_num_ob_threshold = 32;
		builders.push_back(builder);
	}

	{
		Reference<BinningBVHBuilder> builder = new BinningBVHBuilder(1, max_num_objects_per_leaf, /*max_depth=*/60, 4.0f,
			num_objects // num objects
		);

		for(size_t z=0; z<tris.size(); ++z)
			builder->setObjectAABB((int)z, aabbs[z]);

		builder->new_task_num_ob_threshold = 32;
		builders.push_back(builder);
	}

	{
		Reference<SBVHBuilder> builder = new SBVHBuilder(1, max_num_objects_per_leaf, /*max_depth=*/60, 4.0f,
			tris.data(),
			num_objects // num objects
		);

		builder->new_task_num_ob_threshold = 32;
		builders.push_back(builder);
	}
}


static void testBVHBuildersWithTriangles(glare::TaskManager& task_manager, const js::Vector<BVHBuilderTri, 16>& tris)
{
	const int num_objects = (int)tris.size();

	js::Vector<js::AABBox, 16> aabbs(tris.size());
	for(size_t z=0; z<tris.size(); ++z)
	{
		aabbs[z] = js::AABBox::emptyAABBox();
		aabbs[z].enlargeToHoldPoint(tris[z].v[0]);
		aabbs[z].enlargeToHoldPoint(tris[z].v[1]);
		aabbs[z].enlargeToHoldPoint(tris[z].v[2]);
	}

	StandardPrintOutput print_output;
	DummyShouldCancelCallback dummy_should_cancel_callback;

	{
		std::vector<BVHBuilderRef> builders;
		buildBuilders(aabbs, num_objects, tris, builders);

		for(size_t i=0; i<builders.size(); ++i)
		{
			js::Vector<ResultNode, 64> result_nodes;
			builders[i]->build(task_manager,
				dummy_should_cancel_callback,
				print_output,
				result_nodes
			);

			const bool duplicate_prims_allowed = builders[i].isType<SBVHBuilder>();
			testResultsValid(builders[i]->getResultObjectIndices(), result_nodes, aabbs.size(), duplicate_prims_allowed);
		}
	}

	PCG32 rng(1);

	// Test cancelling
	{
		std::vector<BVHBuilderRef> builders;
		buildBuilders(aabbs, num_objects, tris, builders);
		

		for(size_t i=0; i<builders.size(); ++i)
		{
			// Run once to measure expected time
			double first_run_time;
			{
				Timer timer;
				js::Vector<ResultNode, 64> result_nodes;
				builders[i]->build(task_manager,
					dummy_should_cancel_callback,
					print_output,
					result_nodes
				);
				first_run_time = timer.elapsed();
			}

			conPrint("First build took " + doubleToStringNSigFigs(first_run_time, 3) + " s");

			const int NUM_TESTS = 3;
			for(int z=0; z<NUM_TESTS; ++z)
			{
				TestShouldCancelCallback test_should_cancel_callback;
				glare::TaskManager temp_task_manager(1);
				temp_task_manager.addTask(new CancelAfterNSecondsTask(first_run_time * (-0.1 + rng.unitRandom() * 1.2), &test_should_cancel_callback));
				Timer timer;
				try
				{
					js::Vector<ResultNode, 64> result_nodes;
					builders[i]->build(task_manager,
						test_should_cancel_callback,
						print_output,
						result_nodes
					);

					conPrint("Build was not interrupted.");
				}
				catch(glare::CancelledException& )
				{
					// Expected
					conPrint("Successfully interrupted build after " + timer.elapsedStringNSigFigs(3));
				}
				catch(...)
				{
					failTest("Caught exception");
				}
			}
		}
	}
}


static void testBVHBuildersWithNRandomObjects(glare::TaskManager& task_manager, int num_objects)
{
	PCG32 rng(1);
	js::Vector<BVHBuilderTri, 16> tris(num_objects);
	for(int z=0; z<num_objects; ++z)
	{
		const Vec4f v0(rng.unitRandom() * 0.8f, rng.unitRandom() * 0.8f, rng.unitRandom() * 0.8f, 1);
		const Vec4f v1 = v0 + Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 0) * 0.02f;
		const Vec4f v2 = v0 + Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 0) * 0.02f;
		tris[z].v[0] = v0;
		tris[z].v[1] = v1;
		tris[z].v[2] = v2;
	}

	testBVHBuildersWithTriangles(task_manager, tris);
}


#if 0
static void testBVHBuilderWithNRandomObjectsGetResults(glare::TaskManager& task_manager, int num_objects, js::Vector<ResultNode, 64>& result_nodes_out)
{
	PCG32 rng(1);
	StandardPrintOutput print_output;

	js::Vector<js::AABBox, 16> aabbs(num_objects);
	for(int z=0; z<num_objects; ++z)
	{
		const Vec4f p(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1);
		aabbs[z] = js::AABBox(p, p + Vec4f(0.01f, 0.01f, 0.01f, 0));
	}

	const int max_num_objects_per_leaf = 16;
	Reference<NonBinningBVHBuilder> builder = new NonBinningBVHBuilder(1, max_num_objects_per_leaf, 4.0f,
		aabbs.data(), // aabbs
		num_objects // num objects
	);

	// Set these multi-threading thresholds lower than normal, in order to flush out any multi-threading bugs.
	builder->axis_parallel_num_ob_threshold = 32;
	builder->new_task_num_ob_threshold = 32;

	//js::Vector<ResultNode, 64> result_nodes;
	builder->build(task_manager,
		print_output,
		result_nodes_out
	);

	testResultsValid(builder->getResultObjectIndices(), result_nodes_out, aabbs, /*duplicate_prims_allowed=*/false);
}
#endif


void test()
{
	conPrint("BVHBuilderTests::test()");

	PCG32 rng(1);
	glare::TaskManager task_manager;
	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;

	
	/*{
		int num_objects = 8;
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		for(int z=0; z<num_objects; ++z)
			aabbs[z] = js::AABBox(Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1), Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1));

		js::Vector<ResultNode, 64> result_nodes;

		const int max_num_objects_per_leaf = 16;
		BVHBuilder builder(1, max_num_objects_per_leaf, 1.0f);
		BVHBuilderTestsCallBack callback(max_num_objects_per_leaf);
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			callback,
			result_nodes
		);

		BVHBuilder::printResultNodes(result_nodes);

		testResultsValid(builder, result_nodes, num_objects);
	}*/


	//===================== Test lots of little random objects, plus a large object that spans the entire range. =====================
	/*{
		const int num_objects = 32;
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		for(int z=0; z<num_objects; ++z)
		{
			const Vec4f p(rng.unitRandom() * 0.98f, rng.unitRandom() * 0.98f, rng.unitRandom() * 0.98f, 1);
			aabbs[z] = js::AABBox(p, p + Vec4f(0.01f, 0.01f, 0.01f, 0));
		}

		aabbs[0] = js::AABBox(Vec4f(0,0,0, 1), Vec4f(1,1,1, 1));

		const int max_num_objects_per_leaf = 16;
		BVHBuilder builder(1, max_num_objects_per_leaf, 100.0f);

		// Set these multi-threading thresholds lower than normal, in order to flush out any multi-threading bugs.
		builder.axis_parallel_num_ob_threshold = 30002;
		builder.new_task_num_ob_threshold = 30002; 

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			result_nodes
		);

		testResultsValid(builder, result_nodes, num_objects);
	}*/


#if 0
	//==========================================
	/*
	|----------0----------|
	|----------1----------|
	----------------------|-------> x
	                      1
													 */
	{
		const int num_objects = 2;
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		aabbs[0] = js::AABBox(Vec4f(0.0f,0,0,1), Vec4f(1.f,0,0.1f,1));
		aabbs[1] = js::AABBox(Vec4f(0.0f,0,0,1), Vec4f(1.f,0,0.1f,1));

		BVHBuilder builder(1, 16, 100.0f);

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			result_nodes
		);

		testResultsValid(builder.getResultObjectIndices(), result_nodes, aabbs);
		testAssert(result_nodes.size() == 1 && !result_nodes[0].interior); // Should just be one leaf node.
	}

	//==========================================
	/*
	      |----0----|
	|----------1----------|
	----------------------|-------> x
	                      1
													 */
	{
		const int num_objects = 2;
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		aabbs[0] = js::AABBox(Vec4f(0.25f,0,0,1), Vec4f(0.75f,0,0.1f,1));
		aabbs[1] = js::AABBox(Vec4f(0.0f,0,0,1), Vec4f(1.f,0,0.1f,1));

		BVHBuilder builder(1, 16, 100.0f);

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			result_nodes
		);

		testResultsValid(builder.getResultObjectIndices(), result_nodes, aabbs);
		testAssert(result_nodes.size() == 1 && !result_nodes[0].interior); // Should just be one leaf node.
	}


	//==========================================
	/*
	|----------0----------|
	      |----1----|
	----------------------|-------> x
	                      1
													 */
	{
		const int num_objects = 2;
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		aabbs[0] = js::AABBox(Vec4f(0.0f,0,0,1), Vec4f(1.f,0,0.1f,1));
		aabbs[1] = js::AABBox(Vec4f(0.25f,0,0,1), Vec4f(0.75f,0,0.1f,1));

		BVHBuilder builder(1, 16, 100.0f);

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			result_nodes
		);

		testResultsValid(builder.getResultObjectIndices(), result_nodes, aabbs);
		testAssert(result_nodes.size() == 1 && !result_nodes[0].interior); // Should just be one leaf node.
	}


	//==========================================
	/*
	|-----------0----------|
	                   |---------------1---------------|

	                       1                           2
													 */
	/*{
		const int num_objects = 2;
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		aabbs[0] = js::AABBox(Vec4f(0.0f,0,0,1), Vec4f(1.f,0,0.1f,1));
		aabbs[1] = js::AABBox(Vec4f(0.8f,0,0,1), Vec4f(2.f,0,0.1f,1));

		const int max_num_objects_per_leaf = 16;
		BVHBuilder builder(1, max_num_objects_per_leaf, 100.0f);

		// Set these multi-threading thresholds lower than normal, in order to flush out any multi-threading bugs.
		builder.axis_parallel_num_ob_threshold = 32;
		builder.new_task_num_ob_threshold = 32; 

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			result_nodes
		);

		testResultsValid(builder, result_nodes, num_objects);
	}*/

	//==========================================
	/*
	|-0-|
	      |-1-|
       |------------------------2----------------------|
	                       1                           2
													 */
	/*{
		const int num_objects = 3;
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		aabbs[0] = js::AABBox(Vec4f(0.1f,0,0,1), Vec4f(0.2f,0,0.1f,1));
		aabbs[1] = js::AABBox(Vec4f(0.3f,0,0,1), Vec4f(0.4f,0,0.1f,1));
		aabbs[2] = js::AABBox(Vec4f(0.05f,0,0,1), Vec4f(2.f,0,0.1f,1));

		const int max_num_objects_per_leaf = 16;
		BVHBuilder builder(1, max_num_objects_per_leaf, 100.0f);

		// Set these multi-threading thresholds lower than normal, in order to flush out any multi-threading bugs.
		builder.axis_parallel_num_ob_threshold = 32;
		builder.new_task_num_ob_threshold = 32; 

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			result_nodes
		);

		testResultsValid(builder, result_nodes, num_objects);
	}*/


	//==========================================
	/*
	  |---------0--------|
	|-----------1----------|
	                   |---------------2---------------|
	                             |-----3-----|
	                                               |----4----|   
	            0.5        1          1.3                    2
													 */

	{
		const int num_objects = 5;
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		aabbs[0] = js::AABBox(Vec4f(0.2f,0,0,1), Vec4f(0.8f,0,0.1f,1));
		aabbs[1] = js::AABBox(Vec4f(0.0f,0,0,1), Vec4f(1.0f,0,0.1f,1));
		aabbs[2] = js::AABBox(Vec4f(0.8f,0,0,1), Vec4f(1.8f,0,0.1f,1));
		aabbs[3] = js::AABBox(Vec4f(1.1f,0,0,1), Vec4f(1.5f,0,0.1f,1));
		aabbs[4] = js::AABBox(Vec4f(1.8f,0,0,1), Vec4f(2.0f,0,0.1f,1));

		const int max_num_objects_per_leaf = 16;
		BVHBuilder builder(1, max_num_objects_per_leaf, 100.0f);

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			result_nodes
		);

		testResultsValid(builder.getResultObjectIndices(), result_nodes, aabbs);
	}

	//====================================================================
	/*
	  |---------0--------|
	|-----------1----------|
	                             |-----2-----|
	                   |---------------3---------------|
	                                               |----4----|   
	            0.5        1          1.3                    2
													 */

	{
		const int num_objects = 5;
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		aabbs[0] = js::AABBox(Vec4f(0.2f,0,0,1), Vec4f(0.8f,0,0.1f,1));
		aabbs[1] = js::AABBox(Vec4f(0.0f,0,0,1), Vec4f(1.0f,0,0.1f,1));
		aabbs[2] = js::AABBox(Vec4f(1.1f,0,0,1), Vec4f(1.5f,0,0.1f,1));
		aabbs[3] = js::AABBox(Vec4f(0.8f,0,0,1), Vec4f(1.8f,0,0.1f,1));
		aabbs[4] = js::AABBox(Vec4f(1.8f,0,0,1), Vec4f(2.0f,0,0.1f,1));

		const int max_num_objects_per_leaf = 16;
		BVHBuilder builder(1, max_num_objects_per_leaf, 100.0f);

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			result_nodes
		);

		testResultsValid(builder.getResultObjectIndices(), result_nodes, aabbs);
	}

	//====================================================================
	/*
	|-----0-----|
	            |------1-----|
	                                       |------2------|
	           0.5            1           1.5            2
													
		
	Resulting tree:
	            0
	           / \
	         1     2
	       /  \
	      3    4
													*/

	{
		const int num_objects = 3;
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		aabbs[0] = js::AABBox(Vec4f(0.0f,0,0,1), Vec4f(0.5f,0,0.1f,1));
		aabbs[1] = js::AABBox(Vec4f(0.5f,0,0,1), Vec4f(1.0f,0,0.1f,1));
		aabbs[2] = js::AABBox(Vec4f(1.5f,0,0,1), Vec4f(2.0f,0,0.1f,1));

		const int max_num_objects_per_leaf = 16;
		BVHBuilder builder(1, max_num_objects_per_leaf, /*intersection_cost=*/100.0f);

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			result_nodes
		);

		testResultsValid(builder.getResultObjectIndices(), result_nodes, aabbs);
		testAssert(result_nodes.size() == 5);
		testAssert(result_nodes[0].left == 1);
		testAssert(result_nodes[0].right == 2);
		testAssert(result_nodes[1].interior);
		testAssert(result_nodes[1].left == 3);
		testAssert(result_nodes[1].right == 4);

		// Test that node 3 is a leaf containing object 0
		testAssert(!result_nodes[3].interior);
		testAssert(result_nodes[3].right - result_nodes[3].left == 1);
		testAssert(builder.getResultObjectIndices()[result_nodes[3].left] == 0);

		testAssert(!result_nodes[4].interior);
		testAssert(result_nodes[4].right - result_nodes[4].left == 1);
		testAssert(builder.getResultObjectIndices()[result_nodes[4].left] == 1);

		testAssert(!result_nodes[2].interior);
		testAssert(result_nodes[2].right - result_nodes[2].left == 1);
		testAssert(builder.getResultObjectIndices()[result_nodes[2].left] == 2);
	}

	//====================================================================
	/*
	Test with object 1 off to the right.  Tree should be the same as previously,
	but node 2 should have object 1 in it.

	|-----0-----|
	                                       |------1-----|
	            |------2------|
	           0.5            1           1.5            2
													
		
	Resulting tree:
	            0
	           / \
	         1     2
	       /  \
	      3    4
													*/

	{
		const int num_objects = 3;
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		aabbs[0] = js::AABBox(Vec4f(0.0f,0,0,1), Vec4f(0.5f,0,0.1f,1));
		aabbs[1] = js::AABBox(Vec4f(1.5f,0,0,1), Vec4f(2.0f,0,0.1f,1));
		aabbs[2] = js::AABBox(Vec4f(0.5f,0,0,1), Vec4f(1.0f,0,0.1f,1));

		const int max_num_objects_per_leaf = 16;
		BVHBuilder builder(1, max_num_objects_per_leaf, /*intersection_cost=*/100.0f);

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			result_nodes
		);

		testResultsValid(builder.getResultObjectIndices(), result_nodes, aabbs);
		testAssert(result_nodes.size() == 5);
		testAssert(result_nodes[0].left == 1);
		testAssert(result_nodes[0].right == 2);
		testAssert(result_nodes[1].interior);
		testAssert(result_nodes[1].left == 3);
		testAssert(result_nodes[1].right == 4);

		// Test that node 3 is a leaf containing object 0
		testAssert(!result_nodes[3].interior);
		testAssert(result_nodes[3].right - result_nodes[3].left == 1);
		testAssert(builder.getResultObjectIndices()[result_nodes[3].left] == 0);

		testAssert(!result_nodes[4].interior);
		testAssert(result_nodes[4].right - result_nodes[4].left == 1);
		testAssert(builder.getResultObjectIndices()[result_nodes[4].left] == 2);

		testAssert(!result_nodes[2].interior);
		testAssert(result_nodes[2].right - result_nodes[2].left == 1);
		testAssert(builder.getResultObjectIndices()[result_nodes[2].left] == 1);
	}
#endif
	
	//==================== Test building a BVH with varying numbers of objects (including zero objects) ====================
	for(int num_objects=0; num_objects<64; ++num_objects)
	{
		testBVHBuildersWithNRandomObjects(task_manager, num_objects);
	}

	for(int num_objects=64; num_objects<=4096; num_objects *= 2)
	{
		testBVHBuildersWithNRandomObjects(task_manager, num_objects);
	}


	//==================== Do a stress test with a reasonably large amount of objects ====================
	{
		conPrint("StressTest...");
		testBVHBuildersWithNRandomObjects(task_manager,
			10000
		);

		testBVHBuildersWithNRandomObjects(task_manager,
			100000
		);

		conPrint("Done.");
	}

	
	//==================== Test building a BVH with lots of objects with the same BVH ====================
	// This means that they can't get split as normal, but must still be split up to enforce the max_num_objects_per_leaf limit.
	{
		std::vector<int> test_sizes;
		for(int num_objects=1; num_objects<64; ++num_objects)
			test_sizes.push_back(num_objects);

		test_sizes.push_back(1000);
		test_sizes.push_back(10000);

		for(size_t i=0; i<test_sizes.size(); ++i)
		{
			const int num_objects = test_sizes[i];
			js::Vector<BVHBuilderTri, 16> tris(num_objects);
			for(int z=0; z<num_objects; ++z)
			{
				// Use the same tri / AABB for each object.
				tris[z].v[0] = Vec4f(0, 0, 0, 1);
				tris[z].v[1] = Vec4f(1, 0, 0, 1);
				tris[z].v[2] = Vec4f(1, 1, 0, 1);
			}

			testBVHBuildersWithTriangles(task_manager, tris);
		}
	}


	//==================== Test building on every igmesh we can find ====================
	if(false)
	{
		const std::vector<std::string> files = FileUtils::getFilesInDirWithExtensionFullPathsRecursive(TestUtils::getIndigoTestReposDir(), "igmesh");
		for(size_t i=0; i<files.size(); ++i)
		{
			conPrint("Building '" + files[i] + "'...");

			Indigo::Mesh mesh;
			Indigo::Mesh::readFromFile(toIndigoString(files[i]), mesh);
			js::Vector<js::AABBox, 16> aabbs(mesh.triangles.size() + mesh.quads.size());
			for(size_t t=0; t<mesh.triangles.size(); ++t)
			{
				Vec4f v0(mesh.vert_positions[mesh.triangles[t].vertex_indices[0]].x, mesh.vert_positions[mesh.triangles[t].vertex_indices[0]].y, mesh.vert_positions[mesh.triangles[t].vertex_indices[0]].z, 1.);
				Vec4f v1(mesh.vert_positions[mesh.triangles[t].vertex_indices[1]].x, mesh.vert_positions[mesh.triangles[t].vertex_indices[1]].y, mesh.vert_positions[mesh.triangles[t].vertex_indices[1]].z, 1.);
				Vec4f v2(mesh.vert_positions[mesh.triangles[t].vertex_indices[2]].x, mesh.vert_positions[mesh.triangles[t].vertex_indices[2]].y, mesh.vert_positions[mesh.triangles[t].vertex_indices[2]].z, 1.);
				
				aabbs[t] = js::AABBox(v0, v0);
				aabbs[t].enlargeToHoldPoint(v1);
				aabbs[t].enlargeToHoldPoint(v2);
			}
			for(size_t q=0; q<mesh.quads.size(); ++q)
			{
				Vec4f v0(mesh.vert_positions[mesh.quads[q].vertex_indices[0]].x, mesh.vert_positions[mesh.quads[q].vertex_indices[0]].y, mesh.vert_positions[mesh.quads[q].vertex_indices[0]].z, 1.);
				Vec4f v1(mesh.vert_positions[mesh.quads[q].vertex_indices[1]].x, mesh.vert_positions[mesh.quads[q].vertex_indices[1]].y, mesh.vert_positions[mesh.quads[q].vertex_indices[1]].z, 1.);
				Vec4f v2(mesh.vert_positions[mesh.quads[q].vertex_indices[2]].x, mesh.vert_positions[mesh.quads[q].vertex_indices[2]].y, mesh.vert_positions[mesh.quads[q].vertex_indices[2]].z, 1.);
				Vec4f v3(mesh.vert_positions[mesh.quads[q].vertex_indices[3]].x, mesh.vert_positions[mesh.quads[q].vertex_indices[3]].y, mesh.vert_positions[mesh.quads[q].vertex_indices[3]].z, 1.);

				aabbs[mesh.triangles.size() + q] = js::AABBox(v0, v0);
				aabbs[mesh.triangles.size() + q].enlargeToHoldPoint(v1);
				aabbs[mesh.triangles.size() + q].enlargeToHoldPoint(v2);
				aabbs[mesh.triangles.size() + q].enlargeToHoldPoint(v3);
			}

			const int max_num_objects_per_leaf = 16;
			const float intersection_cost = 10.f;
			NonBinningBVHBuilder builder(1, max_num_objects_per_leaf, intersection_cost, aabbs.data(), // aabbs
				(int)aabbs.size() // num objects
			);
			js::Vector<ResultNode, 64> result_nodes;
			builder.build(task_manager,
				should_cancel_callback,
				print_output,
				result_nodes
			);
		}
	}


	{
		PCG32 rng_(1);
		js::Vector<BVHBuilderTri, 16> tris(3);

		// Tris in rect like in paper
		{
			tris[0].v[0] = Vec4f(0.2, 0, 0.000000000, 1.00000000);
			tris[0].v[1] = Vec4f(1, 0.8, 0.000000000, 1.00000000);
			tris[0].v[2] = Vec4f(0.8, 1, 0.000000000, 1.00000000);
		}

		{
			tris[1].v[0] = Vec4f(0.2, 0, 0.000000000, 1.00000000);
			tris[1].v[1] = Vec4f(0.8, 1, 0.000000000, 1.00000000);
			tris[1].v[2] = Vec4f(0, 0.2, 0.000000000, 1.00000000);
		}

		{
			tris[2].v[0] = Vec4f(0.45, 0.45, 0.000000000, 1.00000000);
			tris[2].v[1] = Vec4f(0.55, 0.45, 0.000000000, 1.00000000);
			tris[2].v[2] = Vec4f(0.55, 0.55, 0.000000000, 1.00000000);
		}

		/*{
			tris[0].v[0] = Vec4f(0.0, 0,   0.000000000, 1.00000000);
			tris[0].v[1] = Vec4f(0.6, 0.1, 0.000000000, 1.00000000);
			tris[0].v[2] = Vec4f(0.3, 0.1, 0.000000000, 1.00000000);
			aabbs[0] = js::AABBox::emptyAABBox();
			aabbs[0].enlargeToHoldPoint(tris[0].v[0]);
			aabbs[0].enlargeToHoldPoint(tris[0].v[1]);
			aabbs[0].enlargeToHoldPoint(tris[0].v[2]);
		}

		{
			tris[1].v[0] = Vec4f(0.4, 0, 0.000000000, 1.00000000);
			tris[1].v[1] = Vec4f(1.0, 0.1, 0.000000000, 1.00000000);
			tris[1].v[2] = Vec4f(0.6, 0.1, 0.000000000, 1.00000000);
			aabbs[1] = js::AABBox::emptyAABBox();
			aabbs[1].enlargeToHoldPoint(tris[1].v[0]);
			aabbs[1].enlargeToHoldPoint(tris[1].v[1]);
			aabbs[1].enlargeToHoldPoint(tris[1].v[2]);
		}*/
		
		/*
tri	{v=0x000000000810edc0 {{x=0x000000000810edc0 {0.0500073023, 0.0621716492, 0.000000000, 1.00000000} v=...}, ...} }	const SBVHTri &
	v	0x000000000810edc0 {{x=0x000000000810edc0 {0.0500073023, 0.0621716492, 0.000000000, 1.00000000} v={m128_f32=...} }, ...}	Vec4f[3]
+		[0]	{x=0x000000000810edc0 {0.0500073023, 0.0621716492, 0.000000000, 1.00000000} v={m128_f32=0x000000000810edc0 {...} ...} }	Vec4f
+		[1]	{x=0x000000000810edd0 {0.137831196, 0.126597553, 0.000000000, 1.00000000} v={m128_f32=0x000000000810edd0 {...} ...} }	Vec4f
+		[2]	{x=0x000000000810ede0 {0.222000197, 0.251402110, 0.000000000, 1.00000000} v={m128_f32=0x000000000810ede0 {...} ...} }	Vec4f

tri	{v=0x000000000810f600 {{x=0x000000000810f600 {0.0811252221, 0.0860071033, 0.000000000, 1.00000000} v=...}, ...} }	const SBVHTri &
	v	0x000000000810f600 {{x=0x000000000810f600 {0.0811252221, 0.0860071033, 0.000000000, 1.00000000} v={m128_f32=...} }, ...}	Vec4f[3]
+		[0]	{x=0x000000000810f600 {0.0811252221, 0.0860071033, 0.000000000, 1.00000000} v={m128_f32=0x000000000810f600 {...} ...} }	Vec4f
+		[1]	{x=0x000000000810f610 {0.198112756, 0.210741162, 0.000000000, 1.00000000} v={m128_f32=0x000000000810f610 {...} ...} }	Vec4f
+		[2]	{x=0x000000000810f620 {0.262631893, 0.0956056118, 0.000000000, 1.00000000} v={m128_f32=0x000000000810f620 {...} ...} }	Vec4f

tri	{v=0x000000000810fff0 {{x=0x000000000810fff0 {0.0515251160, 0.0506747477, 0.000000000, 1.00000000} v=...}, ...} }	const SBVHTri &
	v	0x000000000810fff0 {{x=0x000000000810fff0 {0.0515251160, 0.0506747477, 0.000000000, 1.00000000} v={m128_f32=...} }, ...}	Vec4f[3]
+		[0]	{x=0x000000000810fff0 {0.0515251160, 0.0506747477, 0.000000000, 1.00000000} v={m128_f32=0x000000000810fff0 {...} ...} }	Vec4f
+		[1]	{x=0x0000000008110000 {0.142836809, 0.135006323, 0.000000000, 1.00000000} v={m128_f32=0x0000000008110000 {...} ...} }	Vec4f
+		[2]	{x=0x0000000008110010 {0.109402373, 0.223440573, 0.000000000, 1.00000000} v={m128_f32=0x0000000008110010 {...} ...} }	Vec4f

*/
		/*{
			tris[0].v[0] = Vec4f(0.0500073023, 0.0621716492, 0.000000000, 1.00000000);
			tris[0].v[1] = Vec4f(0.137831196, 0.126597553, 0.000000000, 1.00000000);
			tris[0].v[2] = Vec4f(0.222000197, 0.251402110, 0.000000000, 1.00000000);
			aabbs[0] = js::AABBox::emptyAABBox();
			aabbs[0].enlargeToHoldPoint(tris[0].v[0]);
			aabbs[0].enlargeToHoldPoint(tris[0].v[1]);
			aabbs[0].enlargeToHoldPoint(tris[0].v[2]);
		}

		{
			tris[1].v[0] = Vec4f(0.0811252221, 0.0860071033, 0.000000000, 1.00000000);
			tris[1].v[1] = Vec4f(0.198112756, 0.210741162, 0.000000000, 1.00000000);
			tris[1].v[2] = Vec4f(0.262631893, 0.0956056118, 0.000000000, 1.00000000);
			aabbs[1] = js::AABBox::emptyAABBox();
			aabbs[1].enlargeToHoldPoint(tris[1].v[0]);
			aabbs[1].enlargeToHoldPoint(tris[1].v[1]);
			aabbs[1].enlargeToHoldPoint(tris[1].v[2]);
		}

		{
			tris[2].v[0] = Vec4f(0.0515251160, 0.0506747477, 0.000000000, 1.00000000);
			tris[2].v[1] = Vec4f(0.142836809, 0.135006323, 0.000000000, 1.00000000);
			tris[2].v[2] = Vec4f(0.109402373, 0.223440573, 0.000000000, 1.00000000);
			aabbs[2] = js::AABBox::emptyAABBox();
			aabbs[2].enlargeToHoldPoint(tris[2].v[0]);
			aabbs[2].enlargeToHoldPoint(tris[2].v[1]);
			aabbs[2].enlargeToHoldPoint(tris[2].v[2]);
		}*/

		
		// Triangles with one edge on the splitting plane (x=0.5)
		/*{
			const Vec4f v0(0.5f, 0.1f, 0.f, 1);
			const Vec4f v1(0.9f, 0.3f, 0.f, 1);
			const Vec4f v2(0.5f, 0.4f, 0.f, 1);
			tris[0].v[0] = v0;
			tris[0].v[1] = v1;
			tris[0].v[2] = v2;
			aabbs[0] = js::AABBox::emptyAABBox();
			aabbs[0].enlargeToHoldPoint(v0);
			aabbs[0].enlargeToHoldPoint(v1);
			aabbs[0].enlargeToHoldPoint(v2);
		}

		{
			const Vec4f v0(0.5f, 0.4f, 0.f, 1);
			const Vec4f v1(0.1f, 0.5f, 0.f, 1);
			const Vec4f v2(0.5f, 0.6f, 0.f, 1);
			tris[1].v[0] = v0;
			tris[1].v[1] = v1;
			tris[1].v[2] = v2;
			aabbs[1] = js::AABBox::emptyAABBox();
			aabbs[1].enlargeToHoldPoint(v0);
			aabbs[1].enlargeToHoldPoint(v1);
			aabbs[1].enlargeToHoldPoint(v2);
		}*/
		/*{
			const Vec4f v0(0.3f, 0.45f, 0.f, 1);
			const Vec4f v1(0.5f, 0.3f, 0.f, 1);
			const Vec4f v2(0.6f, 0.55f, 0.f, 1);
			tris[0].v[0] = v0;
			tris[0].v[1] = v1;
			tris[0].v[2] = v2;
			aabbs[0] = js::AABBox::emptyAABBox();
			aabbs[0].enlargeToHoldPoint(v0);
			aabbs[0].enlargeToHoldPoint(v1);
			aabbs[0].enlargeToHoldPoint(v2);
		}
		{
			const Vec4f v0(0.2f, 0.65f, 0.f, 1);
			const Vec4f v1(0.9f, 0.9f, 0.f, 1);
			const Vec4f v2(0.1f, 0.8f, 0.f, 1);
			tris[1].v[0] = v0;
			tris[1].v[1] = v1;
			tris[1].v[2] = v2;
			aabbs[1] = js::AABBox::emptyAABBox();
			aabbs[1].enlargeToHoldPoint(v0);
			aabbs[1].enlargeToHoldPoint(v1);
			aabbs[1].enlargeToHoldPoint(v2);
		}*/

		testBVHBuildersWithTriangles(task_manager, tris);
	}


	const bool DO_PERF_TESTS = false;
	if(DO_PERF_TESTS)
	{
		const int num_objects = 2000000;

		PCG32 rng_(1);
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		js::Vector<BVHBuilderTri, 16> tris(num_objects);
		for(int z=0; z<num_objects; ++z)
		{
			const Vec4f v0(rng_.unitRandom() * 0.8f, rng_.unitRandom() * 0.8f, 0/*rng_.unitRandom()*/ * 0.8f, 1);
			const Vec4f v1 = v0 + Vec4f(rng_.unitRandom(), rng_.unitRandom(), /*rng_.unitRandom()*/0, 0) * 0.02f;
			const Vec4f v2 = v0 + Vec4f(rng_.unitRandom(), rng_.unitRandom(), /*rng_.unitRandom()*/0, 0) * 0.02f;
			tris[z].v[0] = v0;
			tris[z].v[1] = v1;
			tris[z].v[2] = v2;
			aabbs[z] = js::AABBox::emptyAABBox();
			aabbs[z].enlargeToHoldPoint(v0);
			aabbs[z].enlargeToHoldPoint(v1);
			aabbs[z].enlargeToHoldPoint(v2);
		}

		double sum_time = 0;
		double min_time = 1.0e100;
		const int NUM_ITERS = 100;
		for(int q=0; q<NUM_ITERS; ++q)
		{
			//conPrint("------------- perf test --------------");
			Timer timer;

			const int max_num_objects_per_leaf = 16;
			const float intersection_cost = 1.f;

			//------------- Embree -----------------
#ifndef NO_EMBREE
#if 1
			const bool DO_SBVH_BUILD = true;
			EmbreeBVHBuilder builder(DO_SBVH_BUILD, max_num_objects_per_leaf, intersection_cost,
				tris.data(),
				num_objects
			);
			js::Vector<ResultInteriorNode, 64> result_interior_nodes;
			builder.doBuild(task_manager,
				should_cancel_callback,
				print_output,
				result_interior_nodes
			);

			const double elapsed = timer.elapsed();
			sum_time += elapsed;
			min_time = myMin(min_time, elapsed);
			conPrint("Embree: BVH building for " + toString(num_objects) + " objects took " + toString(elapsed) + " s");

			testResultsValid(builder.getResultObjectIndices(), result_interior_nodes, aabbs, /*duplicate_prims_allowed=*/DO_SBVH_BUILD);
#endif
#endif
			//--------------------------------------

			//------------- BinningBVHBuilder -----------------
#if 0
			BinningBVHBuilder builder(1, max_num_objects_per_leaf, intersection_cost,
				num_objects
			);
			for(int z=0; z<num_objects; ++z)
				builder.setObjectAABB(z, aabbs[z]);

			js::Vector<ResultNode, 64> result_nodes;
			builder.build(task_manager,
				should_cancel_callback,
				print_output,
				result_nodes
			);

			const double elapsed = timer.elapsed();
			sum_time += elapsed;
			min_time = myMin(min_time, elapsed);
			conPrint("BVH building for " + toString(num_objects) + " objects took " + toString(elapsed) + " s");

			testResultsValid(builder.getResultObjectIndices(), result_nodes, aabbs, /*duplicate_prims_allowed=*/false);
#endif
			//--------------------------------------

			//------------- SBVHBuilder -----------------
#if 0
			SBVHBuilder builder(1, max_num_objects_per_leaf, intersection_cost,
				tris.data(),
				num_objects
			);
			//for(int z=0; z<num_objects; ++z)
			//	builder.setObjectAABB(z, aabbs[z]);

			js::Vector<ResultNode, 64> result_nodes;
			builder.build(task_manager,
				should_cancel_callback,
				print_output,
				result_nodes
			);

			const double elapsed = timer.elapsed();
			sum_time += elapsed;
			min_time = myMin(min_time, elapsed);
			conPrint("SBVHBuilder: BVH building for " + toString(num_objects) + " objects took " + toString(elapsed) + " s");

			testResultsValid(builder.getResultObjectIndices(), result_nodes, aabbs, /*duplicate_prims_allowed=*/true);
#endif
			//--------------------------------------
		}

		const double av_time = sum_time / NUM_ITERS;
		conPrint("av_time:  " + toString(av_time) + " s");
		conPrint("min_time: " + toString(min_time) + " s");
	}

	//==================== Test varying a build parameter and plotting the resulting speeds ====================
	if(false)
	{
		Plotter plotter;
		Plotter::DataSet dataset;

		const int num_objects = 100000;

		js::Vector<js::AABBox, 16> aabbs(num_objects);
		for(int z=0; z<num_objects; ++z)
		{
			const Vec4f p(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1);
			aabbs[z] = js::AABBox(p, p + Vec4f(0.01f, 0.01f, 0.01f, 0));
		}

		double largest_time = 0;
		for(int axis_parallel_num_ob_threshold = 1 << 10; axis_parallel_num_ob_threshold < (1 << 25); axis_parallel_num_ob_threshold *= 2)
		{
			double min_time = 1.0e30;
			for(int q=0; q<20; ++q)
			{
				//conPrint("------------- perf test --------------");
				Timer timer;

				const int max_num_objects_per_leaf = 16;
				const float intersection_cost = 10.f;
				Reference<NonBinningBVHBuilder> builder = new NonBinningBVHBuilder(1, max_num_objects_per_leaf, intersection_cost, 
					aabbs.data(), // aabbs
					num_objects // num objects
				);
				builder->axis_parallel_num_ob_threshold = axis_parallel_num_ob_threshold;
				js::Vector<ResultNode, 64> result_nodes;
				builder->build(task_manager,
					should_cancel_callback,
					print_output,
					result_nodes
				);

				conPrint("axis_parallel_num_ob_threshold: " + toString(axis_parallel_num_ob_threshold) + ": BVH building for " + toString(num_objects) + " objects took " + timer.elapsedString());

				//testResultsValid(builder, result_nodes, num_objects);
				min_time = myMin(min_time, timer.elapsed());
			}
			dataset.points.push_back(Vec2f((float)axis_parallel_num_ob_threshold, (float)min_time));
			largest_time = myMax(largest_time, min_time);
		}

		Plotter::PlotOptions options;
		options.x_axis_log = true;
		options.explicit_y_range = true;
		options.y_start = 0;
		options.y_end = largest_time * 1.2f;
		Plotter::plot("bvh_builder_speed_vs_axis_parallel_num_ob_threshold_" + toString(num_objects) + "_objects.png",
			"BVH construction time for " + toString(num_objects) + " objects", "axis_parallel_num_ob_threshold", "elapsed (s)", std::vector<Plotter::DataSet>(1, dataset), options);
	}
}


}


#endif // BUILD_TESTS
