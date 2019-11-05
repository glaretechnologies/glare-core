/*=====================================================================
Vec4i.cpp
---------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#include "Vec4i.h"


#include "../utils/StringUtils.h"


const std::string Vec4i::toString() const
{
	return "(" + ::toString(x[0]) + "," + ::toString(x[1]) + "," + ::toString(x[2]) + "," + ::toString(x[3]) + ")";
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/ConPrint.h"


void Vec4i::test()
{
	conPrint("Vec4i::test()");

	//======================== Test constructor Vec4i(__m128i v) ==========================
	{
		const Vec4i a(Vec4i(1, 2, 3, 4).v);

		testAssert(a[0] == 1);
		testAssert(a[1] == 2);
		testAssert(a[2] == 3);
		testAssert(a[3] == 4);
	}

	//======================== Test constructor Vec4i(int x) ==========================
	{
		const Vec4i a(7);

		testAssert(a[0] == 7);
		testAssert(a[1] == 7);
		testAssert(a[2] == 7);
		testAssert(a[3] == 7);
	}

	//======================== Test operator[] (const) ==========================
	{
		const Vec4i a(1, 2, 3, 4);

		testAssert(a[0] == 1);
		testAssert(a[1] == 2);
		testAssert(a[2] == 3);
		testAssert(a[3] == 4);
	}

	//======================== Test operator[] (non-const) ==========================
	{
		Vec4i a(7);

		a[0] = 1;
		a[1] = 2;
		a[2] = 3;
		a[3] = 4;

		testAssert(a[0] == 1);
		testAssert(a[1] == 2);
		testAssert(a[2] == 3);
		testAssert(a[3] == 4);
	}

	//======================== Test operator += ==========================
	{
		Vec4i a(7);
		a += Vec4i(1, 2, 3, 4);
		testAssert(a == Vec4i(8, 9, 10, 11));
	}

	//======================== Test operator == ==========================
	{
		testAssert(Vec4i(1, 2, 3, 4) == Vec4i(1, 2, 3, 4));
		testAssert(Vec4i(-1, -2, -3, -4) == Vec4i(-1, -2, -3, -4));
		testAssert(Vec4i(1) == Vec4i(1));
		testAssert(Vec4i(-1) == Vec4i(-1));

		testAssert(!(Vec4i(1, 2, 3, 4) == Vec4i(100, 2, 3, 4)));
		testAssert(!(Vec4i(1, 2, 3, 4) == Vec4i(1, 200, 3, 4)));
		testAssert(!(Vec4i(1, 2, 3, 4) == Vec4i(1, 2, 300, 4)));
		testAssert(!(Vec4i(1, 2, 3, 4) == Vec4i(1, 2, 3, 400)));
	}

	//======================== Test operator != ==========================
	{
		testAssert(Vec4i(1, 2, 3, 4) != Vec4i(5, 6, 7, 8));
		
		testAssert(Vec4i(1, 2, 3, 4) != Vec4i(100, 2, 3, 4));
		testAssert(Vec4i(1, 2, 3, 4) != Vec4i(1, 200, 3, 4));
		testAssert(Vec4i(1, 2, 3, 4) != Vec4i(1, 2, 300, 4));
		testAssert(Vec4i(1, 2, 3, 4) != Vec4i(1, 2, 3, 400));

		testAssert(!(Vec4i(1, 2, 3, 4) != Vec4i(1, 2, 3, 4)));

		testAssert(!(Vec4i(-1) != Vec4i(-1)));
		testAssert(!(Vec4i(-1, -2, -3, -4) != Vec4i(-1, -2, -3, -4)));
	}

	//======================== Test loadVec4i ==========================
	{
		SSE_ALIGN int32 data[4] = { 1, 2, 3, 4 };
		testAssert(loadVec4i(data) == Vec4i(1, 2, 3, 4));
	}

	//======================== Test copyToAll ==========================
	{
		testAssert(copyToAll<0>(Vec4i(1, 2, 3, 4)) == Vec4i(1, 1, 1, 1));
		testAssert(copyToAll<1>(Vec4i(1, 2, 3, 4)) == Vec4i(2, 2, 2, 2));
		testAssert(copyToAll<2>(Vec4i(1, 2, 3, 4)) == Vec4i(3, 3, 3, 3));
		testAssert(copyToAll<3>(Vec4i(1, 2, 3, 4)) == Vec4i(4, 4, 4, 4));
	}

	//======================== Test elem ==========================
	{
		const Vec4i a(1, 2, 3, 4);

		testAssert(elem<0>(a) == 1);
		testAssert(elem<1>(a) == 2);
		testAssert(elem<2>(a) == 3);
		testAssert(elem<3>(a) == 4);
	}

	//======================== Test mulLo ==========================
	{
		testAssert(mulLo(Vec4i(1, 2, 3, 4), Vec4i(5, 6, 7, 8)) == Vec4i(5, 12, 21, 32));
	}

	//======================== operator + ==========================
	{
		testAssert(Vec4i(1, 2, 3, 4) + Vec4i(5, 6, 7, 8) == Vec4i(6, 8,10, 12));

		// Test that addition can be used on unsigned integers to give wraparound behaviour.
		SSE_ALIGN uint32 data[4] = { 1, 2000000000u, 3000000000u, 4 };
		Vec4i v = loadVec4i(data);
		Vec4i res = v + v;

		// (2000000000 + 2000000000) % 2^32 = 4000000000
		// 4000000000 - 2^31 = 1852516352
		// -2**31 + 1852516352 = 1852516352
		// (3000000000 + 3000000000) % 2^32 = 1705032704
		testAssert(res == Vec4i(2, -294967296, 1705032704, 8));
	}

	//======================== operator - ==========================
	{
		testAssert(Vec4i(1, 2, 3, 4) - Vec4i(5, 6, 7, -8) == Vec4i(-4, -4, -4, 12));
	}

	//======================== operator < ==========================
	{
		testAssert((Vec4i(1, 2, 3, 4) < Vec4i(5, -6, 7, -8)) == Vec4i(0xFFFFFFFF, 0, 0xFFFFFFFF, 0));
	}

}


#endif // BUILD_TESTS
