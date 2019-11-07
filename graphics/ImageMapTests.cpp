/*=====================================================================
ImageMapTests.cpp
-------------------
Copyright Glare Technologies Limited 2014 -
Generated at 2011-05-22 19:51:52 +0100
=====================================================================*/
#include "ImageMapTests.h"


#ifdef BUILD_TESTS


#include "ImageMap.h"
#include "image.h"
#include "PNGDecoder.h"
#include "jpegdecoder.h"
#include "imformatdecoder.h"
#include "Colour4f.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"
#include "../utils/Plotter.h"
#include "../utils/TaskManager.h"


#if 0 // If do perf tests

// See also DXTImageMapTests which has some more up-to-date code.

static void testTexture(int res, double& elapsed_out)
{
	const int W = res;
	const int H = res;
	const int channels = 3;
	ImageMap<unsigned char, UInt8ComponentValueTraits> m(W, H, channels);
	for(int x=0; x<W; ++x)
		for(int y=0; y<H; ++y)
			for(int c=0; c<channels; ++c)
				m.getPixel(x, y)[c] = (unsigned char)(x + y + c);

	double clock_freq = 3.7e9;

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
	conPrint("Over head cycles: " + toString(overhead_cycles));


	timer.reset();

	Colour4f sum(0);
	xy_sum = 0;
	for(int i=0; i<N; ++i)
	{
		float x = (float)(-45654 + ((i * 87687) % 95465)) * 0.003454f;
		float y = (float)(-67878 + ((i * 5354) % 45445)) * 0.007345f;

		//conPrint(toString(x) + " " + toString(y));
		Colour4f c = m.vec3SampleTiled(x, y);
		//float dv_ds, dv_dt;
		//Colour3f c(m.getDerivs(x, y, dv_ds, dv_dt));
		sum += c;
		xy_sum += x + y;
	}
	elapsed = timer.elapsed();
	conPrint(sum.toString());
	conPrint("xy sum: " + toString(xy_sum));
	//conPrint("Elapsed: " + toString(elapsed) + " s");
	const double cycles = (elapsed / (double)N) * clock_freq; // s * cycles s^-1
	//conPrint("cycles: " + toString(cycles));

	elapsed_out = cycles - overhead_cycles;
}


static void perfTestWithTextureWidth(int width)
{
	conPrint("-----------------");
	conPrint("width: " + toString(width));

	double elapsed;
	testTexture(width, elapsed);
	conPrint("elapsed: " + toString(elapsed) + " cycles / sample");
}


#endif // End if do perf tests


#if MAP2D_FILTERING_SUPPORT


static void testResizeMidQuality(int new_w, int new_h)
{
	ImageMapFloat map(100, 100, 3);
	map.set(0.12345f);

	Indigo::TaskManager task_manager;

	const Map2DRef resized_map = map.resizeMidQuality(new_w, new_h, task_manager);

	testAssert(resized_map->getMapWidth() == (unsigned int)new_w && resized_map->getMapHeight() == (unsigned int)new_h);
	testAssert(resized_map.isType<ImageMapFloat>());

	const ImageMapFloatRef resized_map_f = resized_map.downcast<ImageMapFloat>();
	testAssert(resized_map_f->getN() == 3);

	for(int x=0; x<new_w; ++x)
		for(int y=0; y<new_h; ++y)
			for(int c=0; c<3; ++c)
				testEpsEqual(resized_map_f->getPixel(x, y)[c], 0.12345f);
}


