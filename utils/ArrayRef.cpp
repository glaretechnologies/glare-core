/*=====================================================================
ArrayRef.cpp
------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "ArrayRef.h"


void checkedArrayRefMemcpy(MutableArrayRef<uint8> dest, size_t dest_start_index, ArrayRef<uint8> src, size_t src_start_index, size_t size_B)
{
	if(size_B > 0)
	{
		runtimeCheck(
			!Maths::unsignedIntAdditionWraps(dest_start_index, size_B) &&
			((dest_start_index + size_B) <= dest.size()) &&
			!Maths::unsignedIntAdditionWraps(src_start_index, size_B) &&
			((src_start_index + size_B) <= src.size()) &&
			areDisjoint(dest.getSlice(dest_start_index, size_B), src.getSlice(src_start_index, size_B))
		);

		std::memcpy(dest.data() + dest_start_index, src.data() + src_start_index, size_B);
	}
}


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "CycleTimer.h"
#include "Timer.h"
#include "StringUtils.h"


void doArrayRefTests()
{
	conPrint("doArrayRefTests()");

	{
		std::vector<int> v({0, 1, 2, 3, 4, 5});

		ArrayRef<int> a(v);
		testAssert(a.data() == v.data());
		testAssert(a.size() == v.size());

		for(size_t i=0; i<v.size(); ++i)
			testAssert(a[i] == v[i]);
	}

	//-------------------------- Test slice --------------------------
	{
		const std::vector<int> v({0, 1, 2, 3, 4, 5});
		ArrayRef<int> a(v);
		ArrayRef<int> b = a.getSlice(0, 6);
		testAssert(a.data() == b.data() && a.size() == b.size());

		ArrayRef<int> c = a.getSlice(1, 5);
		testAssert(c.data() == &a[1] && c.size() == 5);

		ArrayRef<int> zeroslice = a.getSlice(1, 0);
		testAssert(zeroslice.size() == 0);

		ArrayRef<int> zeroslice2 = a.getSlice(6, 0);
		testAssert(zeroslice2.size() == 0);
	}

	//-------------------------- Test getSliceChecked --------------------------
	{
		const std::vector<int> v({0, 1, 2, 3, 4, 5});
		ArrayRef<int> a(v);
		ArrayRef<int> b = a.getSliceChecked(0, 6);
		testAssert(a.data() == b.data() && a.size() == b.size());

		ArrayRef<int> c = a.getSliceChecked(1, 5);
		testAssert(c.data() == &a[1] && c.size() == 5);

		ArrayRef<int> zeroslice = a.getSliceChecked(1, 0);
		testAssert(zeroslice.size() == 0);

		ArrayRef<int> zeroslice2 = a.getSliceChecked(6, 0);
		testAssert(zeroslice2.size() == 0);

		// Triggers asserts (runtime check fails):
	//	a.getSliceChecked(0, 7);
	//	a.getSliceChecked(1, 6);
	//	a.getSliceChecked(7, 0);
	//	a.getSliceChecked(1, std::numeric_limits<size_t>::max()); // Runtime check should handle overflow and wraparound of size_t addition.
	}

	//-------------------------- Test areDisjoint --------------------------
	{
		const std::vector<int> v({0, 1, 2, 3, 4, 5});
		ArrayRef<int> a(v);

		const std::vector<int> v2({0, 1, 2, 3, 4, 5});
		ArrayRef<int> b(v2);

		testAssert(areDisjoint(a, b));
		testAssert(areDisjoint(b, a));

		testAssert(!areDisjoint(a, a));

		testAssert( areDisjoint(a.getSlice(0, 1), a.getSlice(1, 1)));
		testAssert( areDisjoint(a.getSlice(0, 2), a.getSlice(2, 2)));
		testAssert(!areDisjoint(a.getSlice(0, 3), a.getSlice(2, 2)));
		testAssert( areDisjoint(a.getSlice(2, 2), a.getSlice(0, 2)));
		testAssert(!areDisjoint(a.getSlice(2, 2), a.getSlice(0, 3)));
		testAssert(!areDisjoint(a.getSlice(2, 2), a.getSlice(1, 2)));
	}

	conPrint("doArrayRefTests() done");
}

#endif // BUILD_TESTS
