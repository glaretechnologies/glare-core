/*=====================================================================
Vec4.cpp
--------
File created by ClassTemplate on Thu Mar 26 15:28:20 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "Vec4f.h"


#include "../utils/StringUtils.h"


const std::string Vec4f::toString() const
{
	//return "(" + ::toString(x[0]) + "," + ::toString(x[1]) + "," + ::toString(x[2]) + "," + ::toString(x[3]) + ")";
	return "(" + ::doubleToStringScientific(x[0], 7) + ", " + ::doubleToStringScientific(x[1], 7) + ", " + ::doubleToStringScientific(x[2], 7) + ", " + ::toString(x[3]) + ")";
}


const std::string Vec4f::toStringNSigFigs(int n) const
{
	return "(" + ::floatToStringNSigFigs(x[0], n) + ", " + ::floatToStringNSigFigs(x[1], n) + ", " + ::floatToStringNSigFigs(x[2], n) + ", " + ::floatToStringNSigFigs(x[3], n) + ")";
}


#if BUILD_TESTS


#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"
#include "../utils/CycleTimer.h"



static INDIGO_STRONG_INLINE float dot3Vec(const Vec4f& a, const Vec4f& b)
{
	Vec4f res;
	res.v = _mm_dp_ps(a.v, b.v, 0x71);
	return res.x[0];
}


static INDIGO_STRONG_INLINE float scalarDotProduct(const Vec4f& a, const Vec4f& b)
{
	return a[0]*b[0] + a[1]*b[1] + a[2]*b[2] + a[3]*b[3];
}


static inline float horizontalAdd(const __m128& v)
{
	// v =																   [d, c, b, a]
	const __m128 v2 = _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 3, 0, 1));    // [c, d, a, b]
	const __m128 v3 = _mm_add_ps(v, v2);                                // [m(c, d), m(c, d), m(a, b), m(a, b)]
	const __m128 v4 = _mm_shuffle_ps(v3, v3, _MM_SHUFFLE(0, 1, 2, 3));  // [m(a, b), m(a, b), [m(c, d), [m(c, d)]
	const __m128 v5 = _mm_add_ps(v4, v3);                               // [m(a, b, c, d), m(a, b, c, d), m(a, b, c, d), m(a, b, c, d)]

	SSE_ALIGN float x[4];
	_mm_store_ps(x, v5);
	return x[0];
}


void Vec4f::test()
{
	conPrint("Vec4f::test()");

	// Test 4-float constructor
	{
		Vec4f v(1, 2, 3, 4);
		testAssert(v.x[0] == 1);
		testAssert(v.x[1] == 2);
		testAssert(v.x[2] == 3);
		testAssert(v.x[3] == 4);
	}

	// Test constructor from __m128
	{
		Vec4f v;
		v.set(1, 2, 3, 4);
		Vec4f v2(v.v);
		testAssert(v2.x[0] == 1);
		testAssert(v2.x[1] == 2);
		testAssert(v2.x[2] == 3);
		testAssert(v2.x[3] == 4);
	}

	// Test scalar constructor
	{
		Vec4f v(3);
		testAssert(v.x[0] == 3);
		testAssert(v.x[1] == 3);
		testAssert(v.x[2] == 3);
		testAssert(v.x[3] == 3);
	}

	// Test copy constructor
	{
		Vec4f v;
		v.set(1, 2, 3, 4);
		Vec4f v2(v);
		testAssert(v2.x[0] == 1);
		testAssert(v2.x[1] == 2);
		testAssert(v2.x[2] == 3);
		testAssert(v2.x[3] == 4);
	}

	// Test set()
	{
		Vec4f v;
		v.set(1, 2, 3, 4);
		testAssert(v.x[0] == 1);
		testAssert(v.x[1] == 2);
		testAssert(v.x[2] == 3);
		testAssert(v.x[3] == 4);
	}

	// Test operator = 
	{
		Vec4f v;
		v.set(1, 2, 3, 4);
		Vec4f v2;
		v2 = v;
		testAssert(v2.x[0] == 1);
		testAssert(v2.x[1] == 2);
		testAssert(v2.x[2] == 3);
		testAssert(v2.x[3] == 4);
	}

	// Test operator []
	{
		Vec4f v(1, 2, 3, 4);
		testAssert(v[0] == 1);
		testAssert(v[1] == 2);
		testAssert(v[2] == 3);
		testAssert(v[3] == 4);
	}

	// Test operator +=
	{
		Vec4f a(1, 2, 3, 4);
		Vec4f b(5, 6, 7, 8);
		a += b;
		testAssert(epsEqual(a, Vec4f(6, 8, 10, 12)));
	}

	// Test operator -=
	{
		Vec4f a(10, 20, 30, 40);
		Vec4f b(5, 6, 7, 8);
		a -= b;
		testAssert(epsEqual(a, Vec4f(5, 14, 23, 32)));
	}

	// Test operator *=
	{
		Vec4f a(1, 2, 3, 4);
		a *= 2.f;
		testAssert(epsEqual(a, Vec4f(2, 4, 6, 8)));
	}


	// Test operator ==
	{
		testAssert(Vec4f(1, 2, 3, 4) == Vec4f(1, 2, 3, 4));
		testAssert(!(Vec4f(1, 2, 3, 4) == Vec4f(-1, 2, 3, 4)));

		testAssert(Vec4f(1, 2, 3, 0) == maskWToZero(Vec4f(1, 2, 3, 4)));
	}

	// Test length(), length2(), normalise(), isUnitLength()
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

	// Test operator + 
	{
		const Vec4f a(1, 2, 3, 4);
		const Vec4f b(5, 6, 7, 8);

		testAssert(epsEqual(Vec4f(a + b), Vec4f(6, 8, 10, 12)));
	}

	// Test operator - 
	{
		const Vec4f a(10, 20, 30, 40);
		const Vec4f b(5, 6, 7, 8);

		testAssert(epsEqual(Vec4f(a + b), Vec4f(15, 26, 37, 48)));
	}

	// Test operator *
	{
		const Vec4f a(10, 20, 30, 40);

		testAssert(epsEqual(Vec4f(a * 2.0f), Vec4f(20, 40, 60, 80)));
	}

	// Test dot()
	{
		const Vec4f a(1, 2, 3, 4);
		const Vec4f b(5, 6, 7, 8);

		testAssert(epsEqual(dot(a, b), 70.0f));
	}

	// Test crossProduct
	{
		const Vec4f a(1, 0, 0, 0);
		const Vec4f b(0, 1, 0, 0);

		const Vec4f res(crossProduct(a, b));
		testAssert(epsEqual(res, Vec4f(0, 0, 1, 0)));
	}

	// Test removeComponentInDir
	{
		const Vec4f a(1, 0, 0, 0);

		const Vec4f b(1, 2, 3, 4);

		testAssert(epsEqual(removeComponentInDir(b, a), Vec4f(0, 2, 3, 4)));
	}


	// Test unary operator -
	{
		const Vec4f a(1, 2, 3, 4);

		testAssert(epsEqual(Vec4f(-1, -2, -3, -4), -a));
	}


	// Test horizontalSum
	{
		const Vec4f a(1, 2, 3, 4);

		testAssert(epsEqual(horizontalSum(a), 10.f));
	}





	conPrint("================ Perf test dot() ================");

	{
		CycleTimer cycle_timer;

		const float N = 1000000;
		float sum = 0.f;

		float f;
		for( f=0; f<N; f += 1.f)
		{
			const Vec4f a(_mm_load_ps1(&f));

			sum += dot(a, a);
		}

		const uint64 cycles = cycle_timer.elapsed();
		conPrint("dot(): " + ::toString((float)cycles / N) + " cycles");
		TestUtils::silentPrint(::toString(sum));
	}

	{
		CycleTimer cycle_timer;

		const float N = 1000000;
		float sum = 0.f;

		float f;
		for( f=0; f<N; f += 1.f)
		{
			const Vec4f a(_mm_load_ps1(&f));

			sum += dot3Vec(a, a);
		}

		const uint64 cycles = cycle_timer.elapsed();
		conPrint("dot3Vec(): " + ::toString((float)cycles / N) + " cycles");
		TestUtils::silentPrint(::toString(sum));
	}

	{
		CycleTimer cycle_timer;

		const float N = 1000000;
		float sum = 0.f;

		float f;
		for( f=0; f<N; f += 1.f)
		{
			const Vec4f a(_mm_load_ps1(&f));

			sum += scalarDotProduct(a, a);
		}

		const uint64 cycles = cycle_timer.elapsed();
		conPrint("scalarDotProduct(): " + ::toString((float)cycles / N) + " cycles");
		TestUtils::silentPrint(::toString(sum));
	}

	{
		CycleTimer cycle_timer;

		const float N = 1000000;
		float sum = 0.f;

		float f;
		for( f=0; f<N; f += 1.f)
		{
			const Vec4f a(f);

			sum += horizontalSum(a);
		}

		const uint64 cycles = cycle_timer.elapsed();
		conPrint("horizontalSum(): " + ::toString((float)cycles / N) + " cycles");
		TestUtils::silentPrint(::toString(sum));
	}

	{
		CycleTimer cycle_timer;

		const float N = 1000000;
		float sum = 0.f;

		float f;
		for( f=0; f<N; f += 1.f)
		{
			const Vec4f a(f);

			sum += horizontalAdd(a.v);
		}

		const uint64 cycles = cycle_timer.elapsed();
		conPrint("horizontalAdd(): " + ::toString((float)cycles / N) + " cycles");
		TestUtils::silentPrint(::toString(sum));
	}
}


void doAssertIsUnitLength(const Vec4f& v)
{
	if(!v.isUnitLength())
	{
		conPrint("Assert failure: vector was not unit length. v=" + v.toString() + ", v.length()=" + ::toString(v.length()));
		assert(!"Assert failure: vector was not unit length");
	}
}


#endif // BUILD_TESTS
