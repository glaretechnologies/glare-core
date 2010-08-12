/*=====================================================================
Vec4.cpp
--------
File created by ClassTemplate on Thu Mar 26 15:28:20 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "Vec4f.h"


#include "../utils/stringutils.h"
#include "../indigo/TestUtils.h"


const std::string Vec4f::toString() const
{
	return "(" + ::toString(x[0]) + "," + ::toString(x[1]) + "," + ::toString(x[2]) + "," + ::toString(x[3]) + ")";
}


#if (BUILD_TESTS)
void Vec4f::test()
{
	{
		testAssert(Vec4f(1, 2, 3, 4) == Vec4f(1, 2, 3, 4));
		testAssert(!(Vec4f(1, 2, 3, 4) == Vec4f(-1, 2, 3, 4)));

		testAssert(Vec4f(1, 2, 3, 0) == maskWToZero(Vec4f(1, 2, 3, 4)));
	}

	{
		const Vec4f a(1, 2, 3, 4);
		const Vec4f b(5, 6, 7, 8);

		testAssert(epsEqual(dot(a, b), 70.0f));
	}

	{
		const Vec4f a(1, 0, 0, 0);
		const Vec4f b(0, 1, 0, 0);

		const Vec4f res(crossProduct(a, b));
		testAssert(epsEqual(res, Vec4f(0, 0, 1, 0)));
	}

	{
		const Vec4f a(1, 2, 3, 4);

		testAssert(epsEqual(a.length(), std::sqrt(30.0f)));
		testAssert(epsEqual(a.length2(), 30.0f));

		testAssert(!a.isUnitLength());

		const Vec4f b(normalise(a));

		testAssert(b.isUnitLength());
		testAssert(epsEqual(b.length(), 1.0f));

		testAssert(epsEqual(b.x[0], 1.0f / std::sqrt(30.0f)));
		testAssert(epsEqual(b.x[1], 2.0f / std::sqrt(30.0f)));
		testAssert(epsEqual(b.x[2], 3.0f / std::sqrt(30.0f)));
		testAssert(epsEqual(b.x[3], 4.0f / std::sqrt(30.0f)));

	}

}
#endif
