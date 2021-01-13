/*=====================================================================
Vec4.cpp
--------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#include "Vec4.h"


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include <complex>
typedef std::complex<float> Complex;


static inline bool epsEqual(Complex a, Complex b, double epsilon = NICKMATHS_EPSILON)
{
	// With fast-math (/fp:fast) enabled, comparisons are not checked for NAN compares (the 'unordered predicate').
	// So we need to do this explicitly ourselves.
	const auto fabs_diff_re = (a - b).real();
	const auto fabs_diff_im = (a - b).imag();
	if(isNAN(fabs_diff_re) || isNAN(fabs_diff_im))
		return false;
	return fabs_diff_re <= epsilon && fabs_diff_im <= epsilon;
}


template <class T>
inline static bool epsEqual(const Vec4<T>& a, const Vec4<T>& b, float eps = NICKMATHS_EPSILON)
{
	return ::epsEqual(a.x, b.x, eps) && ::epsEqual(a.y, b.y, eps) && ::epsEqual(a.z, b.z, eps) && ::epsEqual(a.w, b.w, eps);
}


void doVec4Tests()
{
	conPrint("doVec4Tests()");

	// Test 4-float constructor
	{
		Vec4<float> v(1, 2, 3, 4);
		testAssert(v[0] == 1);
		testAssert(v[1] == 2);
		testAssert(v[2] == 3);
		testAssert(v[3] == 4);
	}

	// Test scalar constructor
	{
		Vec4<float> v(3);
		testAssert(v[0] == 3);
		testAssert(v[1] == 3);
		testAssert(v[2] == 3);
		testAssert(v[3] == 3);
	}

	// Test copy constructor
	{
		Vec4<float> v;
		v.set(1, 2, 3, 4);
		Vec4<float> v2(v);
		testAssert(v2[0] == 1);
		testAssert(v2[1] == 2);
		testAssert(v2[2] == 3);
		testAssert(v2[3] == 4);
	}

	// Test set()
	{
		Vec4<float> v;
		v.set(1, 2, 3, 4);
		testAssert(v[0] == 1);
		testAssert(v[1] == 2);
		testAssert(v[2] == 3);
		testAssert(v[3] == 4);
	}

	// Test operator = 
	{
		Vec4<float> v;
		v.set(1, 2, 3, 4);
		Vec4<float> v2;
		v2 = v;
		testAssert(v2[0] == 1);
		testAssert(v2[1] == 2);
		testAssert(v2[2] == 3);
		testAssert(v2[3] == 4);
	}

	// Test operator []
	{
		Vec4<float> v(1, 2, 3, 4);
		testAssert(v[0] == 1);
		testAssert(v[1] == 2);
		testAssert(v[2] == 3);
		testAssert(v[3] == 4);
	}

	// Test operator +=
	{
		Vec4<float> a(1, 2, 3, 4);
		Vec4<float> b(5, 6, 7, 8);
		a += b;
		testAssert(epsEqual(a, Vec4<float>(6, 8, 10, 12)));
	}

	// Test operator -=
	{
		Vec4<float> a(10, 20, 30, 40);
		Vec4<float> b(5, 6, 7, 8);
		a -= b;
		testAssert(epsEqual(a, Vec4<float>(5, 14, 23, 32)));
	}

	// Test operator *=
	{
		Vec4<float> a(1, 2, 3, 4);
		a *= 2.f;
		testAssert(epsEqual(a, Vec4<float>(2, 4, 6, 8)));
	}


	// Test operator ==
	{
		testAssert(Vec4<float>(1, 2, 3, 4) == Vec4<float>(1, 2, 3, 4));

		testAssert(!(Vec4<float>(1, 2, 3, 4) == Vec4<float>(-1, 2, 3, 4)));
		testAssert(!(Vec4<float>(1, 2, 3, 4) == Vec4<float>(1, -2, 3, 4)));
		testAssert(!(Vec4<float>(1, 2, 3, 4) == Vec4<float>(1, 2, -3, 4)));
		testAssert(!(Vec4<float>(1, 2, 3, 4) == Vec4<float>(1, 2, 3, -4)));
		testAssert(!(Vec4<float>(1, 2, 3, 4) == Vec4<float>(-1, -2, -3, -4)));
	}

	// Test operator !=
	{
		testAssert(!(Vec4<float>(1, 2, 3, 4) != Vec4<float>(1, 2, 3, 4)));

		testAssert(Vec4<float>(1, 2, 3, 4) != Vec4<float>(-1, 2, 3, 4));
		testAssert(Vec4<float>(1, 2, 3, 4) != Vec4<float>(1, -2, 3, 4));
		testAssert(Vec4<float>(1, 2, 3, 4) != Vec4<float>(1, 2, -3, 4));
		testAssert(Vec4<float>(1, 2, 3, 4) != Vec4<float>(1, 2, 3, -4));
		testAssert(Vec4<float>(1, 2, 3, 4) != Vec4<float>(-1, -2, -3, -4));
	}

	// Test length(), length2(), normalise(), isUnitLength()
	{
		const Vec4<float> a(1, 2, 3, 4);

		testAssert(epsEqual(a.length(), std::sqrt(30.0f)));
		testAssert(epsEqual(a.length2(), 30.0f));

		testAssert(!a.isUnitLength());

		const Vec4<float> b(normalise(a));

		testAssert(b.isUnitLength());
		testAssert(epsEqual(b.length(), 1.0f));

		testAssert(epsEqual(b[0], 1.0f / std::sqrt(30.0f)));
		testAssert(epsEqual(b[1], 2.0f / std::sqrt(30.0f)));
		testAssert(epsEqual(b[2], 3.0f / std::sqrt(30.0f)));
		testAssert(epsEqual(b[3], 4.0f / std::sqrt(30.0f)));
	}

	// Test operator + 
	{
		const Vec4<float> a(1, 2, 3, 4);
		const Vec4<float> b(5, 6, 7, 8);

		testAssert(epsEqual(Vec4<float>(a + b), Vec4<float>(6, 8, 10, 12)));
	}

	// Test operator - 
	{
		const Vec4<float> a(10, 20, 30, 40);
		const Vec4<float> b(5, 6, 7, 8);

		testAssert(epsEqual(Vec4<float>(a + b), Vec4<float>(15, 26, 37, 48)));
	}

	// Test operator *
	{
		const Vec4<float> a(10, 20, 30, 40);

		testAssert(epsEqual(Vec4<float>(a * 2.0f), Vec4<float>(20, 40, 60, 80)));
	}

	// Test dot()
	{
		const Vec4<float> a(1, 2, 3, 4);
		const Vec4<float> b(5, 6, 7, 8);

		testAssert(epsEqual(dot(a, b), 70.0f));
	}

	// Test crossProduct
	{
		const Vec4<float> a(1, 0, 0, 0);
		const Vec4<float> b(0, 1, 0, 0);

		const Vec4<float> res = crossProduct(a, b);
		testAssert(epsEqual(res, Vec4<float>(0, 0, 1, 0)));
	}
	{
		const Vec4<float> a(1, 2, 3, 4);
		const Vec4<float> b(5, 6, 7, 8);

		const Vec4<float> res = crossProduct(a, b);
		testAssert(epsEqual(res, Vec4<float>(2*7 - 3*6, 3*5 - 1*7, 1*6 - 2*5, 0)));
		testAssert(res[3] == 0.f); // w component should be exactly 0
		testAssert(epsEqual(dot(res, a), 0.f));
		testAssert(epsEqual(dot(res, b), 0.f));
	}

	// Test removeComponentInDir
	{
		const Vec4<float> a(1, 0, 0, 0);

		const Vec4<float> b(1, 2, 3, 4);

		testAssert(epsEqual(removeComponentInDir(b, a), Vec4<float>(0, 2, 3, 4)));
	}

	// Test unary operator -
	{
		const Vec4<float> a(1, 2, 3, 4);

		testAssert(epsEqual(Vec4<float>(-1, -2, -3, -4), -a));
	}


	//========================= Test Vec4<Complex> ========================
	{
		Vec4<Complex> a(1, 2, 3, 4);
		Vec4<Complex> b(5, 6, 7, 8);

		Vec4<Complex> c = a + b;
		testAssert(epsEqual(c, Vec4<Complex>(6, 8, 10, 12)));

		testAssert(epsEqual(a - b, Vec4<Complex>(-4, -4, -4, -4)));
	}
	{
		Vec4<Complex> a(Complex(1, 0), Complex(2, 1), 3, 4);
		Vec4<Complex> b(5, 6, Complex(7, 2), 8);

		Vec4<Complex> c = a + b;
		testAssert(epsEqual(c, Vec4<Complex>(6, Complex(8, 1), Complex(10, 2), 12)));
		testAssert(epsEqual(a - b, Vec4<Complex>(-4, Complex(-4, 1), Complex(-4, -2), -4)));
	}
}


#endif // BUILD_TESTS
