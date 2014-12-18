#include "mathstypes.h"


#include "Matrix2.h"
#include "matrix3.h"
#include "Quat.h"
#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "../utils/CycleTimer.h"
#include "../utils/Platform.h"
#include "../utils/MTwister.h"
#include <vector>
#include <limits>


#if BUILD_TESTS


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


void Maths::test()
{
	conPrint("Maths::test()");

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

	//======================================= uInt32ToUnitFloatScale() ========================================
	{
		const float x = 4294967295u * Maths::uInt32ToUnitFloatScale();
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


	//======================================== isNAN () ========================================
	testAssert(!isNAN(0.0f));
	testAssert(!isNAN(std::numeric_limits<float>::infinity()));
	testAssert(!isNAN(-std::numeric_limits<float>::infinity()));

	testAssert(isNAN(-std::numeric_limits<float>::quiet_NaN()));
	testAssert(isNAN(std::numeric_limits<float>::quiet_NaN()));

	

	testAssert(!isNAN(0.0));
	testAssert(!isNAN(std::numeric_limits<double>::infinity()));
	testAssert(!isNAN(-std::numeric_limits<double>::infinity()));

	testAssert(isNAN(-std::numeric_limits<double>::quiet_NaN()));
	testAssert(isNAN(std::numeric_limits<double>::quiet_NaN()));


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







	testAssert(approxEq(1.0, 1.0));
	testAssert(approxEq(1.0, 1.0000001));
	testAssert(!approxEq(1.0, 1.0001));

	testAssert(roundToInt(0.0) == 0);
	testAssert(roundToInt(0.1) == 0);
	testAssert(roundToInt(0.5) == 0 || roundToInt(0.5) == 1);
	testAssert(roundToInt(0.51) == 1);
	testAssert(roundToInt(0.9) == 1);
	testAssert(roundToInt(1.0) == 1);
	testAssert(roundToInt(1.1) == 1);
	testAssert(roundToInt(1.9) == 2);


	double one = 1.0;
	//double zero = 0.0;
	//testAssert(posOverflowed(one / zero));
	testAssert(!posOverflowed(one / 0.000000001));

	testAssert(posUnderflowed(1.0e-320));
	testAssert(!posUnderflowed(1.0e-300));
	testAssert(!posUnderflowed(1.0));
	testAssert(!posUnderflowed(0.0));

	testAssert(posFloorToInt(0.5) == 0);
	testAssert(posFloorToInt(0.0) == 0);
	testAssert(posFloorToInt(1.0) == 1);
	testAssert(posFloorToInt(1.1) == 1);
	testAssert(posFloorToInt(1.99999) == 1);
	testAssert(posFloorToInt(2.0) == 2);

	testAssert(floorToInt(-0.5) == -1);
	testAssert(floorToInt(0.5) == 0);
	testAssert(floorToInt(-0.0) == 0);
	testAssert(floorToInt(-1.0) == -1);
	testAssert(floorToInt(-1.1) == -2);
	testAssert(floorToInt(-1.99999) == -2);
	testAssert(floorToInt(-2.0) == -2);
	testAssert(floorToInt(2.1) == 2);

	testAssert(epsEqual(tanForCos(0.2), tan(acos(0.2))));


	testAssert(roundedUpDivide(0, 1) == 0);
	testAssert(roundedUpDivide(1, 1) == 1);
	testAssert(roundedUpDivide(1, 2) == 1);
	testAssert(roundedUpDivide(2, 2) == 1);
	testAssert(roundedUpDivide(3, 2) == 2);

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

	// Test oneMinusCosX()

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
	
#if !defined(OSX)
	// Don't run these speed tests on OSX, as CycleTimer crashes on OSX.

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

	conPrint("sqrt() [float]");
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

	MTwister rng(1);

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
	}
	
#endif // !defined(OSX)

}


#endif // BUILD_TESTS
