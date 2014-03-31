/*=====================================================================
ImmutableVector.cpp
-------------------
Copyright Glare Technologies Limited 2014 -
Generated at 2014-03-29 12:47:28 +0000
=====================================================================*/
#include "ImmutableVector.h"


#if BUILD_TESTS


#include "../indigo/TestUtils.h"


static void testNPushBackMutateInPlaces(size_t n)
{
	ImmutableVector<int, 4> v;

	// Push back the items
	for(size_t i=0; i<n; ++i)
	{
		v.pushBackMutateInPlace((int)i);
	}

	// Check the [] operator is correct for all items added
	for(size_t i=0; i<n; ++i)
	{
		testAssert(v[i] == i);
	}
}


static void testUpdate(size_t vec_size, size_t index_to_update)
{
	ImmutableVector<int, 4> v;

	// Push back the items
	for(size_t i=0; i<vec_size; ++i)
		v.pushBackMutateInPlace((int)i);

	// Check the [] operator is correct for all items added
	for(size_t i=0; i<vec_size; ++i)
		testAssert(v[i] == i);

	// Get an updated copy
	ImmutableVector<int, 4> updated_copy;
	v.update(index_to_update, 666, updated_copy);

	testAssert(updated_copy.size() == vec_size);
	//testAssert(updated_copy.depth == v.depth);

	// Check original is still fine
	for(size_t i=0; i<vec_size; ++i)
		testAssert(v[i] == i);

	// Check copy
	for(size_t i=0; i<vec_size; ++i)
		if(i == index_to_update)
			testAssert(updated_copy[i] == 666);
		else
			testAssert(updated_copy[i] == i);
}


static void testNPushBacksWithCopy(size_t n)
{
	ImmutableVector<int, 4> v;

	for(size_t i=0; i<n; ++i)
	{
		// Check the original vector is still valid.
		for(size_t z=0; z<i; ++z)
			testAssert(v[z] == z);

		ImmutableVector<int, 4> updated_copy;
		v.push_back((int)i, updated_copy);

		// Check the original vector is still valid.
		testAssert(v.size() == i);
		for(size_t z=0; z<i; ++z)
			testAssert(v[z] == z);

		// Check the updated_copy valid.
		testAssert(updated_copy.size() == i + 1);
		for(size_t z=0; z<=i; ++z)
			testAssert(updated_copy[z] == z);

		v = updated_copy;
	}

	
	// Check the [] operator is correct for all items added
	for(size_t i=0; i<n; ++i)
		testAssert(v[i] == i);
}


