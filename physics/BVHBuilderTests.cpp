/*=====================================================================
BVHBuilderTests.cpp
-------------------
Copyright Glare Technologies Limited 2015 -
Generated at 2015-09-28 16:25:21 +0100
=====================================================================*/
#include "BVHBuilderTests.h"


#include "BVHBuilder.h"
#include "jscol_aabbox.h"
#include "../indigo/TestUtils.h"
#include "../utils/StandardPrintOutput.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Vector.h"
#include "../utils/MTwister.h"
#include "../utils/TaskManager.h"
#include "../utils/Timer.h"


#if BUILD_TESTS


namespace BVHBuilderTests
{


static void testResultsValid(BVHBuilder& builder, const js::Vector<ResultNode, 64>& result_nodes, int num_objects)
{
	// Test that the resulting object indices are a permutation of the original indices.
	std::vector<bool> seen(num_objects, false);
	for(int z=0; z<num_objects; ++z)
	{
		const uint32 ob_i = builder.getResultObjectIndices()[z];
		testAssert(!seen[ob_i]);
		seen[ob_i] = true;
	}
	
	// Test that the result nodes object ranges cover all leaf objects, e.g. test that each object is in a leaf node.
	std::vector<bool> ob_in_leaf(num_objects, false);
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
			testAssert(node.right <= num_objects);

			for(int z = node.left; z < node.right; ++z)
			{
				testAssert(!ob_in_leaf[z]);
				ob_in_leaf[z] = true;
			}
		}
	}

	for(int z=0; z<num_objects; ++z)
		testAssert(ob_in_leaf[z]);
}


static void testBVHBuilderWithNRandomObjects(MTwister& rng, Indigo::TaskManager& task_manager, int num_objects)
{
	StandardPrintOutput print_output;

	js::Vector<js::AABBox, 16> aabbs(num_objects);
	for(int z=0; z<num_objects; ++z)
	{
		//aabbs[z] = js::AABBox(Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1), Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1));
		const Vec4f p(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1);
		aabbs[z] = js::AABBox(p, p + Vec4f(0.01f, 0.01f, 0.01f, 0));
	}

	const int max_num_leaf_objects = 16;
	BVHBuilder builder(1, max_num_leaf_objects, 4.0f);

	// Set these multi-threading thresholds lower than normal, in order to flush out any multi-threading bugs.
	builder.axis_parallel_num_ob_threshold = 32;
	builder.new_task_num_ob_threshold = 32; 

	js::Vector<ResultNode, 64> result_nodes;
	builder.build(task_manager,
		aabbs.data(), // aabbs
		num_objects, // num objects
		print_output, 
		false, // verbose
		result_nodes
	);

	testResultsValid(builder, result_nodes, num_objects);
}


