/*=====================================================================
Matrix4.h
---------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#include "Matrix4.h"


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
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


void doMatrix4Tests()
{
	conPrint("doMatrix4Tests()");

	//========================= Test Matrix4<float> ========================
	{
		const Matrix4<float> m = Matrix4<float>::identity();

		const Vec4<float> v(1, 2, 3, 4);
		const Vec4<float> res = m * v;
		testAssert(epsEqual(v, res));
	}

	{
		const float data[16] = { 1, 0, 2, 0,    0, 1, 0, 0,     0, 0, 1, 0,    10, 0, 0, 1 };
		const Matrix4<float> m = Matrix4<float>(data);

		const Vec4<float> v(1, 2, 3, 4);
		const Vec4<float> res = m * v;
		testAssert(epsEqual(res, Vec4<float>(7, 2, 3, 14)));
	}

	//========================= Test Matrix4<Complex> ========================
	{
		const Matrix4<Complex> m = Matrix4<Complex>::identity();

		const Vec4<Complex> v(1, 2, 3, 4);
		const Vec4<Complex> res = m * v;
		testAssert(epsEqual(v, res));
	}

	{
		const Complex data[16] = { 1, 0, 2, 0,    0, Complex(1, 2), 0, 0,     0, 0, 1, 0,    10, 0, 0, 1 };
		const Matrix4<Complex> m = Matrix4<Complex>(data);

		const Vec4<Complex> v(1, 2, 3, 4);
		const Vec4<Complex> res = m * v;
		testAssert(epsEqual(res, Vec4<Complex>(7, Complex(2, 4), 3, 14)));
	}
}


#endif // BUILD_TESTS
