#include "Colour4f.h"


#include "../utils/StringUtils.h"


const std::string Colour4f::toString() const
{
	return "(" + ::toString(x[0]) + ", " + ::toString(x[1]) + ", " + ::toString(x[2]) + ", " + ::toString(x[3]) + ")";
}


#if BUILD_TESTS


#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../maths/Vec4f.h"


void Colour4f::test()
{
	conPrint("Colour4f::test()");

	testAssert(Colour4f(1.23f).isFinite());
	testAssert(Colour4f(0.f).isFinite());
	testAssert(Colour4f(0.f, 1.f, 2.f, 3.f).isFinite());
	testAssert(Colour4f(-0.f, -1.f, -2.f, -3.f).isFinite());

	testAssert(!Colour4f(0.f, 1.f, 2.f, -std::numeric_limits<float>::infinity()).isFinite());
	testAssert(!Colour4f(0.f, 1.f, 2.f, std::numeric_limits<float>::infinity()).isFinite());
	testAssert(!Colour4f(0.f, 1.f, std::numeric_limits<float>::infinity(), 3.f).isFinite());
	testAssert(!Colour4f(0.f, std::numeric_limits<float>::infinity(), 2.f, 3.f).isFinite());
	testAssert(!Colour4f(std::numeric_limits<float>::infinity(), 1.f, 2.f, 3.f).isFinite());
	testAssert(!Colour4f(std::numeric_limits<float>::infinity()).isFinite());

	testAssert(!Colour4f(0.f, 1.f, 2.f, -std::numeric_limits<float>::quiet_NaN()).isFinite());
	testAssert(!Colour4f(0.f, 1.f, 2.f, std::numeric_limits<float>::quiet_NaN()).isFinite());
	testAssert(!Colour4f(0.f, 1.f, std::numeric_limits<float>::quiet_NaN(), 3.f).isFinite());
	testAssert(!Colour4f(0.f, std::numeric_limits<float>::quiet_NaN(), 2.f, 3.f).isFinite());
	testAssert(!Colour4f(std::numeric_limits<float>::quiet_NaN(), 1.f, 2.f, 3.f).isFinite());
	testAssert(!Colour4f(std::numeric_limits<float>::quiet_NaN()).isFinite());


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
				Colour4f col(x);
				sum += col.isFinite();
			}
			elapsed = myMin(elapsed, timer.elapsed());
		}
		conPrint("isFinite() took: " + ::toString(elapsed * 1.0e9 / N) + " ns");
		TestUtils::silentPrint(::toString(sum));
	}
}


#endif // BUILD_TESTS
