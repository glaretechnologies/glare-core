/*=====================================================================
mathstypes.cpp
--------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "mathstypes.h"


#include "Matrix2.h"
#include "matrix3.h"
#include "Quat.h"
#include "../utils/TestUtils.h"
#include "../indigo/globals.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "../utils/CycleTimer.h"
#include "../utils/Platform.h"
#include "../maths/PCG32.h"
#include <vector>
#include <limits>


#if BUILD_TESTS


#if !defined(__APPLE__) // Don't run these speed tests on Mac, as CycleTimer crashes on Mac.


inline static float fastPosFract(float x)
{
	return x - (int)x;
}


inline static float fastFract(float x)
{
	if(x < 0)
	{
		// (int)-1.3 = -1
		// -1.3 - (int)-1.3 = -1.3 - -1 = -0.3
		// 1.f - (1.3 - (int)1.3) = 1 + -0.3 = 0.7
		return 1.f + (x - (int)x);
	}
	else
		return (x - (int)x);
}


inline static float modfFract(float x)
{
	if(x < 0)
	{
		float intpart; // not used
		return 1.f + std::modf(x, &intpart);
		
	}
	else
	{	
		float intpart; // not used
		return std::modf(x, &intpart);
	}
}


static GLARE_STRONG_INLINE float sqrtSSE(float x)
{
	return _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(x)));
}


#endif // !defined(__APPLE__)


template <class T>
inline T pow4Fast(T x)
{
	T x2 = x*x;
	return x2 * x2;
}


template <class T>
inline T pow8(T x)
{
	return x*x * x*x * x*x * x*x;
}


template <class T>
inline T pow8Fast(T x)
{
	T x2 = x*x;
	T x4 = x2*x2;
	return x4*x4;
}


template <class T>
inline T powOneOverEight(T x)
{
	T xOneOver2 = std::sqrt(x);
	T xOneOver4 = std::sqrt(xOneOver2);
	return std::sqrt(xOneOver4);
}


static inline bool isNegativeZero(float x)
{
	return x == 0.f && ((bitCast<uint32>(x) & 0x80000000) != 0);
}


static inline bool isNegativeZero(double x)
{
	return x == 0.0 && ((bitCast<uint64>(x) & 0x8000000000000000ULL) != 0);
}


void Maths::test()
{
	conPrint("Maths::test()");

	//=============================== unsignedIntAdditionWraps ================================
	testAssert(!unsignedIntAdditionWraps(0u, 0u));
	testAssert(!unsignedIntAdditionWraps(1u, 2u));
	testAssert(!unsignedIntAdditionWraps(4294967295u, 0u));
	testAssert(!unsignedIntAdditionWraps(4294967294u, 1u));
	testAssert(unsignedIntAdditionWraps(4294967295u, 1u));
	testAssert(unsignedIntAdditionWraps(4294967295u, 2u));
	testAssert(unsignedIntAdditionWraps(1u, 4294967295u));
	testAssert(unsignedIntAdditionWraps(2u, 4294967295u));
	testAssert(unsignedIntAdditionWraps(4294967295u, 4294967295u));
	testAssert(!unsignedIntAdditionWraps(2147483648u, 2147483647u)); // 2^31 + 2^31-1 = 2^32-1
	testAssert(unsignedIntAdditionWraps(2147483648u, 2147483648u)); // 2^31 + 2^31 = 2^32

	// max uint64 val = 2^64 - 1 = 18446744073709551615
	testAssert(!unsignedIntAdditionWraps(0ull, 0ull));
	testAssert(!unsignedIntAdditionWraps(1ull, 2ull));
	testAssert(!unsignedIntAdditionWraps(18446744073709551615ull, 0ull));
	testAssert(!unsignedIntAdditionWraps(18446744073709551614ull, 1ull));
	testAssert(unsignedIntAdditionWraps(18446744073709551615ull, 1ull));
	testAssert(unsignedIntAdditionWraps(18446744073709551615ull, 2ull));
	testAssert(unsignedIntAdditionWraps(1ull, 18446744073709551615ull));
	testAssert(unsignedIntAdditionWraps(2ull, 18446744073709551615ull));
	testAssert(unsignedIntAdditionWraps(18446744073709551615ull , 18446744073709551615ull));


	//=============================== roundToNextHighestPowerOf2() =========================================
	testAssert(roundToNextHighestPowerOf2(0) == 0); // This is expected.
	testAssert(roundToNextHighestPowerOf2(1) == 1);
	testAssert(roundToNextHighestPowerOf2(2) == 2);
	testAssert(roundToNextHighestPowerOf2(3) == 4);
	testAssert(roundToNextHighestPowerOf2(4) == 4);
	testAssert(roundToNextHighestPowerOf2(5) == 8);
	testAssert(roundToNextHighestPowerOf2(6) == 8);
	testAssert(roundToNextHighestPowerOf2(7) == 8);
	testAssert(roundToNextHighestPowerOf2(8) == 8);
	testAssert(roundToNextHighestPowerOf2(9) == 16);
	testAssert(roundToNextHighestPowerOf2(10) == 16);

	// Test around 2^32 = 4294967296
	testAssert(roundToNextHighestPowerOf2(4294967295ull) == 4294967296ull);
	testAssert(roundToNextHighestPowerOf2(4294967296ull) == 4294967296ull); 
	testAssert(roundToNextHighestPowerOf2(4294967297ull) == 8589934592ull);

	// Test around 2^63 = 9223372036854775808
	testAssert(roundToNextHighestPowerOf2(9223372036854775807ull) == 9223372036854775808ull);
	testAssert(roundToNextHighestPowerOf2(9223372036854775808ull) == 9223372036854775808ull);


	//======================================= uInt32ToUnitFloatScale() ========================================
	{
		const float x = (float)4294967295u * Maths::uInt32ToUnitFloatScale();
		// NOTE: 0.99999994f is the largest float value below 1.0f.  See http://www.h-schmidt.net/FloatConverter/IEEE754.html
		testAssert(x == 0.99999994f);
	}


	//======================================== sqrt2Pi(), recipSqrt2Pi() ========================================
	testAssert(epsEqual(sqrt2Pi<float>(), std::sqrt(2 * pi<float>())));
	testAssert(epsEqual(recipSqrt2Pi<float>(), 1 / std::sqrt(2 * pi<float>())));


	//======================================== logBase2() ========================================
	testAssert(epsEqual(logBase2(0.5), -1.0));
	testAssert(epsEqual(logBase2(1.0), 0.0));
	testAssert(epsEqual(logBase2(2.0), 1.0));
	testAssert(epsEqual(logBase2(4.0), 2.0));
	testAssert(epsEqual(logBase2(32.0), 5.0));
	testAssert(epsEqual(logBase2(25.281321979628068984528066732234), 4.66)); // 2^4.66 = 25.281321979628068984528066732234


	//======================================== isFinite() ========================================
	// Test for floats
	{
		testAssert(isFinite(0.f));
		testAssert(isFinite(1.0f));
		testAssert(isFinite(1.0e30f));
		testAssert(isFinite(1.0e-30f));
		testAssert(isFinite(std::numeric_limits<float>::max()));
		testAssert(isFinite(-std::numeric_limits<float>::max()));
		testAssert(isFinite(std::numeric_limits<float>::min()));
		testAssert(isFinite(-std::numeric_limits<float>::min()));
		testAssert(isFinite(std::numeric_limits<float>::denorm_min()));
		testAssert(isFinite(-std::numeric_limits<float>::denorm_min()));
		
		testAssert(!isFinite(std::numeric_limits<float>::infinity()));
		testAssert(!isFinite(-std::numeric_limits<float>::infinity()));
	
		testAssert(!isFinite(std::numeric_limits<float>::quiet_NaN()));
		testAssert(!isFinite(-std::numeric_limits<float>::quiet_NaN()));
	}

	// Speedtest for isFinite()
	{
		const int N = 1000000;

		conPrint("isFinite() [float]");
		{
			Timer timer;
			float sum = 0.0;
			for(int i=0; i<N; ++i)
			{
				const float x = isFinite((float)i) ? 1.0f : 0.0f;
				sum += x;
			}

			const double elapsed = timer.elapsed();
			conPrint("\telapsed / iter: " + toString(elapsed * 1.0e9 / N) + " ns");
			conPrint("\tsum: " + toString(sum));
		}
	}

	// Test for doubles
	{
		testAssert(isFinite(0.));
		testAssert(isFinite(1.0));
		testAssert(isFinite(1.0e30));
		testAssert(isFinite(1.0e-30));
		testAssert(isFinite(std::numeric_limits<double>::max()));
		testAssert(isFinite(-std::numeric_limits<double>::max()));
		testAssert(isFinite(std::numeric_limits<double>::min()));
		testAssert(isFinite(-std::numeric_limits<double>::min()));
		testAssert(isFinite(std::numeric_limits<double>::denorm_min()));
		testAssert(isFinite(-std::numeric_limits<double>::denorm_min()));
		
		testAssert(!isFinite(std::numeric_limits<double>::infinity()));
		testAssert(!isFinite(-std::numeric_limits<double>::infinity()));
	
		testAssert(!isFinite(std::numeric_limits<double>::quiet_NaN()));
		testAssert(!isFinite(-std::numeric_limits<double>::quiet_NaN()));
	}

	// Speedtest for isFinite()
	{
		const int N = 1000000;

		conPrint("isFinite() [double]");
		{
			Timer timer;
			float sum = 0.0;
			for(int i=0; i<N; ++i)
			{
				const float x = isFinite((double)i) ? 1.0f : 0.0f;
				sum += x;
			}

			const double elapsed = timer.elapsed();
			conPrint("\telapsed / iter: " + toString(elapsed * 1.0e9 / N) + " ns");
			conPrint("\tsum: " + toString(sum));
		}
	}


	//======================================== isNAN () ========================================
	// isNAN(float)
	testAssert(!isNAN(0.0f));
	testAssert(!isNAN(1.234f));
	testAssert(!isNAN(std::numeric_limits<float>::infinity()));
	testAssert(!isNAN(-std::numeric_limits<float>::infinity()));

	testAssert(isNAN(-std::numeric_limits<float>::quiet_NaN()));
	testAssert(isNAN(std::numeric_limits<float>::quiet_NaN()));

	// isNAN(double)
	testAssert(!isNAN(0.0));
	testAssert(!isNAN(1.234));
	testAssert(!isNAN(std::numeric_limits<double>::infinity()));
	testAssert(!isNAN(-std::numeric_limits<double>::infinity()));

	testAssert(isNAN(-std::numeric_limits<double>::quiet_NaN()));
	testAssert(isNAN(std::numeric_limits<double>::quiet_NaN()));


	//======================================== isInf () ========================================
	testAssert(!isInf(0.0f));
	testAssert(!isInf(-std::numeric_limits<float>::quiet_NaN()));
	testAssert(!isInf(std::numeric_limits<float>::quiet_NaN()));

	testAssert(isInf(std::numeric_limits<float>::infinity()));
	testAssert(isInf(-std::numeric_limits<float>::infinity()));


	testAssert(!isInf(0.0));
	testAssert(!isInf(-std::numeric_limits<double>::quiet_NaN()));
	testAssert(!isInf(std::numeric_limits<double>::quiet_NaN()));

	testAssert(isInf(std::numeric_limits<double>::infinity()));
	testAssert(isInf(-std::numeric_limits<double>::infinity()));


	//======================================= epsEqual() ==============================================
	testAssert(epsEqual(1.f, 1.000001f));
	testAssert(!epsEqual(1.f, 2.f));
	testAssert(!epsEqual(1.f, std::numeric_limits<float>::quiet_NaN()));
	testAssert(!epsEqual(std::numeric_limits<float>::quiet_NaN(), 1.f));
	testAssert(!epsEqual(1.f, std::numeric_limits<float>::infinity()));
	testAssert(!epsEqual(std::numeric_limits<float>::infinity(), 1.f));

	testAssert(epsEqual(1.0, 1.000001));
	testAssert(!epsEqual(1.0, 2.0));
	testAssert(!epsEqual(1.0, std::numeric_limits<double>::quiet_NaN()));
	testAssert(!epsEqual(std::numeric_limits<double>::quiet_NaN(), 1.0));


	// Some perf tests
	if(false)
	{
		const int N = 100000;
		const int trials = 10;
		
		{
			Timer timer;
			float sum = 0.0;
			double elapsed = 1.0e10;
			for(int t=0; t<trials; ++t)
			{
				for(int i=0; i<N; ++i)
				{
					const float x = (float)i;
					sum += isNAN(x);
				}
				elapsed = myMin(elapsed, timer.elapsed());
			}
			conPrint("isNAN() took:     " + toString(elapsed * 1.0e9 / N) + " ns");
			TestUtils::silentPrint(toString(sum));
		}
		{
			Timer timer;
			float sum = 0.0;
			double elapsed = 1.0e10;
			for(int t=0; t<trials; ++t)
			{
				for(int i=0; i<N; ++i)
				{
					const float x = (float)i;
					sum += isFinite(x);
				}
				elapsed = myMin(elapsed, timer.elapsed());
			}
			conPrint("isFinite() took: " + toString(elapsed * 1.0e9 / N) + " ns");
			TestUtils::silentPrint(toString(sum));
		}
		/*{
			Timer timer;
			float sum = 0.0;
			double elapsed = 1.0e10;
			for(int t=0; t<trials; ++t)
			{
				for(int i=0; i<N; ++i)
				{
					const float x = (float)i;
					sum += _finite(x);
				}
				elapsed = myMin(elapsed, timer.elapsed());
			}
			conPrint("_finite() took: " + toString(elapsed * 1.0e9 / N) + " ns");
			TestUtils::silentPrint(toString(sum));
		}*/
	}


	//======================================= intMod() ==============================================
	testAssert(intMod(-6, 4) == 2);
	testAssert(intMod(-5, 4) == 3);
	testAssert(intMod(-4, 4) == 0);
	testAssert(intMod(-3, 4) == 1);
	testAssert(intMod(-2, 4) == 2);
	testAssert(intMod(-1, 4) == 3);
	testAssert(intMod(0, 4)  == 0);
	testAssert(intMod(1, 4)  == 1);
	testAssert(intMod(2, 4)  == 2);
	testAssert(intMod(3, 4)  == 3);
	testAssert(intMod(4, 4)  == 0);
	testAssert(intMod(5, 4)  == 1);
	testAssert(intMod(6, 4)  == 2);

	// Test with a non-power-of-two divisor
	testAssert(intMod(-8, 7)  == 6); // c mod is -1, add 7 gives 6
	testAssert(intMod(-7, 7)  == 0); // c mod is 0
	testAssert(intMod(-6, 7)  == 1); // c mod is -6, add 7 gives 4
	testAssert(intMod(-5, 7)  == 2); // c mod is -5, add 7 gives 4
	testAssert(intMod(-4, 7)  == 3); // c mod is -4, add 7 gives 4
	testAssert(intMod(-3, 7)  == 4); // c mod is -3, add 7 gives 4
	testAssert(intMod(-2, 7)  == 5); // c mod is -2, add 7 gives 5
	testAssert(intMod(-1, 7)  == 6); // c mod is -1, add 7 gives 6
	testAssert(intMod(0, 7)  == 0);
	testAssert(intMod(1, 7)  == 1);
	testAssert(intMod(2, 7)  == 2);
	testAssert(intMod(3, 7)  == 3);
	testAssert(intMod(4, 7)  == 4);
	testAssert(intMod(5, 7)  == 5);
	testAssert(intMod(6, 7)  == 6);
	testAssert(intMod(7, 7)  == 0);
	testAssert(intMod(8, 7)  == 1);

	// Test some edge cases
	testAssert(intMod(std::numeric_limits<int>::min() + 0, 4) == 0);
	testAssert(intMod(std::numeric_limits<int>::min() + 1, 4) == 1);
	testAssert(intMod(std::numeric_limits<int>::min() + 2, 4) == 2);
	testAssert(intMod(std::numeric_limits<int>::min() + 3, 4) == 3);
	testAssert(intMod(std::numeric_limits<int>::min() + 4, 4) == 0);

	testAssert(intMod(std::numeric_limits<int>::max() - 4, 4) == 3);
	testAssert(intMod(std::numeric_limits<int>::max() - 3, 4) == 0); //(2147483647 - 3) / 4.0 = 536870911.0
	testAssert(intMod(std::numeric_limits<int>::max() - 2, 4) == 1);
	testAssert(intMod(std::numeric_limits<int>::max() - 1, 4) == 2);
	testAssert(intMod(std::numeric_limits<int>::max() - 0, 4) == 3);

	// TODO: Test for really large divisors


	//======================================= floatMod() ==============================================
	testAssert(floatMod(-6, 4) == 2.f);
	testAssert(floatMod(-5, 4) == 3.f);
	testAssert(floatMod(-4, 4) == 0.f);
	testAssert(floatMod(-3, 4) == 1.f);
	testAssert(floatMod(-2, 4) == 2.f);
	testAssert(floatMod(-1, 4) == 3.f);
	testAssert(floatMod(0, 4)  == 0.f);
	testAssert(floatMod(1, 4)  == 1.f);
	testAssert(floatMod(2, 4)  == 2.f);
	testAssert(floatMod(3, 4)  == 3.f);
	testAssert(floatMod(4, 4)  == 0.f);
	testAssert(floatMod(5, 4)  == 1.f);
	testAssert(floatMod(6, 4)  == 2.f);


	//======================================= copySign() ==============================================

	// Test for floats
	testAssert(copySign(2.0f,  10.f) ==  2.0f);
	testAssert(copySign(2.0f, -10.f) == -2.0f);
	testAssert(copySign(3.0f,  10.f) ==  3.0f);
	testAssert(copySign(3.0f, -10.f) == -3.0f);
	testAssert(copySign(std::numeric_limits<float>::max(),  10.f) ==  std::numeric_limits<float>::max());
	testAssert(copySign(std::numeric_limits<float>::max(), -10.f) == -std::numeric_limits<float>::max());
	testAssert(copySign(std::numeric_limits<float>::infinity(),  10.f) ==  std::numeric_limits<float>::infinity());
	testAssert(copySign(std::numeric_limits<float>::infinity(), -10.f) == -std::numeric_limits<float>::infinity());

	testAssert(copySign(2.0f,  0.f) ==  2.0f);
	testAssert(copySign(2.0f, -0.f) == -2.0f);

	testAssert(copySign(0.0f,  10.f) ==  0.0f);
	testAssert(isNegativeZero(copySign(0.0f, -10.f)));

	// Test for doubles
	testAssert(copySign(2.0,  10.0) ==  2.0);
	testAssert(copySign(2.0, -10.0) == -2.0);
	testAssert(copySign(3.0,  10.0) ==  3.0);
	testAssert(copySign(3.0, -10.0) == -3.0);
	testAssert(copySign(std::numeric_limits<double>::max(),  10.0) ==  std::numeric_limits<double>::max());
	testAssert(copySign(std::numeric_limits<double>::max(), -10.0) == -std::numeric_limits<double>::max());
	testAssert(copySign(std::numeric_limits<double>::infinity(),  10.0) ==  std::numeric_limits<double>::infinity());
	testAssert(copySign(std::numeric_limits<double>::infinity(), -10.0) == -std::numeric_limits<double>::infinity());

	testAssert(copySign(2.0,  0.0) ==  2.0);
	testAssert(copySign(2.0, -0.0) == -2.0);

	testAssert(copySign(0.0,  10.0) ==  0.0);
	testAssert(isNegativeZero(copySign(0.0, -10.0)));


	//======================================= sign() ==============================================
	testAssert(sign( 1.f) ==  1.0f);
	testAssert(sign(-1.f) == -1.0f);
	testAssert(sign( 10.f) ==  1.0f);
	testAssert(sign(-10.f) == -1.0f);
	testAssert( sign(std::numeric_limits<float>::max()) ==  1.0f);
	testAssert(-sign(std::numeric_limits<float>::max()) == -1.0f);
	testAssert( sign(std::numeric_limits<float>::infinity()) ==  1.0f);
	testAssert(-sign(std::numeric_limits<float>::infinity()) == -1.0f);

	testAssert(sign(0.0f) == 0.0f);
	testAssert(isNegativeZero(sign(-0.0f)));

	// Test for doubles
	testAssert(sign( 1.0) ==  1.0);
	testAssert(sign(-1.0) == -1.0);
	testAssert(sign( 10.) ==  1.0);
	testAssert(sign(-10.) == -1.0);
	testAssert( sign(std::numeric_limits<double>::max()) ==  1.0);
	testAssert(-sign(std::numeric_limits<double>::max()) == -1.0);
	testAssert( sign(std::numeric_limits<double>::infinity()) ==  1.0);
	testAssert(-sign(std::numeric_limits<double>::infinity()) == -1.0);

	testAssert(sign(0.0) == 0.0);
	testAssert(isNegativeZero(sign(-0.0)));


	//======================================= approxEq() ==============================================
	testAssert(approxEq(1.0, 1.0));
	testAssert(approxEq(1.0, 1.0000001));
	testAssert(approxEq(1.0, 0.9999999));
	testAssert(!approxEq(1.0, 1.0001));
	testAssert(!approxEq(1.0, 0.9999));
	testAssert(!approxEq(1.0, -1.0));

	testAssert(approxEq(1.0e15, 1.0000001e15));
	testAssert(approxEq(1.0e15, 0.9999999e15));


	//======================================= posOverflowed(), posUnderflowed() ==============================================
	double one = 1.0;
	//double zero = 0.0;
	//testAssert(posOverflowed(one / zero));
	testAssert(!posOverflowed(one / 0.000000001));

	testAssert(posUnderflowed(1.0e-320)); // NOTE: this test will fail if we are flushing denorms to zero.
	testAssert(!posUnderflowed(1.0e-300));
	testAssert(!posUnderflowed(1.0));
	testAssert(!posUnderflowed(0.0));


	//======================================= posFloorToInt() ==============================================
	testAssert(posFloorToInt(0.5) == 0);
	testAssert(posFloorToInt(0.0) == 0);
	testAssert(posFloorToInt(1.0) == 1);
	testAssert(posFloorToInt(1.1) == 1);
	testAssert(posFloorToInt(1.99999) == 1);
	testAssert(posFloorToInt(2.0) == 2);


	//======================================= floorToInt() ==============================================
	testAssert(floorToInt(-0.5) == -1);
	testAssert(floorToInt(0.5) == 0);
	testAssert(floorToInt(-0.0) == 0);
	testAssert(floorToInt(-1.0) == -1);
	testAssert(floorToInt(-1.1) == -2);
	testAssert(floorToInt(-1.99999) == -2);
	testAssert(floorToInt(-2.0) == -2);
	testAssert(floorToInt(2.1) == 2);


	//======================================= tanForCos() ==============================================
	testAssert(epsEqual(tanForCos(0.2), tan(acos(0.2))));


	//======================================= roundedUpDivide() ==============================================
	testAssert(roundedUpDivide(0, 1) == 0);
	testAssert(roundedUpDivide(1, 1) == 1);
	testAssert(roundedUpDivide(1, 2) == 1);
	testAssert(roundedUpDivide(2, 2) == 1);
	testAssert(roundedUpDivide(3, 2) == 2);
	testAssert(roundedUpDivide(13, 5) == 3);
	testAssert(roundedUpDivide(14, 5) == 3);
	testAssert(roundedUpDivide(15, 5) == 3);
	testAssert(roundedUpDivide(16, 5) == 4);


	//======================================= roundUpToMultiple() ==============================================
	testAssert(roundUpToMultiple(0, 1) == 0);
	testAssert(roundUpToMultiple(1, 1) == 1);
	testAssert(roundUpToMultiple(2, 1) == 2);
	testAssert(roundUpToMultiple(13, 5) == 15);
	testAssert(roundUpToMultiple(14, 5) == 15);
	testAssert(roundUpToMultiple(15, 5) == 15);
	testAssert(roundUpToMultiple(16, 5) == 20);


	//======================================= roundUpToMultipleFloating() ==============================================
	// single precision
	testEpsEqual(roundUpToMultipleFloating(500.f, 200.0f), 600.f);
	testEpsEqual(roundUpToMultipleFloating(-500.f, 200.0f), -400.f);

	// double precision
	testEpsEqual(roundUpToMultipleFloating(500.0, 200.0), 600.0);
	testEpsEqual(roundUpToMultipleFloating(-500.0, 200.0), -400.0);


	//======================================= roundToMultipleFloating() ==============================================
	// single precision
	testEpsEqual(roundToMultipleFloating(490.f, 200.0f), 400.f);
	testEpsEqual(roundToMultipleFloating(510.f, 200.0f), 600.f);
	testEpsEqual(roundToMultipleFloating(-490.f, 200.0f), -400.f);
	testEpsEqual(roundToMultipleFloating(-510.f, 200.0f), -600.f);

	// double precision
	testEpsEqual(roundToMultipleFloating(490.0, 200.0), 400.0);
	testEpsEqual(roundToMultipleFloating(510.0, 200.0), 600.0);
	testEpsEqual(roundToMultipleFloating(-490.0, 200.0), -400.0);
	testEpsEqual(roundToMultipleFloating(-510.0, 200.0), -600.0);


	//======================================= isPowerOfTwo() ==============================================
	testAssert(!isPowerOfTwo((int)-4));
	testAssert(!isPowerOfTwo((int)-3));
	testAssert(!isPowerOfTwo((int)-2));
	testAssert(!isPowerOfTwo((int)-1));
	testAssert(!isPowerOfTwo((int)0));
	testAssert(!isPowerOfTwo((unsigned int)-3));
	testAssert(!isPowerOfTwo((unsigned int)-2));
	testAssert(!isPowerOfTwo((unsigned int)-1));
	testAssert(!isPowerOfTwo((unsigned int)0));

	testAssert(isPowerOfTwo((int)1));
	testAssert(isPowerOfTwo((int)2));
	testAssert(isPowerOfTwo((int)4));
	testAssert(isPowerOfTwo((int)65536));
	testAssert(isPowerOfTwo((unsigned int)1));
	testAssert(isPowerOfTwo((unsigned int)2));
	testAssert(isPowerOfTwo((unsigned int)4));
	testAssert(isPowerOfTwo((unsigned int)65536));

	testAssert(!isPowerOfTwo((int)3));
	testAssert(!isPowerOfTwo((int)9));
	testAssert(!isPowerOfTwo((unsigned int)3));
	testAssert(!isPowerOfTwo((unsigned int)9));


	//=============================== roundUpToMultipleOfPowerOf2() =========================================
	testAssert(roundUpToMultipleOfPowerOf2(0, 4) == 0);
	testAssert(roundUpToMultipleOfPowerOf2(1, 4) == 4);
	testAssert(roundUpToMultipleOfPowerOf2(2, 4) == 4);
	testAssert(roundUpToMultipleOfPowerOf2(3, 4) == 4);
	testAssert(roundUpToMultipleOfPowerOf2(4, 4) == 4);
	testAssert(roundUpToMultipleOfPowerOf2(5, 4) == 8);
	testAssert(roundUpToMultipleOfPowerOf2(6, 4) == 8);
	testAssert(roundUpToMultipleOfPowerOf2(7, 4) == 8);
	testAssert(roundUpToMultipleOfPowerOf2(8, 4) == 8);
	testAssert(roundUpToMultipleOfPowerOf2(9, 4) == 12);

	testAssert(roundUpToMultipleOfPowerOf2(0, 128) == 0);
	testAssert(roundUpToMultipleOfPowerOf2(1, 128) == 128);
	testAssert(roundUpToMultipleOfPowerOf2(127, 128) == 128);
	testAssert(roundUpToMultipleOfPowerOf2(128, 128) == 128);
	testAssert(roundUpToMultipleOfPowerOf2(129, 128) == 256);


	//=============================== divideByTwoRoundedDown() =========================================
	testAssert(divideByTwoRoundedDown(-6) == -3);
	testAssert(divideByTwoRoundedDown(-5) == -3);
	testAssert(divideByTwoRoundedDown(-4) == -2);
	testAssert(divideByTwoRoundedDown(-3) == -2);
	testAssert(divideByTwoRoundedDown(-2) == -1);
	testAssert(divideByTwoRoundedDown(-1) == -1);
	testAssert(divideByTwoRoundedDown(0) == 0);
	testAssert(divideByTwoRoundedDown(1) == 0);
	testAssert(divideByTwoRoundedDown(2) == 1);
	testAssert(divideByTwoRoundedDown(3) == 1);
	testAssert(divideByTwoRoundedDown(4) == 2);
	testAssert(divideByTwoRoundedDown(5) == 2);
	testAssert(divideByTwoRoundedDown(6) == 3);


	//======================================= intExp2(uint32) ==============================================
	{
		testAssert(intExp2((uint32)0u) == 1);
		testAssert(intExp2((uint32)1u) == 2);
		testAssert(intExp2((uint32)2u) == 4);
		testAssert(intExp2((uint32)3u) == 8);
		testAssert(intExp2((uint32)4u) == 16);
		testAssert(intExp2((uint32)5u) == 32);

		testAssert(intExp2((uint32)31u) == 2147483648u);
	}


	//======================================= intExp2(uint64) ==============================================
	{
		testAssert(intExp2((uint64)0ull) == 1);
		testAssert(intExp2((uint64)1ull) == 2);
		testAssert(intExp2((uint64)2ull) == 4);
		testAssert(intExp2((uint64)3ull) == 8);
		testAssert(intExp2((uint64)4ull) == 16);
		testAssert(intExp2((uint64)5ull) == 32);

		testAssert(intExp2((uint64)31ull) == 2147483648u);
		testAssert(intExp2((uint64)32ull) == 4294967296u);

		testAssert(intExp2((uint64)63ull) == 9223372036854775808ull);
	}


	//======================================= intLogBase2(uint32) ==============================================
	{
		testAssert(intLogBase2((uint32)1u) == 0);
		testAssert(intLogBase2((uint32)2u) == 1);
		testAssert(intLogBase2((uint32)3u) == 1);
		testAssert(intLogBase2((uint32)4u) == 2);
		testAssert(intLogBase2((uint32)5u) == 2);
		testAssert(intLogBase2((uint32)6u) == 2);
		testAssert(intLogBase2((uint32)7u) == 2);
		testAssert(intLogBase2((uint32)8u) == 3);
		testAssert(intLogBase2(intExp2((uint32)20u) - 1) == 19);
		testAssert(intLogBase2(intExp2((uint32)20u)    ) == 20);
		testAssert(intLogBase2(intExp2((uint32)20u) + 1) == 20);

		testAssert(intLogBase2(intExp2((uint32)31u) - 1) == 30);
		testAssert(intLogBase2(intExp2((uint32)31u)    ) == 31);
		testAssert(intLogBase2(intExp2((uint32)31u) + 1) == 31);
		testAssert(intLogBase2((uint32)4294967295u) == 31);

		for(uint32 i=0; i<31; ++i)
			testAssert(intLogBase2((uint32)1u << i) == i);
	}


	//======================================= intLogBase2(uint64) ==============================================
	{
		testAssert(intLogBase2((uint64)1ull) == 0);
		testAssert(intLogBase2((uint64)2ull) == 1);
		testAssert(intLogBase2((uint64)3ull) == 1);
		testAssert(intLogBase2((uint64)4ull) == 2);
		testAssert(intLogBase2((uint64)5ull) == 2);
		testAssert(intLogBase2((uint64)6ull) == 2);
		testAssert(intLogBase2((uint64)7ull) == 2);
		testAssert(intLogBase2((uint64)8ull) == 3);
		testAssert(intLogBase2(intExp2((uint64)20ull) - 1) == 19);
		testAssert(intLogBase2(intExp2((uint64)20ull)    ) == 20);
		testAssert(intLogBase2(intExp2((uint64)20ull) + 1) == 20);

		testAssert(intLogBase2(intExp2((uint64)31ull) - 1) == 30);
		testAssert(intLogBase2(intExp2((uint64)31ull)    ) == 31);
		testAssert(intLogBase2(intExp2((uint64)31ull) + 1) == 31);
		testAssert(intLogBase2((uint64)4294967295ull) == 31);

		testAssert(intLogBase2(intExp2((uint64)63ull) - 1) == 62);
		testAssert(intLogBase2(intExp2((uint64)63ull)    ) == 63);
		testAssert(intLogBase2(intExp2((uint64)63ull) + 1) == 63);
		testAssert(intLogBase2(std::numeric_limits<uint64>::max()) == 63);

		for(uint32 i=0; i<63; ++i)
			testAssert(intLogBase2((uint64)1ull << i) == i);
	}


	//======================================= Matrix2::inverse() ==============================================
	{
	const Matrix2d m(1, 2, 3, 4);
	// determinant = 1 / (ad - bc) = 1 / (4 - 6) = -1/2

	const Matrix2d inv = m.inverse();

	testAssert(epsEqual(inv.e[0], -2.0));
	testAssert(epsEqual(inv.e[1], 1.0));
	testAssert(epsEqual(inv.e[2], 3.0/2.0));
	testAssert(epsEqual(inv.e[3], -1.0/2.0));

	{
	const Matrix2d r = m * inv;

	testAssert(epsEqual(r.e[0], 1.0));
	testAssert(epsEqual(r.e[1], 0.0));
	testAssert(epsEqual(r.e[2], 0.0));
	testAssert(epsEqual(r.e[3], 1.0));
	}
	{
	const Matrix2d r = inv * m;

	testAssert(epsEqual(r.e[0], 1.0));
	testAssert(epsEqual(r.e[1], 0.0));
	testAssert(epsEqual(r.e[2], 0.0));
	testAssert(epsEqual(r.e[3], 1.0));
	}
	{
	const Matrix2d r = m.transpose();

	testAssert(epsEqual(r.e[0], 1.0));
	testAssert(epsEqual(r.e[1], 3.0));
	testAssert(epsEqual(r.e[2], 2.0));
	testAssert(epsEqual(r.e[3], 4.0));
	}
	}


	//======================================= Matrix3d::inverse() ==============================================
	{
	const double e[9] = {1, 2, 3, 4, 5, 6, 7, 8, 0};
	const Matrix3d m(e);

	Matrix3d inv;
	testAssert(m.inverse(inv));


	{
	const Matrix3d r = m * inv;

	testAssert(epsEqual(r.e[0], 1.0));
	testAssert(epsEqual(r.e[1], 0.0));
	testAssert(epsEqual(r.e[2], 0.0));
	testAssert(epsEqual(r.e[3], 0.0));
	testAssert(epsEqual(r.e[4], 1.0));
	testAssert(epsEqual(r.e[5], 0.0));
	testAssert(epsEqual(r.e[6], 0.0));
	testAssert(epsEqual(r.e[7], 0.0));
	testAssert(epsEqual(r.e[8], 1.0));
	}
	{
	const Matrix3d r = inv * m;

	testAssert(epsEqual(r.e[0], 1.0));
	testAssert(epsEqual(r.e[1], 0.0));
	testAssert(epsEqual(r.e[2], 0.0));
	testAssert(epsEqual(r.e[3], 0.0));
	testAssert(epsEqual(r.e[4], 1.0));
	testAssert(epsEqual(r.e[5], 0.0));
	testAssert(epsEqual(r.e[6], 0.0));
	testAssert(epsEqual(r.e[7], 0.0));
	testAssert(epsEqual(r.e[8], 1.0));
	}
	}



	for(float x=0; x<1.0f; x += 0.01f)
	{
		//conPrint("x: " + toString(x));
		//conPrint("pow(" + toString(x) + ", 2.2f):     " + toString(pow(x, 2.2f)));
		//conPrint("fastPow(" + toString(x) + ", 2.2f): " + toString(fastPow(x, 2.2f)));
		testAssert(epsEqual(std::pow(x, 2.2f), Maths::fastPow(x, 2.2f), 0.02f));
	}

	//======================================= oneMinusCosX() ==============================================
	float max_direct_error = 0;
	float max_func_error = 0;
	float max_direct_relative_error = 0;
	float max_func_relative_error = 0;

	for(float x=0; x<1.f; x += 0.0001f)
	{
		const float one_minus_cos_x_d = (float)(1 - std::cos((double)x));

		const float one_minus_cos_x_f = 1 - std::cos(x);

		const float one_minus_cos_x_func = oneMinusCosX(x);

		const float direct_error = std::fabs(one_minus_cos_x_f    - one_minus_cos_x_d);
		const float func_error   = std::fabs(one_minus_cos_x_func - one_minus_cos_x_d);

		const float direct_relative_error = one_minus_cos_x_d == 0 ? 0 : direct_error / one_minus_cos_x_d;
		const float func_relative_error   = one_minus_cos_x_d == 0 ? 0 : func_error   / one_minus_cos_x_d;

		max_direct_error = myMax(max_direct_error, direct_error);
		max_func_error   = myMax(max_func_error,   func_error);
		max_direct_relative_error = myMax(max_direct_relative_error, direct_relative_error);
		max_func_relative_error   = myMax(max_func_relative_error,   func_relative_error);

		/*conPrint("");
		printVar(one_minus_cos_x_d);
		printVar(one_minus_cos_x_f);
		printVar(one_minus_cos_x_func);

		printVar(x);
		printVar(direct_error);
		printVar(func_error);*/
	}

	conPrint("");
	printVar(max_direct_error);
	printVar(max_func_error);
	printVar(max_direct_relative_error);
	printVar(max_func_relative_error);
	conPrint("");


	//assert(epsEqual(r, Matrix2d::identity(), Matrix2d(NICKMATHS_EPSILON, NICKMATHS_EPSILON, NICKMATHS_EPSILON, NICKMATHS_EPSILON)));

	

	//======================================= Misc perf tests ==============================================
	/*conPrint("float sin()");
	{
		float sum = 0.0;
		int64_t least_cycles = std::numeric_limits<int64_t>::max();
		for(int t=0; t<trials; ++t)
		{
			CycleTimer timer;
			for(int i=0; i<N; ++i)
			{
				const float x = (float)i * 0.001f;
				sum += std::sin(x);
			}

			int64_t cycles = timer.getCyclesElapsed();

			least_cycles = myMin(least_cycles, timer.getCyclesElapsed());

			printVar(cycles);
			printVar(timer.getCyclesElapsed());
			printVar((int64_t)timer.getCyclesElapsed());
			printVar((double)timer.getCyclesElapsed());
			printVar(timer.getCPUIDTime());
			printVar(least_cycles);
		}

		const double cycles = (double)least_cycles / (double)N;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + toString(sum));
	}

	conPrint("double sin()");
	{
		double sum = 0.0;
		int64_t least_cycles = std::numeric_limits<int64_t>::max();
		for(int t=0; t<trials; ++t)
		{
			CycleTimer timer;
			for(int i=0; i<N; ++i)
			{
				const double x = (double)i * 0.001;
				sum += sin(x);
			}

			least_cycles = myMin(least_cycles, timer.getCyclesElapsed());
		}

		const double cycles = (double)least_cycles / (double)N;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + toString(sum));
	}


	conPrint("float sqrt()");
	{
		float sum = 0.0;
		int64_t least_cycles = std::numeric_limits<int64_t>::max();
		for(int t=0; t<trials; ++t)
		{
			CycleTimer timer;
			for(int i=0; i<N; ++i)
			{
				const float x = (float)i * 0.001f;
				sum += std::sqrt(x);
			}

			least_cycles = myMin(least_cycles, timer.getCyclesElapsed());
		}

		const double cycles = (double)least_cycles / (double)N;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + toString(sum));
	}*/

	// Compute clock speed
	
