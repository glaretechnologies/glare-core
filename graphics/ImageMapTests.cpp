/*=====================================================================
ImageMapTests.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-05-22 19:51:52 +0100
=====================================================================*/
#include "ImageMapTests.h"



#ifdef BUILD_TESTS


#include "ImageMap.h"
#include "../utils/stringutils.h"
#include "../utils/timer.h"
#include "../indigo/globals.h"


void ImageMapTests::test()
{
	const int W = 4;
	const int H = 4;
	const int channels = 3;
	ImageMap<unsigned char, UInt8ComponentValueTraits> m(W, H, channels);
	for(int x=0; x<W; ++x)
		for(int y=0; y<H; ++y)
			for(int c=0; c<channels; ++c)
				m.getPixel(x, y)[c] = x + y + c;

	Timer timer;
	const int N = 1000000;
	Colour3f sum(0,0,0);
	for(int i=0; i<N; ++i)
	{
		float x = (float)(-45654 + ((i * 87687) % 95465)) * 0.003454f;
		float y = (float)(-67878 + ((i * 5354) % 45445)) * 0.007345f;

		//conPrint(toString(x) + " " + toString(y));
		Colour3f c = m.vec3SampleTiled(x, y);
		sum += c;
	}
	double elapsed = timer.elapsed();
	conPrint(sum.toVec3().toString());
	conPrint("Elapsed: " + toString(elapsed) + " s");
	double clock_freq = 2.6e9;
	const double cycles = (elapsed / (double)N) * clock_freq; // s * cycles s^-1
	conPrint("cycles: " + toString(cycles));
	
}


#endif
