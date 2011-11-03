#include "mathstypes.h"


#include "../indigo/TestUtils.h"
#include "Matrix2.h"
#include "matrix3.h"
#include "Quat.h"
#include "../indigo/globals.h"
#include "../utils/stringutils.h"
#include "../utils/timer.h"
#include "../utils/CycleTimer.h"
#include "../utils/platform.h"
#include "../utils/MTwister.h"
#include <vector>


#if (BUILD_TESTS)


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


void Maths::test()
{
	

	conPrint("Maths::test()");


	{
	Quatd identity = Quatd::identity();
	Quatd q = Quatd::fromAxisAndAngle(Vec3d(0,0,1), 20.0);

	Matrix3d mat;
	Maths::lerp(identity, q, 0.5).toMatrix(mat);

	Matrix3d rot = Matrix3d::rotationMatrix(Vec3d(0,0,1), 10.0);

	testAssert(epsMatrixEqual(mat, rot));

	}



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
	double zero = 0.0;
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


	//assert(epsEqual(r, Matrix2d::identity(), Matrix2d(NICKMATHS_EPSILON, NICKMATHS_EPSILON, NICKMATHS_EPSILON, NICKMATHS_EPSILON)));

	const int N = 1000000;
	const int trials = 4;

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

	const double clock_freq = 2.6e9;


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
		/*for(int i=0; i<N; ++i)
		{
			const float x = (float)i * 0.00001f;
			sum += Maths::fract(x);
		}*/
		for(int i=0; i<DATA_SIZE; ++i)
			sum += Maths::fract(data[i]);

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



	for(float x=0; x<1.0f; x += 0.01f)
	{
		//conPrint("x: " + toString(x));
		//conPrint("pow(" + toString(x) + ", 2.2f):     " + toString(pow(x, 2.2f)));
		//conPrint("fastPow(" + toString(x) + ", 2.2f): " + toString(fastPow(x, 2.2f)));
		testAssert(epsEqual(std::pow(x, 2.2f), Maths::fastPow(x, 2.2f), 0.02f));
	}


	// exit(0);

}
#endif
