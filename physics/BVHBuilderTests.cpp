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
#include "../indigo/StandardPrintOutput.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/Vector.h"
#include "../utils/MTwister.h"
#include "../utils/TaskManager.h"


#if BUILD_TESTS


namespace BVHBuilderTests
{


class BVHBuilderTestsCallBack : public BVHBuilderCallBacks
{
public:
	BVHBuilderTestsCallBack(int max_num_leaf_objects_) : num_nodes(0), num_interior_nodes(0), num_leaf_nodes(0), max_num_leaf_objects(max_num_leaf_objects_) {}

	virtual uint32 createNode()
	{
		return num_nodes++;
	}

	virtual int markAsInteriorNode(int node_index, int left_child_index, int right_child_index, const js::AABBox& left_aabb, const js::AABBox& right_aabb, int parent_index, bool is_left_child)
	{
		num_interior_nodes++;
		return node_index;
	}


	virtual void markAsLeafNode(int node_index, const std::vector<uint32>& objects, int begin, int end, int parent_index, bool is_left_child)
	{
		testAssert((end - begin) <= max_num_leaf_objects);
		num_leaf_nodes++;
	}

	int num_nodes;
	int num_interior_nodes;
	int num_leaf_nodes;
	
	int max_num_leaf_objects;
};


void test()
{
	conPrint("BVHBuilderTests::test()");

	StandardPrintOutput print_output;
	MTwister rng(1);
	Indigo::TaskManager task_manager(4);

	//==================== Test building a BVH with zero objects ====================
	{
		const int max_num_leaf_objects = 16;
		BVHBuilder builder(1, max_num_leaf_objects, 1.0f);
		BVHBuilderTestsCallBack callback(max_num_leaf_objects);
		builder.build(task_manager, 
			NULL, // aabbs
			0, // num objects
			print_output, 
			false, // verbose
			callback
		);

		testAssert(callback.num_nodes == 1);
		testAssert(callback.num_interior_nodes == 0);
		testAssert(callback.num_leaf_nodes == 1);
	}

	//==================== Test building a BVH with varying numbers of objects ====================
	for(int i=1; i<64; ++i)
	{
		js::Vector<js::AABBox, 16> aabbs(i);
		for(int z=0; z<i; ++z)
			aabbs[z] = js::AABBox(Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1), Vec4f(rng.unitRandom(), rng.unitRandom(), rng.unitRandom(), 1));

		const int max_num_leaf_objects = 16;
		BVHBuilder builder(1, max_num_leaf_objects, 1.0f);
		BVHBuilderTestsCallBack callback(max_num_leaf_objects);
		builder.build(task_manager,
			&aabbs[0], // aabbs
			i, // num objects
			print_output, 
			false, // verbose
			callback
		);

		testAssert(callback.num_nodes >= 1);
		testAssert(callback.num_leaf_nodes >= 1);
	}



	//==================== Test building a BVH with lots of objects with the same BVH ====================
	// This means that they can't get split as normal, but must still be split up to enforce the max_num_leaf_objects limit.
	for(int i=1; i<64; ++i)
	{
		js::Vector<js::AABBox, 16> aabbs(i);
		for(int z=0; z<i; ++z)
			aabbs[z] = js::AABBox(Vec4f(0, 0, 0, 1), Vec4f(1, 1, 1, 1)); // Use the same AABB for each object.

		const int max_num_leaf_objects = 16;
		BVHBuilder builder(1, max_num_leaf_objects, 1.0f);
		BVHBuilderTestsCallBack callback(max_num_leaf_objects);
		builder.build(task_manager,
			&aabbs[0], // aabbs
			i, // num objects
			print_output, 
			false, // verbose
			callback
		);

		testAssert(callback.num_nodes >= 1);
		testAssert(callback.num_leaf_nodes >= 1);
	}
}


}


#endif // BUILD_TESTS