void test()
{
	conPrint("BVHBuilderTests::test()");

	MTwister rng(1);
	Indigo::TaskManager task_manager(8);
	StandardPrintOutput print_output;

	
	/*{
		int num_objects = 8;
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		for(int z=0; z<num_objects; ++z)
			aabbs[z] = js::AABBox(Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1), Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1));

		js::Vector<ResultNode, 64> result_nodes;

		const int max_num_leaf_objects = 16;
		BVHBuilder builder(1, max_num_leaf_objects, 1.0f);
		BVHBuilderTestsCallBack callback(max_num_leaf_objects);
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			false, // verbose
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

		const int max_num_leaf_objects = 16;
		BVHBuilder builder(1, max_num_leaf_objects, 100.0f);

		// Set these multi-threading thresholds lower than normal, in order to flush out any multi-threading bugs.
		builder.axis_parallel_num_ob_threshold = 30002;
		builder.new_task_num_ob_threshold = 30002; 

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			false, // verbose
			result_nodes
		);

		testResultsValid(builder, result_nodes, num_objects);
	}*/



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
			false, // verbose
			result_nodes
		);

		testResultsValid(builder, result_nodes, num_objects);
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
			false, // verbose
			result_nodes
		);

		testResultsValid(builder, result_nodes, num_objects);
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
			false, // verbose
			result_nodes
		);

		testResultsValid(builder, result_nodes, num_objects);
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

		const int max_num_leaf_objects = 16;
		BVHBuilder builder(1, max_num_leaf_objects, 100.0f);

		// Set these multi-threading thresholds lower than normal, in order to flush out any multi-threading bugs.
		builder.axis_parallel_num_ob_threshold = 32;
		builder.new_task_num_ob_threshold = 32; 

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			false, // verbose
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

		const int max_num_leaf_objects = 16;
		BVHBuilder builder(1, max_num_leaf_objects, 100.0f);

		// Set these multi-threading thresholds lower than normal, in order to flush out any multi-threading bugs.
		builder.axis_parallel_num_ob_threshold = 32;
		builder.new_task_num_ob_threshold = 32; 

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			false, // verbose
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

		const int max_num_leaf_objects = 16;
		BVHBuilder builder(1, max_num_leaf_objects, 100.0f);

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			false, // verbose
			result_nodes
		);

		testResultsValid(builder, result_nodes, num_objects);
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

		const int max_num_leaf_objects = 16;
		BVHBuilder builder(1, max_num_leaf_objects, 100.0f);

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			false, // verbose
			result_nodes
		);

		testResultsValid(builder, result_nodes, num_objects);
	}



	
	//==================== Test building a BVH with varying numbers of objects (including zero objects) ====================
	for(int num_objects=0; num_objects<64; ++num_objects)
	{
		testBVHBuilderWithNRandomObjects(rng, task_manager, num_objects);
	}

	for(int num_objects=64; num_objects<=4096; num_objects *= 2)
	{
		testBVHBuilderWithNRandomObjects(rng, task_manager, num_objects);
	}


	//==================== Do a stress test with a reasonably large amount of objects ====================
	{
		conPrint("StressTest...");
		testBVHBuilderWithNRandomObjects(rng, task_manager, 
			10000
		);
		conPrint("Done.");
	}

	
	//==================== Test building a BVH with lots of objects with the same BVH ====================
	// This means that they can't get split as normal, but must still be split up to enforce the max_num_leaf_objects limit.
	for(int num_objects=1; num_objects<64; ++num_objects)
	{
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		for(int z=0; z<num_objects; ++z)
			aabbs[z] = js::AABBox(Vec4f(0, 0, 0, 1), Vec4f(1, 1, 1, 1)); // Use the same AABB for each object.

		const int max_num_leaf_objects = 16;
		BVHBuilder builder(1, max_num_leaf_objects, 4.0f);

		// Set these multi-threading thresholds lower than normal, in order to flush out any multi-threading bugs.
		builder.axis_parallel_num_ob_threshold = 32;
		builder.new_task_num_ob_threshold = 32; 

		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			false, // verbose
			result_nodes
		);

		testResultsValid(builder, result_nodes, num_objects);
	}

	
	const bool DO_PERF_TESTS = false;
	if(DO_PERF_TESTS)
	{
		conPrint("------------- perf test --------------");

		//==================== Test building speed with lots of objects ====================
		const int num_objects = 5000000;
		
		js::Vector<js::AABBox, 16> aabbs(num_objects);
		for(int z=0; z<num_objects; ++z)
		{
			const Vec4f p(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1);
			aabbs[z] = js::AABBox(p, p + Vec4f(0.01f, 0.01f, 0.01f, 0));
		}


		Timer timer;

		const int max_num_leaf_objects = 16;
		const float intersection_cost = 10.f;
		BVHBuilder builder(1, max_num_leaf_objects, intersection_cost);
		js::Vector<ResultNode, 64> result_nodes;
		builder.build(task_manager,
			aabbs.data(), // aabbs
			num_objects, // num objects
			print_output, 
			false, // verbose
			result_nodes
		);

		conPrint("BVH building for " + toString(num_objects) + " objects took " + timer.elapsedString());

		testResultsValid(builder, result_nodes, num_objects);
	}
}


}


#endif // BUILD_TESTS
