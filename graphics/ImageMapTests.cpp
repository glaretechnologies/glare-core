/*=====================================================================
ImageMapTests.cpp
-------------------
Copyright Glare Technologies Limited 2014 -
Generated at 2011-05-22 19:51:52 +0100
=====================================================================*/
#include "ImageMapTests.h"


#ifdef BUILD_TESTS


#include "ImageMap.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"
#include "../utils/Plotter.h"


#if 0 // If do perf tests


static Colour3f testTexture(int res, double& elapsed_out)
{
	const int W = res;
	const int H = res;
	const int channels = 3;
	ImageMap<unsigned char, UInt8ComponentValueTraits> m(W, H, channels);
	for(int x=0; x<W; ++x)
		for(int y=0; y<H; ++y)
			for(int c=0; c<channels; ++c)
				m.getPixel(x, y)[c] = (unsigned char)(x + y + c);

	double clock_freq = 2.798e9;

	//conPrint("\n\n");

	Timer timer;
	const int N = 10000000;

	float xy_sum = 0;
	for(int i=0; i<N; ++i)
	{
		float x = (float)(-45654 + ((i * 87687) % 95465)) * 0.003454f;
		float y = (float)(-67878 + ((i * 5354) % 45445)) * 0.007345f;

		xy_sum += x + y;
	}


	conPrint("xy sum: " + toString(xy_sum));
	double elapsed = timer.elapsed();
	double overhead_cycles = (elapsed / (double)N) * clock_freq; // s * cycles s^-1
	//conPrint("Over head cycles: " + toString(overhead_cycles));


	timer.reset();

	Colour3f sum(0,0,0);
	xy_sum = 0;
	for(int i=0; i<N; ++i)
	{
		float x = (float)(-45654 + ((i * 87687) % 95465)) * 0.003454f;
		float y = (float)(-67878 + ((i * 5354) % 45445)) * 0.007345f;

		//conPrint(toString(x) + " " + toString(y));
		//Colour3f c = m.vec3SampleTiled(x, y);
		float dv_ds, dv_dt;
		Colour3f c(m.getDerivs(x, y, dv_ds, dv_dt));
		sum += c;
		xy_sum += x + y;
	}
	elapsed = timer.elapsed();
	conPrint(sum.toVec3().toString());
	conPrint("xy sum: " + toString(xy_sum));
	//conPrint("Elapsed: " + toString(elapsed) + " s");
	const double cycles = (elapsed / (double)N) * clock_freq; // s * cycles s^-1
	//conPrint("cycles: " + toString(cycles));

	elapsed_out = cycles - overhead_cycles;
	return sum;
}


static void perfTestWithTextureWidth(int width)
{
	conPrint("-----------------");
	conPrint("width: " + toString(width));

	double elapsed;
	Colour3f col = testTexture(width, elapsed);
	conPrint("elapsed: " + toString(elapsed) + " cycles / sample");
}


#endif // End if do perf tests