#endif // MAP2D_FILTERING_SUPPORT


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

		testAssert(map.vec3SampleTiled(0.0f, 0.f) == Colour4f(0,0,0,0));
		testAssert(map.vec3SampleTiled(0.5f, 0.5f) == Colour4f(0,0,0,0));

		map.set(255);
		testAssert(map.scalarSampleTiled(0.f, 0.f) == 1.0f);
		testAssert(map.scalarSampleTiled(0.5f, 0.5f) == 1.0f);

		testAssert(map.vec3SampleTiled(0.0f, 0.f) == Colour4f(1,1,1,1));
		testAssert(map.vec3SampleTiled(0.5f, 0.5f) == Colour4f(1,1,1,1));

		float dv_ds, dv_dt;
		testAssert(map.getDerivs(0.5f, 0.5f, dv_ds, dv_dt) == 1.0f);
		testAssert(dv_ds == 0);
		testAssert(dv_dt == 0);

		// Test reads with non-finite texture coordinates.  Make sure we don't crash  scalarSampleTiled will probably return a NaN value.
		testAssert(isNAN(map.scalarSampleTiled(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity())));
		testAssert(isNAN(map.scalarSampleTiled(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity())));
		testAssert(isNAN(map.scalarSampleTiled( std::numeric_limits<float>::quiet_NaN(),  std::numeric_limits<float>::quiet_NaN())));
		testAssert(isNAN(map.scalarSampleTiled(-std::numeric_limits<float>::quiet_NaN(), -std::numeric_limits<float>::quiet_NaN())));

		// Test reads with non-finite texture coordinates.  Make sure we don't crash.  vec3SampleTiled will probably return a NaN colour.
		testAssert(map.vec3SampleTiled( std::numeric_limits<float>::infinity(),  std::numeric_limits<float>::infinity()) != Colour4f(1, 2, 3, 4));
		testAssert(map.vec3SampleTiled(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity()) != Colour4f(1, 2, 3, 4));
		testAssert(map.vec3SampleTiled( std::numeric_limits<float>::quiet_NaN(),  std::numeric_limits<float>::quiet_NaN()) != Colour4f(1, 2, 3, 4));
		testAssert(map.vec3SampleTiled(-std::numeric_limits<float>::quiet_NaN(), -std::numeric_limits<float>::quiet_NaN()) != Colour4f(1, 2, 3, 4));
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

		testAssert(map.vec3SampleTiled(0.0f, 0.f) == Colour4f(0,0,0,0));
		testAssert(map.vec3SampleTiled(0.5f, 0.5f) == Colour4f(0,0,0,0));

		map.set(255);
		testAssert(map.scalarSampleTiled(0.f, 0.f) == 1.0f);
		testAssert(map.scalarSampleTiled(0.5f, 0.5f) == 1.0f);

		testAssert(map.vec3SampleTiled(0.0f, 0.f) == Colour4f(1,1,1,0));
		testAssert(map.vec3SampleTiled(0.5f, 0.5f) == Colour4f(1,1,1,0));
	}

	// Test float map with single channel
	{
		const int W = 10;
		ImageMapFloat map(W, W, 1);
		map.set(0.f);
		testAssert(map.scalarSampleTiled(0.f, 0.f) == 0.0f);
		testAssert(map.scalarSampleTiled(0.5f, 0.5f) == 0.0f);

		testAssert(map.vec3SampleTiled(0.0f, 0.f) == Colour4f(0,0,0,0));
		testAssert(map.vec3SampleTiled(0.5f, 0.5f) == Colour4f(0,0,0,0));

		map.set(1.f);
		testAssert(map.scalarSampleTiled(0.f, 0.f) == 1.0f);
		testAssert(map.scalarSampleTiled(0.5f, 0.5f) == 1.0f);

		testAssert(map.vec3SampleTiled(0.0f, 0.f) == Colour4f(1,1,1,1));
		testAssert(map.vec3SampleTiled(0.5f, 0.5f) == Colour4f(1,1,1,1));
	}
		
	// Test float map with 3 channels
	{
		const int W = 10;
		ImageMapFloat map(W, W, 3);
		map.set(0.f);
		testAssert(map.scalarSampleTiled(0.f, 0.f) == 0.0f);
		testAssert(map.scalarSampleTiled(0.5f, 0.5f) == 0.0f);

		testAssert(map.vec3SampleTiled(0.0f, 0.f) == Colour4f(0,0,0,0));
		testAssert(map.vec3SampleTiled(0.5f, 0.5f) == Colour4f(0,0,0,0));

		map.set(1.f);
		testAssert(map.scalarSampleTiled(0.f, 0.f) == 1.0f);
		testAssert(map.scalarSampleTiled(0.5f, 0.5f) == 1.0f);

		testAssert(map.vec3SampleTiled(0.0f, 0.f) == Colour4f(1,1,1,0));
		testAssert(map.vec3SampleTiled(0.5f, 0.5f) == Colour4f(1,1,1,0));
	}

	// Test a 1x1 pixel image.  This can expose bugs with wrapping.
	{
		const int W = 1;
		ImageMapUInt8 map(W, W, 1);
		map.set(128);
		testEpsEqual(map.scalarSampleTiled(0.f, 0.f), (128.f / 255.f));
		testEpsEqual(map.scalarSampleTiled(0.5f, 0.5f), (128.f / 255.f));
		testEpsEqual(map.scalarSampleTiled(0.9999f, 0.9999f), (128.f / 255.f));
		testEpsEqual(map.scalarSampleTiled(1.0001f, 1.00001), (128.f / 255.f));

		float dv_ds, dv_dt;
		testEpsEqual(map.getDerivs(0.5f, 0.5f, dv_ds, dv_dt), (128.f / 255.f));
		testAssert(dv_ds == 0);
		testAssert(dv_dt == 0);
	}

	// Test a 2x2 pixel image.  This can expose bugs with wrapping.
	{
		const int W = 2;
		ImageMapUInt8 map(W, W, 1);
		map.set(128);
		testEpsEqual(map.scalarSampleTiled(0.f, 0.f), (128.f / 255.f));
		testEpsEqual(map.scalarSampleTiled(0.5f, 0.5f), (128.f / 255.f));
		testEpsEqual(map.scalarSampleTiled(0.9999f, 0.9999f), (128.f / 255.f));
		testEpsEqual(map.scalarSampleTiled(1.0001f, 1.00001), (128.f / 255.f));

		float dv_ds, dv_dt;
		testEpsEqual(map.getDerivs(0.5f, 0.5f, dv_ds, dv_dt), (128.f / 255.f));
		testAssert(dv_ds == 0);
		testAssert(dv_dt == 0);
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


	// Test blitToImage()
	{
		ImageMapUInt8 a(5, 5, 3);
		a.set(128);

		ImageMapUInt8 b(10, 10, 3);
		b.set(0);

		a.blitToImage(b, 0, 0);

		for(int y=0; y<10; ++y)
			for(int x=0; x<10; ++x)
				for(int c=0; c<3; ++c)
					testAssert(b.getPixel(x, y)[c] == ((x < 5 && y < 5) ? 128 : 0));

		// Try blitting partially out of bounds of b.
		a.blitToImage(b, 1, 1);
		a.blitToImage(b, -1, -1);

		a.blitToImage(b, 6, 6);
		a.blitToImage(b, -6, -6);

		// Test in bounds blitToImage() with src coords
		a.blitToImage(2, 2, 5, 5, b, 0, 0);

		// Test bounds blitToImage() with out-of-bounds src coords
		a.blitToImage(-2, -2, 5, 5, b, 0, 0);
		a.blitToImage(2, 2, 10, 10, b, 0, 0);
	}


	
	//======================================== Test resizeToImageMapFloat() =======================================
#if MAP2D_FILTERING_SUPPORT
	const bool write_out_result_images = false;

	// Test resizing of a uniform, uint8, 3 component image
	{
		ImageMapUInt8 map(100, 100, 3);
		map.set(123);
		//PNGDecoder::write(map, "flat_image.png");

		bool is_linear;
		const int new_w = 90;
		ImageMapFloatRef resized_map = map.resizeToImageMapFloat(new_w, is_linear);
		testAssert(!is_linear);
		testAssert(resized_map->getWidth() == new_w && resized_map->getHeight() == new_w && resized_map->getN() == 3);

		for(int x=0; x<new_w; ++x)
		for(int y=0; y<new_w; ++y)
		for(int c=0; c<3; ++c)
			testEpsEqual(resized_map->getPixel(x, y)[c], 123.f / 255.f);

		if(write_out_result_images)
		{
			ImageMapUInt8 bmp;
			resized_map->copyToImageMapUInt8(bmp);
			PNGDecoder::write(bmp, "scaled_flat_image.png");
		}
	}

	// Test resizing of a uniform, uint8, 3 component image to a much larger size.
	{
		ImageMapUInt8 map(64, 64, 3);
		map.set(123);

		bool is_linear;
		const int new_w = 300;
		ImageMapFloatRef resized_map = map.resizeToImageMapFloat(new_w, is_linear);
		testAssert(!is_linear);
		testAssert(resized_map->getWidth() == new_w && resized_map->getHeight() == new_w && resized_map->getN() == 3);

		for(int x=0; x<new_w; ++x)
			for(int y=0; y<new_w; ++y)
				for(int c=0; c<3; ++c)
					testEpsEqual(resized_map->getPixel(x, y)[c], 123.f / 255.f);

		if(write_out_result_images)
		{
			ImageMapUInt8 bmp;
			resized_map->copyToImageMapUInt8(bmp);
			PNGDecoder::write(bmp, "upsized_flat_image.png");
		}
	}

	// Test resizing of a uniform, uint8, 1 component image
	{
		ImageMapUInt8 map(100, 100, 1);
		map.set(123);

		bool is_linear;
		const int new_w = 90;
		ImageMapFloatRef resized_map = map.resizeToImageMapFloat(new_w, is_linear);
		testAssert(!is_linear);
		testAssert(resized_map->getWidth() == new_w && resized_map->getHeight() == new_w && resized_map->getN() == 1);

		for(int x=0; x<new_w; ++x)
			for(int y=0; y<new_w; ++y)
				for(int c=0; c<1; ++c)
					testEpsEqual(resized_map->getPixel(x, y)[c], 123.f / 255.f);
	}

	// Test resizing of a uniform, uint16, 3 component image
	{
		ImageMap<uint16, UInt16ComponentValueTraits> map(100, 100, 3);
		map.set(12345);

		bool is_linear;
		const int new_w = 90;
		ImageMapFloatRef resized_map = map.resizeToImageMapFloat(new_w, is_linear);
		testAssert(!is_linear);
		testAssert(resized_map->getWidth() == new_w && resized_map->getHeight() == new_w && resized_map->getN() == 3);

		for(int x=0; x<new_w; ++x)
			for(int y=0; y<new_w; ++y)
				for(int c=0; c<3; ++c)
					testEpsEqual(resized_map->getPixel(x, y)[c], 12345.f / 65535.f);
	}

	// Test resizing of a uniform, float32, 3 component image
	{
		ImageMapFloat map(100, 100, 3);
		map.set(0.12345f);

		bool is_linear;
		const int new_w = 90;
		ImageMapFloatRef resized_map = map.resizeToImageMapFloat(new_w, is_linear);
		testAssert(is_linear);
		testAssert(resized_map->getWidth() == new_w && resized_map->getHeight() == new_w && resized_map->getN() == 3);

		for(int x=0; x<new_w; ++x)
			for(int y=0; y<new_w; ++y)
				for(int c=0; c<3; ++c)
					testEpsEqual(resized_map->getPixel(x, y)[c], 0.12345f);
	}


	//======================================== Test resizeMidQuality() =======================================
	// Test resizing of a uniform, float32, 3 component image
	{
		ImageMapFloat map(100, 100, 3);
		map.set(0.12345f);

		// Test downsize in 2 dims
		testResizeMidQuality(90, 90);

		// Test downsize in 1 dim
		testResizeMidQuality(100, 90);
		testResizeMidQuality(90, 100);

		// Test upsize in 2 dims
		testResizeMidQuality(110, 110);

		// Test upsize in 1 dim
		testResizeMidQuality(110, 100);
		testResizeMidQuality(100, 110);

		// Test upsize in 1 dim, downsize in another
		testResizeMidQuality(90, 110);
		testResizeMidQuality(110, 90);
	}

	// Test resizing of an image loaded from disk
	if(false)
	{
		try
		{
			//Map2DRef map = JPEGDecoder::decode(".", TestUtils::getIndigoTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg");
			Map2DRef map = ImFormatDecoder::decodeImage(".", TestUtils::getIndigoTestReposDir() + "/testfiles/antialias_test3.png");
			//Map2DRef map = ImFormatDecoder::decodeImage(".", TestUtils::getIndigoTestReposDir() + "/testscenes/pentagonal_aperture.png");

			testAssert(map.isType<ImageMapUInt8>());
			ImageMapUInt8Ref map_uint8 = map.downcast<ImageMapUInt8>();

			const int target_res = 100;

			Timer timer;

			bool is_linear;
			ImageMapFloatRef resized_map = map_uint8->resizeToImageMapFloat(target_res, is_linear);
			conPrint("resizeToImageMapFloat() took " + timer.elapsedStringNPlaces(4));

			/*timer.reset();
			ImageMapFloatRef resized_map2 = testResizeToImage(*map_uint8, target_res, is_linear);
			conPrint("testResizeToImage() took     " + timer.elapsedStringNPlaces(4));*/

			ImageMapUInt8 bmp_out;
			resized_map->copyToImageMapUInt8(bmp_out);
			PNGDecoder::write(bmp_out, "scaled_image_old_" + toString(target_res) + ".png");

			//resized_map2->copyToImageMapUInt8(bmp_out);
			//PNGDecoder::write(bmp_out, "scaled_image_test_" + toString(target_res) + ".png");
		}
		catch(ImFormatExcep& e)
		{
			failTest(e.what());
		}
	}


	// Test resizing of an image with resizeMidQuality() loaded from disk
	if(false)
	{
		try
		{
			Map2DRef map = JPEGDecoder::decode(".", TestUtils::getIndigoTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg");
			//Map2DRef map = ImFormatDecoder::decodeImage(".", TestUtils::getIndigoTestReposDir() + "/testfiles/antialias_test3.png");
			//Map2DRef map = ImFormatDecoder::decodeImage(".", TestUtils::getIndigoTestReposDir() + "/testscenes/pentagonal_aperture.png");

			testAssert(map.isType<ImageMapUInt8>());
			ImageMapUInt8Ref map_uint8 = map.downcast<ImageMapUInt8>();

			const int target_res = 100;

			Timer timer;

			bool is_linear;
			ImageMapFloatRef resized_map = map_uint8->resizeToImageMapFloat(target_res, is_linear);
			conPrint("resizeToImageMapFloat() took " + timer.elapsedStringNPlaces(4));

			/*timer.reset();
			ImageMapFloatRef resized_map2 = testResizeToImage(*map_uint8, target_res, is_linear);
			conPrint("testResizeToImage() took     " + timer.elapsedStringNPlaces(4));*/

			ImageMapUInt8 bmp_out;
			resized_map->copyToImageMapUInt8(bmp_out);
			PNGDecoder::write(bmp_out, "scaled_image_old_" + toString(target_res) + ".png");

			//resized_map2->copyToImageMapUInt8(bmp_out);
			//PNGDecoder::write(bmp_out, "scaled_image_test_" + toString(target_res) + ".png");
		}
		catch(ImFormatExcep& e)
		{
			failTest(e.what());
		}
	}
#endif // MAP2D_FILTERING_SUPPORT
	
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
