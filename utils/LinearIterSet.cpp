/*=====================================================================
LinearIterSet.cpp
-----------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "LinearIterSet.h"


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "StringUtils.h"
#include "Timer.h"
#include "RefCounted.h"
#include "Reference.h"
#include "../maths/PCG32.h"
#include <set>


namespace glare
{
	
template <class T, class HashType>
void testLinearIterSetIteration(const LinearIterSet<T, HashType>& linear_iter_set, const std::set<T>& expected_set)
{
	testAssert(linear_iter_set.size() == expected_set.size());

	for(auto it = linear_iter_set.begin(); it != linear_iter_set.end(); ++it)
	{
		const T val = *it;
		testAssert(expected_set.count(val) > 0);
	}

	for(auto it = expected_set.begin(); it != expected_set.end(); ++it)
	{
		const T val = *it;
		testAssert(linear_iter_set.contains(val));
	}
}


void testLinearIterSet()
{
	conPrint("glare::LinearIterSet()");

	{
		LinearIterSet<int, std::hash<int>> set(-1);
		std::set<int> ref_set;

		testAssert(set.size() == 0);

		testAssert(set.insert(1));
		testAssert(set.insert(2));
		testAssert(set.insert(3));

		ref_set.insert(1);
		ref_set.insert(2);
		ref_set.insert(3);

		testAssert(set.size() == 3);
		testAssert(set.contains(1));
		testLinearIterSetIteration(set, ref_set);

		// Insert something already present
		testAssert(!set.insert(3));

		testAssert(set.size() == 3);
		testAssert(set.contains(1));
		testLinearIterSetIteration(set, ref_set);

		// Remove an element
		set.erase(2);
		ref_set.erase(2);

		testAssert(set.size() == 2);
		testAssert(set.contains(1));
		testAssert(set.contains(3));
		testLinearIterSetIteration(set, ref_set);

		set.erase(1);
		ref_set.erase(1);

		testAssert(set.size() == 1);
		testAssert(set.contains(3));
		testLinearIterSetIteration(set, ref_set);

		set.erase(3);
		ref_set.erase(3);

		testAssert(set.size() == 0);
		testLinearIterSetIteration(set, ref_set);
	}


	conPrint("glare::LinearIterSet() done.");
}


} // end namespace glare


#endif // BUILD_TESTS

