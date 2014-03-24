#include "IntervalSet.h"


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/timer.h"
#include "../utils/StringUtils.h"
#include "../indigo/globals.h"


typedef Vec2<int> Vec2i;


void intervalSetTest()
{
	//================= lower() =================
	{
		IntervalSetInt i(1, 5);
		testAssert(i.lower() == 1);
		testAssert(i.upper() == 5);
	}

	{
		IntervalSetInt i(Vec2i(1, 5), Vec2i(7, 10));
		testAssert(i.lower() == 1);
		testAssert(i.upper() == 10);
	}

	//================= includesValue() =================
	{
		IntervalSetInt i(1, 5);
		testAssert(!i.includesValue(0));
		testAssert(i.includesValue(1));
		testAssert(i.includesValue(5));
		testAssert(!i.includesValue(6));
	}

	{
		IntervalSetInt i(Vec2i(1, 5), Vec2i(7, 10));
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
		IntervalSetInt i = intervalWithHole(5);
		testAssert(i == IntervalSetInt(Vec2i(std::numeric_limits<int>::min(), 4), Vec2i(6, std::numeric_limits<int>::max())));
		testAssert(i.includesValue(4));
		testAssert(!i.includesValue(5));
		testAssert(i.includesValue(6));
	}

	// Test INT_MIN
	{
		IntervalSetInt i = intervalWithHole(std::numeric_limits<int>::min());
		testAssert(i == IntervalSetInt(std::numeric_limits<int>::min() + 1, std::numeric_limits<int>::max()));
		testAssert(!i.includesValue(std::numeric_limits<int>::min()));
		testAssert(i.includesValue(std::numeric_limits<int>::min() + 1));
	}

	// Test INT_MAX
	{
		IntervalSetInt i = intervalWithHole(std::numeric_limits<int>::max());
		testAssert(i == IntervalSetInt(std::numeric_limits<int>::min(), std::numeric_limits<int>::max() - 1));
		testAssert(i.includesValue(std::numeric_limits<int>::max() - 1));
		testAssert(!i.includesValue(std::numeric_limits<int>::max()));
	}

	//================= intervalSetIntersection() =================

	// Test empty intersection
	{
		IntervalSetInt a(1, 5);
		IntervalSetInt b(7, 10);

		IntervalSetInt i = intervalSetIntersection(a, b);

		testAssert(i == IntervalSetInt::emptySet());
	}

	// Test non-empty intersection with one-number overlap
	{
		IntervalSetInt a(1, 5);
		IntervalSetInt b(5, 10);

		IntervalSetInt i = intervalSetIntersection(a, b);

		testAssert(i == IntervalSetInt(5, 5));
	}

	// Test non-empty intersection
	{
		IntervalSetInt a(1, 5);
		IntervalSetInt b(3, 7);

		IntervalSetInt i = intervalSetIntersection(a, b);

		testAssert(i == IntervalSetInt(3, 5));
	}

	{
		IntervalSetInt a(1, 3);
		IntervalSetInt b(2, 4);

		IntervalSetInt i = intervalSetIntersection(a, b);

		testAssert(i == IntervalSetInt(2, 3));
	}

	{
		IntervalSetInt a(1, 6);
		IntervalSetInt b(5, 7);

		IntervalSetInt i = intervalSetIntersection(a, b);

		testAssert(i == IntervalSetInt(5, 6));
	}

	// Test non-empty intersection with more complicated interval sets (multiple intervals in a)
	{
		IntervalSetInt a(Vec2i(1, 3), Vec2i(5, 8));
		IntervalSetInt b(0, 2);
		IntervalSetInt i = intervalSetIntersection(a, b);
		testAssert(i == IntervalSetInt(1, 2));
	}
	{
		IntervalSetInt a(Vec2i(1, 3), Vec2i(5, 8));
		IntervalSetInt b(2, 4);
		IntervalSetInt i = intervalSetIntersection(a, b);
		testAssert(i == IntervalSetInt(2, 3));
	}
	{
		IntervalSetInt a(Vec2i(1, 3), Vec2i(5, 8));
		IntervalSetInt b(2, 5);
		IntervalSetInt i = intervalSetIntersection(a, b);
		testAssert(i == IntervalSetInt(Vec2i(2, 3), Vec2i(5, 5)));
	}
	{
		IntervalSetInt a(Vec2i(1, 3), Vec2i(5, 8));
		IntervalSetInt b(2, 6);
		IntervalSetInt i = intervalSetIntersection(a, b);
		testAssert(i == IntervalSetInt(Vec2i(2, 3), Vec2i(5, 6)));
	}
	{
		IntervalSetInt a(Vec2i(1, 3), Vec2i(5, 8));
		IntervalSetInt b(2, 10);
		IntervalSetInt i = intervalSetIntersection(a, b);
		testAssert(i == IntervalSetInt(Vec2i(2, 3), Vec2i(5, 8)));
	}
	{
		IntervalSetInt a(Vec2i(1, 3), Vec2i(5, 8));
		IntervalSetInt b(0, 10);
		IntervalSetInt i = intervalSetIntersection(a, b);
		testAssert(i == IntervalSetInt(Vec2i(1, 3), Vec2i(5, 8)));
	}

	// Test non-empty intersection with more complicated interval sets (multiple intervals in b)
	{
		IntervalSetInt a(2, 6);
		IntervalSetInt b(Vec2i(1, 3), Vec2i(5, 8));
		IntervalSetInt i = intervalSetIntersection(a, b);
		testAssert(i == IntervalSetInt(Vec2i(2, 3), Vec2i(5, 6)));
	}

	// Test non-empty intersection with more complicated interval sets (multiple intervals in both a and b)
	{
		IntervalSetInt a(Vec2i(2, 6), Vec2i(8, 10));
		IntervalSetInt b(Vec2i(1, 3), Vec2i(5, 9));
		IntervalSetInt i = intervalSetIntersection(a, b);
		testAssert(i == IntervalSetInt(Vec2i(2, 3), Vec2i(5, 6), Vec2i(8, 9)));
	}
}


#endif // BUILD_TESTS