void testImmutableVector()
{
/*	{
		ImmutableVector<int, 4> v;
		for(size_t i=0; i<4; ++i)
			v.pushBackMutateInPlace((int)i);

		ImmutableVector<int, 4> updated_copy;
		v.push_back(4, updated_copy);

		for(size_t i=0; i<4; ++i)
			testAssert(v[i] == i);

		for(size_t i=0; i<4; ++i)
			testAssert(updated_copy[i] == i);

		testAssert(updated_copy[4] == 4);
	}*/

	testNPushBacksWithCopy(4);
	testNPushBacksWithCopy(5);
	testNPushBacksWithCopy(512);

	testUpdate(3, 2);
	testUpdate(5, 2);
	testUpdate(7, 5);
	testUpdate(127, 5);
	testUpdate(128, 5);

	// 64
	{
		ImmutableVector<int, 4> v;

		// Push back the items
		for(size_t i=0; i<63; ++i)
			v.pushBackMutateInPlace((int)i);

		v.pushBackMutateInPlace(64);

		//testAssert(v.depth == 4);
		/*testAssert(v.root.num == 2);
		testAssert(v.root.children[0]->num == 4);
		testAssert(v.root.children[1]->num == 1);*/


		// Check the [] operator is correct for all items added
		for(size_t i=0; i<16; ++i)
			testAssert(v[i] == i);
	}

	// 16
	{
		ImmutableVector<int, 4> v;

		// Push back the items
		for(size_t i=0; i<16; ++i)
			v.pushBackMutateInPlace((int)i);

		//testAssert(v.depth == 3);
		//testAssert(v.root.num == 2);
		//testAssert(v.root.children[0]->num == 4);
		//testAssert(v.root.children[1]->num == 1);


		// Check the [] operator is correct for all items added
		for(size_t i=0; i<16; ++i)
			testAssert(v[i] == i);
	}

	// 20
	{
		ImmutableVector<int, 4> v;

		// Push back the items
		for(size_t i=0; i<19; ++i)
			v.pushBackMutateInPlace((int)i);
		v.pushBackMutateInPlace(19);

		/*testAssert(v.depth == 3);
		testAssert(v.root.num == 2);
		testAssert(v.root.children[0]->num == 4);
		testAssert(v.root.children[1]->num == 2);*/


		// Check the [] operator is correct for all items added
		for(size_t i=0; i<20; ++i)
			testAssert(v[i] == i);
	}

	// 32
	{
		ImmutableVector<int, 4> v;

		// Push back the items
		for(size_t i=0; i<31; ++i)
			v.pushBackMutateInPlace((int)i);
		v.pushBackMutateInPlace(31);

		/*testAssert(v.depth == 3);
		testAssert(v.root.num == 3);
		testAssert(v.root.children[0]->num == 4);
		testAssert(v.root.children[1]->num == 4);
		testAssert(v.root.children[2]->num == 1);*/


		// Check the [] operator is correct for all items added
		for(size_t i=0; i<32; ++i)
			testAssert(v[i] == i);
	}

	{
		ImmutableVector<int, 4> v;

		// Push back the items
		for(size_t i=0; i<31; ++i)
		{
			v.pushBackMutateInPlace((int)i);
		}

		v.pushBackMutateInPlace(31);

		// Check the [] operator is correct for all items added
		for(size_t i=0; i<32; ++i)
		{
			testAssert(v[i] == i);
		}
	}


	// Test with push backing >= 4 items, will cause rebalance
	{
		ImmutableVector<int, 4> v;

		v.pushBackMutateInPlace(0);
		v.pushBackMutateInPlace(1);
		v.pushBackMutateInPlace(2);
		v.pushBackMutateInPlace(3);
		v.pushBackMutateInPlace(4);
		v.pushBackMutateInPlace(5);
		v.pushBackMutateInPlace(6);
		v.pushBackMutateInPlace(7);
		v.pushBackMutateInPlace(8);
		v.pushBackMutateInPlace(9);
		v.pushBackMutateInPlace(10);
		v.pushBackMutateInPlace(11);
		v.pushBackMutateInPlace(12);
		v.pushBackMutateInPlace(13);
		v.pushBackMutateInPlace(14);
		v.pushBackMutateInPlace(15);
		v.pushBackMutateInPlace(16);

		testAssert(v[0] == 0);
		testAssert(v[1] == 1);
		testAssert(v[2] == 2);
		testAssert(v[3] == 3);
		testAssert(v[4] == 4);
		testAssert(v[5] == 5);
		testAssert(v[6] == 6);
		testAssert(v[7] == 7);
		testAssert(v[8] == 8);
		testAssert(v[9] == 9);
		testAssert(v[10] == 10);
		testAssert(v[11] == 11);
		testAssert(v[12] == 12);
		testAssert(v[13] == 13);
		testAssert(v[14] == 14);
		testAssert(v[15] == 15);
		testAssert(v[16] == 16);
	}

	// Test with push backing >= 4 items, will cause rebalance
	{
		ImmutableVector<int, 4> v;

		v.pushBackMutateInPlace(0);
		v.pushBackMutateInPlace(1);
		v.pushBackMutateInPlace(2);
		v.pushBackMutateInPlace(3);
		v.pushBackMutateInPlace(4);
		v.pushBackMutateInPlace(5);
		v.pushBackMutateInPlace(6);
		v.pushBackMutateInPlace(7);

		testAssert(v[0] == 0);
		testAssert(v[1] == 1);
		testAssert(v[2] == 2);
		testAssert(v[3] == 3);
		testAssert(v[4] == 4);
		testAssert(v[5] == 5);
		testAssert(v[6] == 6);
		testAssert(v[7] == 7);
	}

	/*testNPushBacks(0);
	testNPushBacks(1);
	testNPushBacks(3);
	testNPushBacks(4);
	testNPushBacks(5);

	testNPushBacks(7);*/
	testNPushBackMutateInPlaces(8);
	testNPushBackMutateInPlaces(9);
	testNPushBackMutateInPlaces(10);
	testNPushBackMutateInPlaces(11);
	testNPushBackMutateInPlaces(12);
	testNPushBackMutateInPlaces(13);
	testNPushBackMutateInPlaces(15);
	testNPushBackMutateInPlaces(16);
	testNPushBackMutateInPlaces(64);
	testNPushBackMutateInPlaces(128);
	testNPushBackMutateInPlaces(256);
	//testNPushBacks(1000000);
}


#endif // BUILD_TESTS