void ImageMapTests::test()
{
	conPrint("ImageMapTests::test()");

	// Test UInt8 map with single channel
	{
		const int W = 10;
		ImageMapUInt8 map(W, W, 1);
		map.set(0);
		testAssert(map.scalarSampleTiled(0.f, 0.f) == 0.0f);
		testAssert(map.scalarSampleTiled(0.5f, 0.5f) == 0.0f);
		testAssert(map.scalarSampleTiled(0.9999f, 0.9999f) == 0.0f);
		testAssert(map.scalarSampleTiled(1.0001f, 1.00001) == 0.0f);

		testAssert(map.vec3SampleTiled(0.0f, 0.f) == Colour3f(0,0,0));
		testAssert(map.vec3SampleTiled(0.5f, 0.5f) == Colour3f(0,0,0));

		map.set(255);
		testAssert(map.scalarSampleTiled(0.f, 0.f) == 1.0f);
		testAssert(map.scalarSampleTiled(0.5f, 0.5f) == 1.0f);

		testAssert(map.vec3SampleTiled(0.0f, 0.f) == Colour3f(1,1,1));
		testAssert(map.vec3SampleTiled(0.5f, 0.5f) == Colour3f(1,1,1));

		float dv_ds, dv_dt;
		testAssert(map.getDerivs(0.5f, 0.5f, dv_ds, dv_dt) == 1.0f);
		testAssert(dv_ds == 0);
		testAssert(dv_dt == 0);
	}
		
	// Test UInt8 map with 3 channels
	{
		const int W = 10;
		ImageMapUInt8 map(W, W, 3);
		map.set(0);
		testAssert(map.scalarSampleTiled(0.f, 0.f) == 0.0f);
		testAssert(map.scalarSampleTiled(0.5f, 0.5f) == 0.0f);
		testAssert(map.scalarSampleTiled(0.9999f, 0.9999f) == 0.0f);
		testAssert(map.scalarSampleTiled(1.0001f, 1.00001) == 0.0f);

		testAssert(map.vec3SampleTiled(0.0f, 0.f) == Colour3f(0,0,0));
		testAssert(map.vec3SampleTiled(0.5f, 0.5f) == Colour3f(0,0,0));

		map.set(255);
		testAssert(map.scalarSampleTiled(0.f, 0.f) == 1.0f);
		testAssert(map.scalarSampleTiled(0.5f, 0.5f) == 1.0f);

		testAssert(map.vec3SampleTiled(0.0f, 0.f) == Colour3f(1,1,1));
		testAssert(map.vec3SampleTiled(0.5f, 0.5f) == Colour3f(1,1,1));
	}

	// Test float map with single channel
	{
		const int W = 10;
		ImageMapFloat map(W, W, 1);
		map.set(0.f);
		testAssert(map.scalarSampleTiled(0.f, 0.f) == 0.0f);
		testAssert(map.scalarSampleTiled(0.5f, 0.5f) == 0.0f);

		testAssert(map.vec3SampleTiled(0.0f, 0.f) == Colour3f(0,0,0));
		testAssert(map.vec3SampleTiled(0.5f, 0.5f) == Colour3f(0,0,0));

		map.set(1.f);
		testAssert(map.scalarSampleTiled(0.f, 0.f) == 1.0f);
		testAssert(map.scalarSampleTiled(0.5f, 0.5f) == 1.0f);

		testAssert(map.vec3SampleTiled(0.0f, 0.f) == Colour3f(1,1,1));
		testAssert(map.vec3SampleTiled(0.5f, 0.5f) == Colour3f(1,1,1));
	}
		
	// Test float map with 3 channels
	{
		const int W = 10;
		ImageMapFloat map(W, W, 3);
		map.set(0.f);
		testAssert(map.scalarSampleTiled(0.f, 0.f) == 0.0f);
		testAssert(map.scalarSampleTiled(0.5f, 0.5f) == 0.0f);

		testAssert(map.vec3SampleTiled(0.0f, 0.f) == Colour3f(0,0,0));
		testAssert(map.vec3SampleTiled(0.5f, 0.5f) == Colour3f(0,0,0));

		map.set(1.f);
		testAssert(map.scalarSampleTiled(0.f, 0.f) == 1.0f);
		testAssert(map.scalarSampleTiled(0.5f, 0.5f) == 1.0f);

		testAssert(map.vec3SampleTiled(0.0f, 0.f) == Colour3f(1,1,1));
		testAssert(map.vec3SampleTiled(0.5f, 0.5f) == Colour3f(1,1,1));
	}


	// Test that all interpolated reads from a UInt8 map with zero values return zero.
	{
		const int W = 10;
		ImageMapUInt8 map(W, W, 1);
		map.set(0);

		for(float s = -10.f; s <= 10.f; s += 0.14f)
		for(float t = -10.f; t <= 10.f; t += 0.13f)
			testAssert(map.scalarSampleTiled(s, t) == 0.f);
	}

	
	// Do some test with non-constant values
	{
		const int W = 4;
		ImageMapFloat map(W, W, 3);
		for(int y=0; y<W; ++y)
		for(int x=0; x<W; ++x)
		{
			map.getPixel(x, y)[0] = (float)x / (W);
			map.getPixel(x, y)[1] = (float)x / (W);
			map.getPixel(x, y)[2] = (float)x / (W);
		}

		testAssert(map.scalarSampleTiled(0.f, 0.f) == 0.0f);
		testAssert(epsEqual(map.scalarSampleTiled(0.3f, 0.7f), 0.3f));
		testAssert(epsEqual(map.scalarSampleTiled(0.6123f, 0.7f), 0.6123f));
		testAssert(epsEqual(map.scalarSampleTiled(0.999999f, 0.7f), 0.0f));


		float dv_ds, dv_dt;
		testAssert(map.getDerivs(0.5f, 0.5f, dv_ds, dv_dt) == 0.5f); // Value should be 0.5.
		testAssert(epsEqual(dv_ds, 1.f)); // Value is increasing with s.
		testAssert(epsEqual(dv_dt, 0.f));

		testAssert(epsEqual(map.getDerivs(0.43f, 0.2f, dv_ds, dv_dt), 0.43f)); // Value should be 0.43.
		testAssert(epsEqual(dv_ds, 1.f)); // Value is increasing with s.
		testAssert(epsEqual(dv_dt, 0.f));
	}

	// Do some test with non-constant values
	{
		const int W = 10;
		const int H = 20;
		ImageMapFloat map(W, H, 3);
		for(int y=0; y<H; ++y)
		for(int x=0; x<W; ++x)
		{
			const float v = (float)x/W   +   0.5f*(1 - (float)y/H);
			map.getPixel(x, y)[0] = v;
			map.getPixel(x, y)[1] = v;
			map.getPixel(x, y)[2] = v;
		}

		//testAssert(map.scalarSampleTiled(0.f, 0.f) == 0.0f);
		testAssert(epsEqual(map.scalarSampleTiled(0.3f, 0.7f), 0.3f + 0.7f/2));
		testAssert(epsEqual(map.scalarSampleTiled(0.6123f, 0.7f), 0.6123f + 0.7f/2));
		//testAssert(epsEqual(map.scalarSampleTiled(0.999999f, 0.999999f), 0.0f));
		
		// Apart from at the texture borders, where the reads will wrap around, the values should be the simple linear function above,
		// and the derivatives are 1 and 0.5.

		for(float s=0.5001f/W; s<1 - 1.5001f/W; s += 0.01f)
		for(float t=1.5001f/H; t<1 - 0.5001f/H; t += 0.01f)
		{
			float dv_ds, dv_dt;
			testEpsEqual(map.getDerivs(s, t, dv_ds, dv_dt), s + t/2);
			testAssert(epsEqual(dv_ds, 1.0f));
			testAssert(epsEqual(dv_dt, 0.5f));
		}
	}


	// Test hasAlphaChannel(), isAlphaChannelAllWhite()
	{
		const int W = 10;
		const int H = 20;
		ImageMapUInt8 map(W, H, 4);
		for(int y=0; y<H; ++y)
		for(int x=0; x<W; ++x)
		{
			map.getPixel(x, y)[0] = 1;
			map.getPixel(x, y)[1] = 2;
			map.getPixel(x, y)[2] = 3;
			map.getPixel(x, y)[3] = 4;
		}

		testAssert(map.hasAlphaChannel());
		testAssert(!map.isAlphaChannelAllWhite());

		for(int y=0; y<H; ++y)
		for(int x=0; x<W; ++x)
		{
			map.getPixel(x, y)[0] = 1;
			map.getPixel(x, y)[1] = 2;
			map.getPixel(x, y)[2] = 3;
			map.getPixel(x, y)[3] = 255;
		}

		testAssert(map.isAlphaChannelAllWhite());
	}




	
	// Do perf tests
	/*perfTestWithTextureWidth(10);
	perfTestWithTextureWidth(100);
	perfTestWithTextureWidth(1000);
	perfTestWithTextureWidth(10000);*/
	

	/*Plotter::DataSet dataset;
	dataset.key = "elapsed (s)";

	Colour3f sum(0);
	for(double w=2; w<=16384; w *= 1.3)
	{
		const int width = (int)w;

		conPrint("width: " + toString(width));
		double elapsed = 0;
		sum += testTexture(width, elapsed);

		conPrint("elapsed: " + toString(elapsed));

		dataset.points.push_back(Vec2f(logBase2((float)width), (float)elapsed));
	}
	
	conPrint(sum.toVec3().toString());

	std::vector<Plotter::DataSet> datasets;
	datasets.push_back(dataset);
	Plotter::plot(
		"texturing_speed.png",
		"Texture sample time in cycles.",
		"log2(texture width)",
		"elapsed cycles",
		datasets
		);
*/
}


#endif // BUILD_TESTS
