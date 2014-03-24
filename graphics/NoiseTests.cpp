/*=====================================================================
NoiseTests.cpp
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-06-27 10:45:37 +0100
=====================================================================*/
#include "NoiseTests.h"


#if BUILD_TESTS


#include "PerlinNoise.h"
#include "../indigo/TestUtils.h"
#include "../utils/BufferInStream.h"
#include "../utils/BufferOutStream.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../indigo/globals.h"


void NoiseTests::test()
{
	conPrint("NoiseTests::test()");

	PerlinNoise::init();


	// Check that noise(x, y, 0) gives the same result as noise(x, y).
	/*{
		const float v = PerlinNoise::noise(Vec4f(0.f, 0.f, 0.f, 0));
		const float v2 = PerlinNoise::noise(0.f, 0.f);
		testAssert(epsEqual(v, v2));
	}
	{
		const float v = PerlinNoise::noise(Vec4f(1.3f, 0.3f, 0.f, 0));
		const float v2 = PerlinNoise::noise(1.3f, 0.3f);
		testAssert(epsEqual(v, v2));
	}

	{
		const float v = PerlinNoise::noise(Vec4f(-1.3f, 0.3f, 0.f, 0));
		const float v2 = PerlinNoise::noise(-1.3f, 0.3f);
		testAssert(epsEqual(v, v2));
	}

	{
		const float v = PerlinNoise::noise(Vec4f(546.3f, 67876.5675f, 0.f, 0));
		const float v2 = PerlinNoise::noise(546.3f, 67876.5675f);
		testAssert(epsEqual(v, v2));
	}*/


	/*{
		float ref_v = PerlinNoise::noiseRef<float>(1.1f, 2.2f, 3.3f);
		float v     = PerlinNoise::noise(Vec4f(1.1f, 2.2f, 3.3f, 0));
		testAssert(epsEqual(ref_v, v));
	}*/
	


	//============================== Performance tests /==============================
	if(false)
	{
		{
			conPrint("PerlinNoise::noise()");
			Timer t;
			const int N = 1000000;
			float sum = 0;
			for(int i=0; i<N; ++i)
			{
				float x = PerlinNoise::noise(Vec4f(i * 0.001f, i * 0.02f, i*0.03f, 0));
				sum += x;
			}

			double elapsed = t.elapsed();
			printVar(sum);
			conPrint("elapsed: " + toString(1.0e9 * elapsed / N) + " ns");
		}

		{
			conPrint("PerlinNoise::noise(x, y)");
			Timer t;
			const int N = 1000000;
			float sum = 0;
			for(int i=0; i<N; ++i)
			{
				float x = PerlinNoise::noise(i * 0.001f, i * 0.02f);
				sum += x;
			}

			double elapsed = t.elapsed();
			printVar(sum);
			conPrint("elapsed: " + toString(1.0e9 * elapsed / N) + " ns");
		}

		{
			conPrint("PerlinNoise::noise4Valued()");
			Timer t;
			const int N = 1000000;
			Vec4f sum(0);
			for(int i=0; i<N; ++i)
			{
				const Vec4f x = PerlinNoise::noise4Valued(Vec4f(i * 0.001f, i * 0.02f, i*0.03f, 0));
				sum += x;
			}

			double elapsed = t.elapsed();
			conPrint("sum: " + sum.toString());
			conPrint("elapsed: " + toString(1.0e9 * elapsed / N) + " ns");
		}
	}

}


#endif