#if CYCLETIMER_SUPPORTED && !defined(__APPLE__)
	// Don't run these speed tests on OSX, as CycleTimer crashes on OSX.
	{
	const int N = 1000000;
	const int trials = 4;


	/*
	Performance results on i7 920, VS 2010 RelWithDebInfo x64
	=========================================================
	sin() [float]
			cycles: 46.822795
			sum: 1749.0287
	sin() [double]
			cycles: 62.940231
			sum: 1748.829789882393
	cos() [float]
			cycles: 47.293724
			sum: 3308.5215
	cos() [double]
			cycles: 62.665445999999996
			sum: 3308.3931283487846
	sqrt() [float]
			cycles: 17.198042
			sum: 21090482
	sqrt() [double]
			cycles: 30.747671999999998
			sum: 21081835.249828096
	exp() [float]
			cycles: 30.691259
			sum: 2202528000
	exp() [double]
			cycles: 49.276868
			sum: 2202535566.766156
	pow() [float]
			cycles: 148.209637
			sum: 49526670
	fastPow() [float]
			cycles: 49.554341
			sum: 49375020
	pow() [double]
			cycles: 155.310506
			sum: 49527833.01978005
	fract() [float]
			cycles: 7.21622
			sum: 524392.8
	fastFract() [float]
			cycles: 16.67572593688965
			sum: 524393.9
	fastPosFract() [float]
			cycles: 3.9148454666137695
			sum: 495.09283
	modfFract() [float]
			cycles: 28.831317901611328
			sum: 524393.9
	Vec4f pow() [float]
			cycles: 397.72076699999997
			sum: (4.9526672E+007,4.9526672E+007,4.9526672E+007,49526670)
	Vec4f pow() take 2 [float]
			cycles: 44.687275
	sum: (4.9527412E+007,4.9527412E+007,4.9527412E+007,49527412)

	Performance results on i7 920, VS 2012 RelWithDebInfo x64
	=========================================================
	Note that autovectorisation is being done in this case.

	sin() [float]
			cycles: 7.51387
			sum: 1749.0292
	sin() [double]
			cycles: 21.525688
			sum: 1748.8297898825363
	cos() [float]
			cycles: 8.698518
			sum: 3308.531
	cos() [double]
			cycles: 23.039825
			sum: 3308.3931283490383
	sqrt() [float]
			cycles: 4.288883
			sum: 21082504
	sqrt() [double]
			cycles: 15.248222
			sum: 21081835.24982831
	exp() [float]
			cycles: 8.037737
			sum: 2202537700
	exp() [double]
			cycles: 18.258219
			sum: 2202535566.7661266
	pow() [float]
			cycles: 27.652151
			sum: 49527576
	fastPow() [float]
			cycles: 55.227899
			sum: 49375020
	pow() [double]
			cycles: 60.218455999999996
			sum: 49527833.01977931
	fract() [float]
			cycles: 21.037748
			sum: 524392.8
	fastFract() [float]
			cycles: 28.883559226989746
			sum: 524393.9
	fastPosFract() [float]
			cycles: 13.387301445007324
			sum: 495.09283
	modfFract() [float]
			cycles: 39.44554901123047
			sum: 524393.9
	Vec4f pow() [float]
			cycles: 403.621428
			sum: (4.9526672E+007,4.9526672E+007,4.9526672E+007,49526670)
	Vec4f pow() take 2 [float]
			cycles: 43.942569
			sum: (4.9527412E+007,4.9527412E+007,4.9527412E+007,49527412)
	 */

	conPrint("sin() [float]");
	{
		CycleTimer timer;
		float sum = 0.0;
		CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
		for(int t=0; t<trials; ++t)
		{
			for(int i=0; i<N; ++i)
			{
				const float x = (float)i * 0.001f;
				sum += std::sin(x);
			}
			elapsed = myMin(elapsed, timer.elapsed());
		}
		const double cycles = elapsed / (double)N;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + toString(sum));
	}

	conPrint("sin() [double]");
	{
	CycleTimer timer;
	double sum = 0.0;
	CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
	for(int t=0; t<trials; ++t)
	{
		for(int i=0; i<N; ++i)
		{
			const double x = (double)i * 0.001;
			sum += std::sin(x);
		}
		elapsed = myMin(elapsed, timer.elapsed());
	}
	const double cycles = elapsed / (double)N;
	conPrint("\tcycles: " + toString(cycles));
	conPrint("\tsum: " + toString(sum));
	}

	conPrint("cos() [float]");
	{
		CycleTimer timer;
		float sum = 0.0;
		CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
		for(int t=0; t<trials; ++t)
		{
			for(int i=0; i<N; ++i)
			{
				const float x = (float)i * 0.001f;
				sum += std::cos(x);
			}
			elapsed = myMin(elapsed, timer.elapsed());
		}
		const double cycles = elapsed / (double)N;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + toString(sum));
	}

	conPrint("cos() [double]");
	{
	CycleTimer timer;
	double sum = 0.0;
	CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
	for(int t=0; t<trials; ++t)
	{
		for(int i=0; i<N; ++i)
		{
			const double x = (double)i * 0.001;
			sum += std::cos(x);
		}
		elapsed = myMin(elapsed, timer.elapsed());
	}
	const double cycles = elapsed / (double)N;
	conPrint("\tcycles: " + toString(cycles));
	conPrint("\tsum: " + toString(sum));
	}

	conPrint("tan() [float]");
	{
		CycleTimer timer;
		float sum = 0.0;
		CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
		for(int t=0; t<trials; ++t)
		{
			for(int i=0; i<N; ++i)
			{
				const float x = (float)i * 0.001f;
				sum += std::tan(x);
			}
			elapsed = myMin(elapsed, timer.elapsed());
		}
		const double cycles = elapsed / (double)N;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + toString(sum));
	}

	conPrint("tan() [double]");
	{
	CycleTimer timer;
	double sum = 0.0;
	CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
	for(int t=0; t<trials; ++t)
	{
		for(int i=0; i<N; ++i)
		{
			const double x = (double)i * 0.001;
			sum += std::tan(x);
		}
		elapsed = myMin(elapsed, timer.elapsed());
	}
	const double cycles = elapsed / (double)N;
	conPrint("\tcycles: " + toString(cycles));
	conPrint("\tsum: " + toString(sum));
	}

	conPrint("--------");
	conPrint("Vec4f(x).length()");
	{
	CycleTimer timer;
	float sum = 0.0;
	for(int i=0; i<N; ++i)
	{
		const float x = (float)i * 0.001f;
		sum += Vec4f(x).length();
	}
	const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
	const double cycles = elapsed / (double)N;
	conPrint("\tcycles: " + toString(cycles));
	conPrint("\tsum: " + toString(sum));
	}

	conPrint("sqrtSSE Vec4f(x) length()");
	{
	CycleTimer timer;
	float sum = 0.0;
	for(int i=0; i<N; ++i)
	{
		const float x = (float)i * 0.001f;
		sum += sqrtSSE(dot(Vec4f(x), Vec4f(x)));
	}
	const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
	const double cycles = elapsed / (double)N;
	conPrint("\tcycles: " + toString(cycles));
	conPrint("\tsum: " + toString(sum));
	}

	conPrint("sqrtf Vec4f(x) length()");
	{
	CycleTimer timer;
	float sum = 0.0;
	for(int i=0; i<N; ++i)
	{
		const float x = (float)i * 0.001f;
		sum += sqrtf(dot(Vec4f(x), Vec4f(x)));
	}
	const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
	const double cycles = elapsed / (double)N;
	conPrint("\tcycles: " + toString(cycles));
	conPrint("\tsum: " + toString(sum));
	}

	conPrint("_mm_sqrt_ss Vec4f(x) length()");
	{
	CycleTimer timer;
	float sum = 0.0;
	for(int i=0; i<N; ++i)
	{
		const float x = (float)i * 0.001f;
		sum += _mm_cvtss_f32(_mm_sqrt_ss(_mm_dp_ps(Vec4f(x).v, Vec4f(x).v, 255)));
	}
	const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
	const double cycles = elapsed / (double)N;
	conPrint("\tcycles: " + toString(cycles));
	conPrint("\tsum: " + toString(sum));
	}

	conPrint("_mm_sqrt_ps Vec4f(x) length()");
	{
	CycleTimer timer;
	float sum = 0.0;
	for(int i=0; i<N; ++i)
	{
		const float x = (float)i * 0.001f;
		sum += _mm_cvtss_f32(_mm_sqrt_ps(_mm_dp_ps(Vec4f(x).v, Vec4f(x).v, 255)));
	}
	const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
	const double cycles = elapsed / (double)N;
	conPrint("\tcycles: " + toString(cycles));
	conPrint("\tsum: " + toString(sum));
	}


	conPrint("--------");
	conPrint("sqrtf() [float]");
	{
	CycleTimer timer;
	float sum = 0.0;
	for(int i=0; i<N; ++i)
	{
		const float x = (float)i * 0.001f;
		sum += sqrtf(x);
	}
	const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
	const double cycles = elapsed / (double)N;
	conPrint("\tcycles: " + toString(cycles));
	conPrint("\tsum: " + toString(sum));
	}

	conPrint("sqrt() [float]");
	{
	CycleTimer timer;
	float sum = 0.0;
	for(int i=0; i<N; ++i)
	{
		const float x = (float)i * 0.001f;
		sum += sqrt(x);
	}
	const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
	const double cycles = elapsed / (double)N;
	conPrint("\tcycles: " + toString(cycles));
	conPrint("\tsum: " + toString(sum));
	}

	conPrint("std::sqrt() [float]");
	{
	CycleTimer timer;
	float sum = 0.0;
	for(int i=0; i<N; ++i)
	{
		const float x = (float)i * 0.001f;
		sum += std::sqrt(x);
	}
	const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
	const double cycles = elapsed / (double)N;
	conPrint("\tcycles: " + toString(cycles));
	conPrint("\tsum: " + toString(sum));
	}

	conPrint("sqrt() [double]");
	{
	CycleTimer timer;
	double sum = 0.0;
	for(int i=0; i<N; ++i)
	{
		const double x = (double)i * 0.001;
		sum += std::sqrt(x);
	}
	const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
	const double cycles = elapsed / (double)N;
	conPrint("\tcycles: " + toString(cycles));
	conPrint("\tsum: " + toString(sum));
	}
	conPrint("--------");




	conPrint("exp() [float]");
	{
	CycleTimer timer;
	float sum = 0.0;
	for(int i=0; i<N; ++i)
	{
		const float x = (float)i * 0.00001f;
		sum += std::exp(x);
	}
	const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
	const double cycles = elapsed / (double)N;
	conPrint("\tcycles: " + toString(cycles));
	conPrint("\tsum: " + toString(sum));
	}

	conPrint("exp() [double]");
	{
	CycleTimer timer;
	double sum = 0.0;
	for(int i=0; i<N; ++i)
	{
		const double x = (double)i * 0.00001;
		sum += std::exp(x);
	}
	const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
	const double cycles = elapsed / (double)N;
	conPrint("\tcycles: " + toString(cycles));
	conPrint("\tsum: " + toString(sum));
	}

	conPrint("uint32 mod");
	{
	CycleTimer timer;
	uint32 sum = 0;
	for(int i=0; i<N; ++i)
	{
		const uint32 x = (uint32)i;
		sum += sum % (x + 1000);
	}
	const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
	const double cycles = elapsed / (double)N;
	conPrint("\tcycles: " + toString(cycles));
	conPrint("\tsum: " + toString(sum));
	}


	conPrint("pow() [float]");
	{
		CycleTimer timer;
		float sum = 0.0;
		for(int i=0; i<N; ++i)
		{
			const float x = (float)i * 0.00001f;
			sum += std::pow(x, 2.2f);
		}
		const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
		const double cycles = elapsed / (double)N;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + toString(sum));
	}

	conPrint("fastPow() [float]");
	{
		CycleTimer timer;
		float sum = 0.0;
		for(int i=0; i<N; ++i)
		{
			const float x = (float)i * 0.00001f;
			sum += fastPow(x, 2.2f);
		}
		const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
		const double cycles = elapsed / (double)N;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + toString(sum));
	}

	conPrint("pow() [double]");
	{
		CycleTimer timer;
		double sum = 0.0;
		for(int i=0; i<N; ++i)
		{
			const double x = (double)i * 0.00001;
			sum += std::pow(x, 2.2);
		}
		const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
		const double cycles = elapsed / (double)N;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + toString(sum));
	}

	PCG32 rng(1);

	const int DATA_SIZE = 1 << 20; // 1048576;
	std::vector<float> data(DATA_SIZE);
	for(int i=0; i<DATA_SIZE; ++i)
		data[i] = (rng.unitRandom() - 0.5f) * 100.0f;

	conPrint("fract() [float]");
	{
		CycleTimer timer;
		float sum = 0.0;
		for(int i=0; i<N; ++i)
		{
			const float x = (float)i * 0.00001f;
			sum += Maths::fract(x);
		}
		//for(int i=0; i<DATA_SIZE; ++i)
		//	sum += Maths::fract(data[i]);

		const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
		const double cycles = elapsed / (double)N;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + toString(sum));
	}
	
	testAssert(epsEqual(fract(1.3f), 0.3f));
	testAssert(epsEqual(fract(-1.3f), 0.7f));
	testAssert(epsEqual(fastFract(1.3f), 0.3f));
	testAssert(epsEqual(fastFract(-1.3f), 0.7f));
	testAssert(epsEqual(modfFract(1.3f), 0.3f));
	testAssert(epsEqual(modfFract(-1.3f), 0.7f));

	conPrint("fastFract() [float]");
	{
		CycleTimer timer;
		float sum = 0.0;
		/*for(int i=0; i<N; ++i)
		{
			const float x = (float)i * 0.00001f;
			sum += fastFract(x);
		}*/
		for(int i=0; i<DATA_SIZE; ++i)
			sum += fastFract(data[i]);

		const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
		const double cycles = elapsed / (double)DATA_SIZE;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + toString(sum));
	}

	conPrint("fastPosFract() [float]");
	{
		CycleTimer timer;
		float sum = 0.0;
		/*for(int i=0; i<N; ++i)
		{
			const float x = (float)i * 0.00001f;
			sum += fastPosFract(x);
		}*/
		for(int i=0; i<DATA_SIZE; ++i)
			sum += fastPosFract(data[i]);

		const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
		const double cycles = elapsed / (double)DATA_SIZE;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + toString(sum));
	}
	
	conPrint("modfFract() [float]");
	{
		CycleTimer timer;
		float sum = 0.0;
		/*for(int i=0; i<N; ++i)
		{
			const float x = (float)i * 0.00001f;
			sum += modfFract(x);
		}*/
		for(int i=0; i<DATA_SIZE; ++i)
			sum += modfFract(data[i]);
		const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
		const double cycles = elapsed / (double)DATA_SIZE;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + toString(sum));
	}

	//================= Test pow vectorisation on Vec4f =================
	conPrint("Vec4f pow() [float]");
	{
		CycleTimer timer;
		Vec4f sum(0.0f);
		for(int i=0; i<N; ++i)
		{
			const float x = (float)i * 0.00001f;
			Vec4f v(x, x, x, x);
			Vec4f res;
			for(int z=0; z<4; ++z)
				res.x[z] = std::pow(v.x[z], 2.2f);
			sum += res; // Vec4f(std::pow(v.x[0], 2.2f), std::pow(v.x[1], 2.2f), std::pow(v.x[2], 2.2f), std::pow(v.x[3], 2.2f));
		}
		const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
		const double cycles = elapsed / (double)N;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + sum.toString());
	}
	
	conPrint("Vec4f powf4() [float]");
	{
		CycleTimer timer;
		Vec4f sum(0.0f);
		for(int i=0; i<N; ++i)
		{
			const float x = (float)i * 0.00001f;
			Vec4f v(x, x, x, x);
			Vec4f res(powf4(v.v, Vec4f(2.2f).v));

			//float res[4];
			//for(int z=0; z<4; ++z)
			//	sum.x[z] += std::pow(x + z, 2.2f);
			
			//sum += Vec4f(res[0], res[1], res[2], res[3]);
			sum += res; // Vec4f(std::pow(v.x[0], 2.2f), std::pow(v.x[1], 2.2f), std::pow(v.x[2], 2.2f), std::pow(v.x[3], 2.2f));
		}
		const CycleTimer::CYCLETIME_TYPE elapsed = timer.elapsed();
		const double cycles = elapsed / (double)N;
		conPrint("\tcycles: " + toString(cycles));
		conPrint("\tsum: " + sum.toString());
	}
	}


	{
		const int trials = 1;
		const int N = 1000000;
		conPrint("pow4() [float]");
		{
		
			CycleTimer timer;
			float sum = 0.0;
			CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
			for(int t=0; t<trials; ++t)
			{
				for(int i=0; i<N; ++i)
				{
					const float x = (float)i * 0.0001f;
					sum += pow4(x);
				}
				elapsed = myMin(elapsed, timer.elapsed());
			}
			const double cycles = elapsed / (double)N;
			conPrint("\tcycles: " + toString(cycles));
			conPrint("\tsum: " + toString(sum));
		}

		conPrint("pow4Fast() [float]");
		{
		
			CycleTimer timer;
			float sum = 0.0;
			CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
			for(int t=0; t<trials; ++t)
			{
				for(int i=0; i<N; ++i)
				{
					const float x = (float)i * 0.0001f;
					sum += pow4Fast(x);
				}
				elapsed = myMin(elapsed, timer.elapsed());
			}
			const double cycles = elapsed / (double)N;
			conPrint("\tcycles: " + toString(cycles));
			conPrint("\tsum: " + toString(sum));
		}

		conPrint("pow8() [float]");
		{
		
			CycleTimer timer;
			float sum = 0.0;
			CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
			for(int t=0; t<trials; ++t)
			{
				for(int i=0; i<N; ++i)
				{
					const float x = (float)i * 0.0001f;
					sum += pow8(x);
				}
				elapsed = myMin(elapsed, timer.elapsed());
			}
			const double cycles = elapsed / (double)N;
			conPrint("\tcycles: " + toString(cycles));
			conPrint("\tsum: " + toString(sum));
		}

		conPrint("pow8Fast() [float]");
		{
		
			CycleTimer timer;
			float sum = 0.0;
			CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
			for(int t=0; t<trials; ++t)
			{
				for(int i=0; i<N; ++i)
				{
					const float x = (float)i * 0.0001f;
					sum += pow8Fast(x);
				}
				elapsed = myMin(elapsed, timer.elapsed());
			}
			const double cycles = elapsed / (double)N;
			conPrint("\tcycles: " + toString(cycles));
			conPrint("\tsum: " + toString(sum));
		}

		conPrint("pow(x, 1/8)() [float]");
		{
		
			CycleTimer timer;
			float sum = 0.0;
			CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
			for(int t=0; t<trials; ++t)
			{
				for(int i=0; i<N; ++i)
				{
					const float x = (float)i * 0.0001f;
					sum += std::pow(x, (1.0f / 8));
				}
				elapsed = myMin(elapsed, timer.elapsed());
			}
			const double cycles = elapsed / (double)N;
			conPrint("\tcycles: " + toString(cycles));
			conPrint("\tsum: " + toString(sum));
		}

		conPrint("powOneOverEight() [float]");
		{
		
			CycleTimer timer;
			float sum = 0.0;
			CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
			for(int t=0; t<trials; ++t)
			{
				for(int i=0; i<N; ++i)
				{
					const float x = (float)i * 0.0001f;
					sum += powOneOverEight(x);
				}
				elapsed = myMin(elapsed, timer.elapsed());
			}
			const double cycles = elapsed / (double)N;
			conPrint("\tcycles: " + toString(cycles));
			conPrint("\tsum: " + toString(sum));
		}

		// Test uInt32ToUnitFloatScale
		conPrint("multiplying with uInt32ToUnitFloatScale()");
		{
			CycleTimer timer;
			float sum = 0.0;
			CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
			for(int t=0; t<trials; ++t)
			{
				for(int i=0; i<N; ++i)
				{
					const float x = (float)i * uInt32ToUnitFloatScale();
					sum += x;
				}
				elapsed = myMin(elapsed, timer.elapsed());
			}
			const double cycles = elapsed / (double)N;
			conPrint("\tcycles: " + toString(cycles));
			conPrint("\tsum: " + toString(sum));
		}
		conPrint("setting bits");
		{
			CycleTimer timer;
			float sum = 0.0;
			CycleTimer::CYCLETIME_TYPE elapsed = std::numeric_limits<CycleTimer::CYCLETIME_TYPE>::max();
			for(int t=0; t<trials; ++t)
			{
				for(int i=0; i<N; ++i)
				{
					float x;
					uint32 data = ((1 << 23) - 1) & i;
					std::memcpy(&x, &data, 4);
					sum += x;
				}
				elapsed = myMin(elapsed, timer.elapsed());
			}
			const double cycles = elapsed / (double)N;
			conPrint("\tcycles: " + toString(cycles));
			conPrint("\tsum: " + toString(sum));
		}
	}
	
#endif // CYCLETIMER_SUPPORTED && !defined(__APPLE__)

}


#endif // BUILD_TESTS
