#include "IntervalSet.h"


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../indigo/globals.h"


typedef Vec2<int> Vec2i;


void intervalSetTest()
{
	//================= lower() =================
	{
		IntervalSetInt32 i(1, 5);
		testAssert(i.lower() == 1);
		testAssert(i.upper() == 5);
	}

	{
		IntervalSetInt32 i(Vec2i(1, 5), Vec2i(7, 10));
		testAssert(i.lower() == 1);
		testAssert(i.upper() == 10);
	}

	//================= includesValue() =================
	{
		IntervalSetInt32 i(1, 5);
		testAssert(!i.includesValue(0));
		testAssert(i.includesValue(1));
		testAssert(i.includesValue(5));
		testAssert(!i.includesValue(6));
	}

	{
		IntervalSetInt32 i(Vec2i(1, 5), Vec2i(7, 10));
		testAssert(!i.includesValue(0));
		testAssert(i.includesValue(1));
		testAssert(i.includesValue(5));
		testAssert(!i.includesValue(6));
		testAssert(i.includesValue(7));
		testAssert(i.includesValue(10));
		testAssert(!i.includesValue(11));
	}

	//================= intervalWithHole() =================
	{
		IntervalSetInt32 i = intervalWithHole(5);
		testAssert(i == IntervalSetInt32(Vec2i(std::numeric_limits<int32>::min(), 4), Vec2i(6, std::numeric_limits<int32>::max())));
		testAssert(i.includesValue(4));
		testAssert(!i.includesValue(5));
		testAssert(i.includesValue(6));
	}

	// Test INT_MIN
	{
		IntervalSetInt32 i = intervalWithHole(std::numeric_limits<int32>::min());
		testAssert(i == IntervalSetInt32(std::numeric_limits<int32>::min() + 1, std::numeric_limits<int32>::max()));
		testAssert(!i.includesValue(std::numeric_limits<int32>::min()));
		testAssert(i.includesValue(std::numeric_limits<int32>::min() + 1));
	}

	// Test INT_MAX
	{
		IntervalSetInt32 i = intervalWithHole(std::numeric_limits<int32>::max());
		testAssert(i == IntervalSetInt32(std::numeric_limits<int32>::min(), std::numeric_limits<int32>::max() - 1));
		testAssert(i.includesValue(std::numeric_limits<int32>::max() - 1));
		testAssert(!i.includesValue(std::numeric_limits<int32>::max()));
	}

	//================= intervalSetIntersection() =================

	// Test empty intersection
	{
		IntervalSetInt32 a(1, 5);
		IntervalSetInt32 b(7, 10);

		IntervalSetInt32 i = intervalSetIntersection(a, b);

		testAssert(i == IntervalSetInt32::emptySet());
	}

	// Test non-empty intersection with one-number overlap
	{
		IntervalSetInt32 a(1, 5);
		IntervalSetInt32 b(5, 10);

		IntervalSetInt32 i = intervalSetIntersection(a, b);

		testAssert(i == IntervalSetInt32(5, 5));
	}

	// Test non-empty intersection
	{
		IntervalSetInt32 a(1, 5);
		IntervalSetInt32 b(3, 7);

		IntervalSetInt32 i = intervalSetIntersection(a, b);

		testAssert(i == IntervalSetInt32(3, 5));
	}

	{
		IntervalSetInt32 a(1, 3);
		IntervalSetInt32 b(2, 4);

		IntervalSetInt32 i = intervalSetIntersection(a, b);

		testAssert(i == IntervalSetInt32(2, 3));
	}

	{
		IntervalSetInt32 a(1, 6);
		IntervalSetInt32 b(5, 7);

		IntervalSetInt32 i = intervalSetIntersection(a, b);

		testAssert(i == IntervalSetInt32(5, 6));
	}

	// Test non-empty intersection with more complicated interval sets (multiple intervals in a)
	{
		IntervalSetInt32 a(Vec2i(1, 3), Vec2i(5, 8));
		IntervalSetInt32 b(0, 2);
		IntervalSetInt32 i = intervalSetIntersection(a, b);
		testAssert(i == IntervalSetInt32(1, 2));
	}
	{
		IntervalSetInt32 a(Vec2i(1, 3), Vec2i(5, 8));
		IntervalSetInt32 b(2, 4);
		IntervalSetInt32 i = intervalSetIntersection(a, b);
		testAssert(i == IntervalSetInt32(2, 3));
	}
	{
		IntervalSetInt32 a(Vec2i(1, 3), Vec2i(5, 8));
		IntervalSetInt32 b(2, 5);
		IntervalSetInt32 i = intervalSetIntersection(a, b);
		testAssert(i == IntervalSetInt32(Vec2i(2, 3), Vec2i(5, 5)));
	}
	{
		IntervalSetInt32 a(Vec2i(1, 3), Vec2i(5, 8));
		IntervalSetInt32 b(2, 6);
		IntervalSetInt32 i = intervalSetIntersection(a, b);
		testAssert(i == IntervalSetInt32(Vec2i(2, 3), Vec2i(5, 6)));
	}
	{
		IntervalSetInt32 a(Vec2i(1, 3), Vec2i(5, 8));
		IntervalSetInt32 b(2, 10);
		IntervalSetInt32 i = intervalSetIntersection(a, b);
		testAssert(i == IntervalSetInt32(Vec2i(2, 3), Vec2i(5, 8)));
	}
	{
		IntervalSetInt32 a(Vec2i(1, 3), Vec2i(5, 8));
		IntervalSetInt32 b(0, 10);
		IntervalSetInt32 i = intervalSetIntersection(a, b);
		testAssert(i == IntervalSetInt32(Vec2i(1, 3), Vec2i(5, 8)));
	}

	// Test non-empty intersection with more complicated interval sets (multiple intervals in b)
	{
		IntervalSetInt32 a(2, 6);
		IntervalSetInt32 b(Vec2i(1, 3), Vec2i(5, 8));
		IntervalSetInt32 i = intervalSetIntersection(a, b);
		testAssert(i == IntervalSetInt32(Vec2i(2, 3), Vec2i(5, 6)));
	}

	// Test non-empty intersection with more complicated interval sets (multiple intervals in both a and b)
	{
		IntervalSetInt32 a(Vec2i(2, 6), Vec2i(8, 10));
		IntervalSetInt32 b(Vec2i(1, 3), Vec2i(5, 9));
		IntervalSetInt32 i = intervalSetIntersection(a, b);
		testAssert(i == IntervalSetInt32(Vec2i(2, 3), Vec2i(5, 6), Vec2i(8, 9)));
	}
}


#endif // BUILD_TESTS
