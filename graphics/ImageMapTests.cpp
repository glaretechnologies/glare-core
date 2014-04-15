/*=====================================================================
ImageMapTests.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-05-22 19:51:52 +0100
=====================================================================*/
#include "ImageMapTests.h"



#ifdef BUILD_TESTS


#include "ImageMap.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "../indigo/globals.h"
#include "../utils/Plotter.h"


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

	double clock_freq = 2.53e9;

	conPrint("\n\n");

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
	conPrint("Over head cycles: " + toString(overhead_cycles));


	timer.reset();

	Colour3f sum(0,0,0);
	xy_sum = 0;
	for(int i=0; i<N; ++i)
	{
		float x = (float)(-45654 + ((i * 87687) % 95465)) * 0.003454f;
		float y = (float)(-67878 + ((i * 5354) % 45445)) * 0.007345f;

		//conPrint(toString(x) + " " + toString(y));
		Colour3f c = m.vec3SampleTiled(x, y);
		sum += c;
		xy_sum += x + y;
	}
	elapsed = timer.elapsed();
	conPrint(sum.toVec3().toString());
	conPrint("xy sum: " + toString(xy_sum));
	conPrint("Elapsed: " + toString(elapsed) + " s");
	const double cycles = (elapsed / (double)N) * clock_freq; // s * cycles s^-1
	//conPrint("cycles: " + toString(cycles));

	elapsed_out = cycles - overhead_cycles;
	return sum;
}


void ImageMapTests::test()
{
	Plotter::DataSet dataset;
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

}


#endif
