/*=====================================================================
DXTImageMapTests.cpp
--------------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#include "DXTImageMapTests.h"


#if BUILD_TESTS


#include "ImageMap.h"
#include "DXTImageMap.h"
#include "DXTCompression.h"
#include "PNGDecoder.h"
#include "jpegdecoder.h"
#include "imformatdecoder.h"
#include "Colour4f.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "../indigo/globals.h"
#include "../utils/TestUtils.h"
#include "../utils/TaskManager.h"


// Read and decode pixel RGB values from pseudo-random locations in a DXTImageMap.
// Measure speed of doing so.
static void textureReadingSpeedTest(DXTImageMap& map)
{
	const double clock_freq = 3.7e9;
	const float W_f = (float)map.getWidth()  * 0.99999f; // Make sure we don't read out of bounds.
	const float H_f = (float)map.getHeight() * 0.99999f;
	const float xstep = sqrt(2.f);
	const float ystep = sqrt(3.f);
	const int N = 1000000;
	const int trials = 10;
	double min_elapsed = 1.0e10;
	Vec4i sum(0);
	for(int t=0; t<trials; ++t)
	{
		Timer timer;
		float x = 0;
		float y = 0;
		for(int i=0; i<N; ++i)
		{
			x = Maths::fract(x + xstep);
			y = Maths::fract(y + ystep);
			//conPrint(toString(x) + " " + toString(y));
			sum += map.decodePixelRGBColour((size_t)(x * W_f), (size_t)(y * H_f));
		}
		min_elapsed = myMin(min_elapsed, timer.elapsed());
	}
	TestUtils::silentPrint("sum: " + toString(sum[0]));
	const double cycles = (min_elapsed / (double)N) * clock_freq; // s * cycles s^-1
	conPrint("eval pixel DXTImageMap cycles:        " + toString(cycles));
}


static void textureReadingSpeedTestReference(ImageMapUInt8& map)
{
	const double clock_freq = 3.7e9;
	const float W_f = (float)map.getWidth()  * 0.99999f; // Make sure we don't read out of bounds.
	const float H_f = (float)map.getHeight() * 0.99999f;
	const float xstep = sqrt(2.f);
	const float ystep = sqrt(3.f);
	const int N = 1000000;
	const int trials = 10;
	double min_elapsed = 1.0e10;
	Colour4f sum(0);
	for(int t=0; t<trials; ++t)
	{
		Timer timer;
		float x = 0;
		float y = 0;
		for(int i=0; i<N; ++i)
		{
			x = Maths::fract(x + xstep);
			y = Maths::fract(y + ystep);
			sum += map.pixelColour((size_t)(x * W_f), (size_t)(y * H_f));
		}
		min_elapsed = myMin(min_elapsed, timer.elapsed());
	}
	TestUtils::silentPrint("sum: " + toString(sum[0]));
	const double cycles = (min_elapsed / (double)N) * clock_freq; // s * cycles s^-1
	conPrint("eval pixel ImageMapUInt8 cycles:      " + toString(cycles));
}


// Read and decode pixel RGB values from pseudo-random locations in a DXTImageMap.
// Measure speed of doing so.
static void vec3SampleTiledSpeedTest(DXTImageMap& map)
{
	const double clock_freq = 3.7e9;
	const float xstep = sqrt(2.f);
	const float ystep = sqrt(3.f);
	const int N = 1000000;
	const int trials = 10;
	double min_elapsed = 1.0e10;
	Colour4f sum(0);
	for(int t=0; t<trials; ++t)
	{
		Timer timer;
		float x = 0;
		float y = 0;
		for(int i=0; i<N; ++i)
		{
			x = Maths::fract(x + xstep);
			y = Maths::fract(y + ystep);
			//conPrint(toString(x) + " " + toString(y));
			sum += map.vec3SampleTiled(x, y);
		}
		min_elapsed = myMin(min_elapsed, timer.elapsed());
	}
	TestUtils::silentPrint("sum: " + toString(sum));
	const double cycles = (min_elapsed / (double)N) * clock_freq; // s * cycles s^-1
	conPrint("vec3SampleTiled DXTImageMap cycles:   " + toString(cycles));
}


static void vec3SampleTiledSpeedTestReference(ImageMapUInt8& map)
{
	const double clock_freq = 3.7e9;
	const float xstep = sqrt(2.f);
	const float ystep = sqrt(3.f);
	const int N = 1000000;
	const int trials = 10;
	double min_elapsed = 1.0e10;
	Colour4f sum(0);
	for(int t=0; t<trials; ++t)
	{
		Timer timer;
		float x = 0;
		float y = 0;
		for(int i=0; i<N; ++i)
		{
			x = Maths::fract(x + xstep);
			y = Maths::fract(y + ystep);
			sum += map.vec3SampleTiled(x, y);
		}
		min_elapsed = myMin(min_elapsed, timer.elapsed());
	}
	TestUtils::silentPrint("sum: " + toString(sum));
	const double cycles = (min_elapsed / (double)N) * clock_freq; // s * cycles s^-1
	conPrint("vec3SampleTiled ImageMapUInt8 cycles: " + toString(cycles));
}


/*static ImageMapUInt8Ref decompress(DXTImageMap& dxt_image)
{
	ImageMapUInt8Ref im = new ImageMapUInt8(dxt_image.getWidth(), dxt_image.getHeight(), dxt_image.getN());

	for(int x=0; x<dxt_image.getWidth(); ++x)
		for(int y=0; y<dxt_image.getHeight(); ++y)
		{
			const Vec4i rgb = dxt_image.pixelRGBColourBytes(x, y);
			im->getPixel(x, y)[0] = (uint8)rgb[0];
			im->getPixel(x, y)[1] = (uint8)rgb[1];
			im->getPixel(x, y)[2] = (uint8)rgb[2];
			if(dxt_image.getN() == 4)
				im->getPixel(x, y)[3] = (uint8)dxt_image.pixelAlphaByte(x, y);//(uint8)getA(rgb);
		}

	return im;
}*/


static void checkError(int a, int b, int allowed_error)
{
	const int error = abs(a - b);
	if(error > allowed_error)
		failTest("Error of " + toString(error) + " was greater than allowed error of " + toString(allowed_error));
}


/*
Takes an ImageMapUInt8, converts it to a compressed DXTImageMap.
Then iterates over and decodes the pixels in the DXTImageMap and checks the decoded values are within the error tolerance
from the original map.
*/
static void checkCompressionOnImage(glare::TaskManager& task_manager, const ImageMapUInt8& image_map, int allowed_error)
{
	const size_t W = image_map.getWidth();
	const size_t H = image_map.getHeight();
	const size_t N = image_map.getN();
	testAssert(N == 3 || N == 4);

	DXTImageMapRef dxt_image = DXTImageMap::compressImageMap(task_manager, image_map);

	testAssert(dxt_image->getWidth() == W);
	testAssert(dxt_image->getHeight() == H);
	testAssert(dxt_image->getN() == N);

	for(size_t x=0; x<W; ++x)
		for(size_t y=0; y<H; ++y)
		{
			const Vec4i rgba = dxt_image->decodePixelRGBColour(x, y);

			// Check decodePixelRGBColour result vs decodePixelRed result
			const uint32 red = dxt_image->decodePixelRed(x, y);
			testEqual(rgba[0], (int)red);

			checkError((int)image_map.getPixel(x, y)[0], rgba[0], allowed_error);
			checkError((int)image_map.getPixel(x, y)[1], rgba[1], allowed_error);
			checkError((int)image_map.getPixel(x, y)[2], rgba[2], allowed_error);
			if(N == 4)
			{
				const uint32 alpha = dxt_image->decodePixelAlpha(x, y);
				checkError((int)image_map.getPixel(x, y)[3], (int)alpha, allowed_error);
			}
		}

	// Check vec3SampleTiled
	for(float x=0; x<1.0; x+=0.003)
		for(float y=0; y<1.0; y+=0.003)
		{
			//float x = 0.500999510;//TEMP HACK
			const float last_pixel_x = ((float)W-1)/W;
			const float last_pixel_y = ((float)H-1)/H;

			if(x < last_pixel_x && y < last_pixel_y) // Allow bottom right border pixels to differ
			{
				const Colour4f dxt_col = dxt_image->vec3SampleTiled(x, y);
				const Colour4f ref_col = image_map.vec3SampleTiled(x, y);

				const float allowed_err_f = allowed_error / 255.f;
				for(int c=0; c<3; ++c)
					testEpsEqualWithEps(dxt_col[c], ref_col[c], allowed_err_f);

				// Test alpha channel if present
				if(N == 4)
				{
					const float dxt_alpha = dxt_image->sampleSingleChannelTiled(x, y, 3);
					const float ref_alpha = image_map.sampleSingleChannelTiled(x, y, 3);
					testEpsEqualWithEps(dxt_alpha, ref_alpha, allowed_err_f);
				}
			}
		}
}


static void doCheckCompressionWithConstantColour(glare::TaskManager& task_manager, const Vec4i& rgba, int W, int H, int N, int allowed_error)
{
	ImageMapUInt8 image_map(W, H, N);
	for(int x=0; x<W; ++x)
		for(int y=0; y<H; ++y)
		{
			image_map.getPixel(x, y)[0] = (uint8)rgba[0];
			image_map.getPixel(x, y)[1] = (uint8)rgba[1];
			image_map.getPixel(x, y)[2] = (uint8)rgba[2];
			if(N == 4)
				image_map.getPixel(x, y)[3] = (uint8)rgba[3];
		}

	DXTImageMapRef dxt_image = DXTImageMap::compressImageMap(task_manager, image_map);

	checkCompressionOnImage(task_manager, image_map, allowed_error);
}


static void checkCompressionWithConstantColour(glare::TaskManager& task_manager, const Vec4i& rgba, int W, int H, int allowed_error)
{
	doCheckCompressionWithConstantColour(task_manager, rgba, W, H, /*N=*/3, allowed_error);
	doCheckCompressionWithConstantColour(task_manager, rgba, W, H, /*N=*/4, allowed_error);
}


void DXTImageMapTests::test()
{
	conPrint("DXTImageMapTests::test()");

	glare::TaskManager task_manager;

	try
	{
		DXTCompression::init();

		/*
		=================== Check fast division by 3 ===================
		The code below is equivalent to a divide by 3 that is accurate for integers in [0, 765].

		largest value before we can multiply with before we hit the sign bit, considering our max value is 255 * 3 = 765.
		(2 ** 31) / (255 * 3) = 2807168
		so use 2**23 = 2796202
		2**23 / 3.0 = 2796202.6666666665
		rounding up:
		2796203.0 / 2**23 = 0.3333333730697632
		*/
		for(int x=0; x<=255*3; ++x)
		{
			Vec4i v = mulLo(Vec4i(x), Vec4i(2796203)) >> 23;
			testEqual(v[0], x / 3);
		}

		/*
		=================== Check fast division by 7 ===================
		The code below is equivalent to a divide by 7 that is accurate for integers in [0, 1785].

		largest value before we hit the sign bit, considering our max value is 255 * 7 = 1785.
		(2 ** 31) / (255 * 7) = 1203072
		so use 2**20 = 1048576
		2**20 / 7.0 = 149796.57142857142
		rounding up:
		149797.0 / 2**20 = 0.14285755157470703
		whereas
		1.0 / 7.0       =  0.14285714285714285
		*/
		for(int x=0; x<=255*7; ++x)
		{
			Vec4i v = mulLo(Vec4i(x), Vec4i(149797)) >> 20;
			testEqual(v[0], x / 7);
		}



		//{
		//	ImageMapUInt8 image_map(300, 300, 3);
		//	DXTImageMapRef dxt_image = DXTImageMap::compressImageMap(task_manager, image_map);
		//	uint32 v = dxt_image->pixelRGBColourBytes(100, 200);
		//	//printVar(v);
		//}

		//=================== Test constant colour images.  Should have zero error for 0 and 255 values. ===================
		checkCompressionWithConstantColour(task_manager, Vec4i(0, 0, 0, 0), /*W=*/20, /*H=*/10, /*allowed_error=*/0);
		checkCompressionWithConstantColour(task_manager, Vec4i(255, 0, 0, 0), /*W=*/20, /*H=*/10, /*allowed_error=*/0);
		checkCompressionWithConstantColour(task_manager, Vec4i(0, 255, 0, 0), /*W=*/20, /*H=*/10, /*allowed_error=*/0);
		checkCompressionWithConstantColour(task_manager, Vec4i(0, 0, 255, 0), /*W=*/20, /*H=*/10, /*allowed_error=*/0);
		checkCompressionWithConstantColour(task_manager, Vec4i(0, 0, 0, 255), /*W=*/20, /*H=*/10, /*allowed_error=*/0);
		checkCompressionWithConstantColour(task_manager, Vec4i(255, 255, 255, 0), /*W=*/20, /*H=*/10, /*allowed_error=*/0);
		checkCompressionWithConstantColour(task_manager, Vec4i(255, 255, 255, 255), /*W=*/20, /*H=*/10, /*allowed_error=*/0);

		//=================== Test handling of edge blocks ===================
		{
			ImageMapUInt8 image_map(2, 2, 3);

			// Red pixels for x=0, green for x=1.
			// This will fail to have error 0 if we don't do edge padding in the block.

			image_map.getPixel(0, 0)[0] = 255;
			image_map.getPixel(0, 0)[1] = 0;
			image_map.getPixel(0, 0)[2] = 0;

			image_map.getPixel(1, 0)[0] = 0;
			image_map.getPixel(1, 0)[1] = 255;
			image_map.getPixel(1, 0)[2] = 0;

			image_map.getPixel(0, 1)[0] = 255;
			image_map.getPixel(0, 1)[1] = 0;
			image_map.getPixel(0, 1)[2] = 0;

			image_map.getPixel(1, 1)[0] = 0;
			image_map.getPixel(1, 1)[1] = 255;
			image_map.getPixel(1, 1)[2] = 0;

			checkCompressionOnImage(task_manager, image_map, /*allowed_error=*/0);
		}

		// Same as above but with 4 channels.
		{
			ImageMapUInt8 image_map(2, 2, 4);

			image_map.getPixel(0, 0)[0] = 255;
			image_map.getPixel(0, 0)[1] = 0;
			image_map.getPixel(0, 0)[2] = 0;
			image_map.getPixel(0, 0)[3] = 255;

			image_map.getPixel(1, 0)[0] = 0;
			image_map.getPixel(1, 0)[1] = 255;
			image_map.getPixel(1, 0)[2] = 0;
			image_map.getPixel(1, 0)[3] = 255;

			image_map.getPixel(0, 1)[0] = 255;
			image_map.getPixel(0, 1)[1] = 0;
			image_map.getPixel(0, 1)[2] = 0;
			image_map.getPixel(0, 1)[3] = 255;

			image_map.getPixel(1, 1)[0] = 0;
			image_map.getPixel(1, 1)[1] = 255;
			image_map.getPixel(1, 1)[2] = 0;
			image_map.getPixel(1, 1)[3] = 255;

			checkCompressionOnImage(task_manager, image_map, /*allowed_error=*/0);
		}

		{
			ImageMapUInt8 image_map(1, 1, 3);

			image_map.getPixel(0, 0)[0] = 255;
			image_map.getPixel(0, 0)[1] = 0;
			image_map.getPixel(0, 0)[2] = 0;

			checkCompressionOnImage(task_manager, image_map, /*allowed_error=*/0);
		}

		//=================== Test Alpha ===================
		// Test with a gradient of alpha values
		{
			const int W = 8;
			const int H = 8;
			ImageMapUInt8 image_map(W, H, 4);
			for(int x=0; x<W; ++x)
				for(int y=0; y<H; ++y)
				{
					image_map.getPixel(x, y)[0] = 255;
					image_map.getPixel(x, y)[1] = 0;
					image_map.getPixel(x, y)[2] = 0;
					image_map.getPixel(x, y)[3] = (uint8)(x * (256 / H));
				}

			checkCompressionOnImage(task_manager, image_map, /*allowed_error=*/5);
		}

		//=================== Test isAlphaChannelAllWhite ===================
		{
			const int W = 8;
			const int H = 8;
			ImageMapUInt8 image_map(W, H, 4);
			for(int x=0; x<W; ++x)
				for(int y=0; y<H; ++y)
				{
					image_map.getPixel(x, y)[0] = 100;
					image_map.getPixel(x, y)[1] = 100;
					image_map.getPixel(x, y)[2] = 100;
					image_map.getPixel(x, y)[3] = 255;
				}

			testAssert(image_map.isAlphaChannelAllWhite());
			DXTImageMapRef dxt_image = DXTImageMap::compressImageMap(task_manager, image_map);
			testAssert(dxt_image->isAlphaChannelAllWhite());

			image_map.getPixel(0, 0)[3] = 254;
			dxt_image = DXTImageMap::compressImageMap(task_manager, image_map);
			testAssert(!dxt_image->isAlphaChannelAllWhite());
		}

		//=================== Do performance tests ===================
		if(true)
		{
			//const int W = 4096;
			//const int H = 4096;
			const int W = 32;
			const int H = 32;
			const int N = 4;
			ImageMapUInt8 image_map(W, H, N);
			for(int x=0; x<W; ++x)
				for(int y=0; y<H; ++y)
				{
					image_map.getPixel(x, y)[0] = (uint8)x;
					image_map.getPixel(x, y)[1] = (uint8)y;
					image_map.getPixel(x, y)[2] = (uint8)(x+y);
					image_map.getPixel(x, y)[3] = (uint8)(255);
				}

			DXTImageMapRef dxt_image = DXTImageMap::compressImageMap(task_manager, image_map);
			
			conPrint("Perf tests, W: " + toString(W) + ", H: " + toString(H) + ", N: " + toString(N));
			conPrint("------------------------");
			textureReadingSpeedTest(*dxt_image);

			textureReadingSpeedTestReference(image_map);

			vec3SampleTiledSpeedTest(*dxt_image);

			vec3SampleTiledSpeedTestReference(image_map);
		}

		
		//=================== Load an image off disk and compress it. ===================
		if(false)
		{
			Reference<Map2D> map = JPEGDecoder::decode(".", TestUtils::getTestReposDir() + "/testfiles/italy_bolsena_flag_flowers_stairs_01.jpg");
			//Reference<Map2D> map = PNGDecoder::decode(TestUtils::getTestReposDir() + "/testfiles/pngs/dof_test_with_alpha.png");
			//Reference<Map2D> map = PNGDecoder::decode(TestUtils::getTestReposDir() + "/testscenes/leaf_with_alpha.png");

			testAssert(dynamic_cast<ImageMapUInt8*>(map.ptr()) != NULL);
			ImageMapUInt8& image_map = *map.downcast<ImageMapUInt8>();

			DXTImageMapRef dxt_image = DXTImageMap::compressImageMap(task_manager, image_map);
			
			//ImageMapUInt8Ref decompressed = decompress(*dxt_image);
			//PNGDecoder::write(*decompressed, "edge_compressed.png");

			// Test with alpha channel
			/*{
				Reference<Map2D> map_with_alpha = PNGDecoder::decode(TestUtils::getTestReposDir() + "/testscenes/leaf_with_alpha.png");
				testAssert(dynamic_cast<ImageMapUInt8*>(map_with_alpha.ptr()));
				ImageMapUInt8& image_map_with_alpha = *map_with_alpha.downcast<ImageMapUInt8>();

				DXTImageMapRef dxt_image_with_alpha = DXTImageMap::compressImageMap(task_manager, image_map_with_alpha);

				conPrint("Test with alpha:");
				textureReadingSpeedTest(*dxt_image_with_alpha);
			}*/


			// Test vec3SampleTiled with reference image
			/*{
				const int scale = 4;
				ImageMapUInt8 bmp(dxt_image->getWidth() * scale, dxt_image->getHeight() * scale, dxt_image->getN());

				for(int x=0; x<bmp.getWidth(); ++x)
					for(int y=0; y<bmp.getHeight(); ++y)
					{
						const Colour4f col = image_map.vec3SampleTiled((float)x / bmp.getWidth(), -(float)y / bmp.getHeight());
						bmp.getPixel(x, y)[0] = (uint8)(col[0] * 255.f);
						bmp.getPixel(x, y)[1] = (uint8)(col[1] * 255.f);
						bmp.getPixel(x, y)[2] = (uint8)(col[2] * 255.f);
						if(dxt_image->getN() == 4)
							bmp.getPixel(x, y)[3] = (uint8)(col[3] * 255.f);
					}

				PNGDecoder::write(bmp, "reference_vec3SampleTiled.png");
			}
			*/
			// Test vec3SampleTiled with compressed image
			/*{
				const int scale = 4;
				ImageMapUInt8 bmp(dxt_image->getWidth() * scale, dxt_image->getHeight() * scale, dxt_image->getN());

				for(int x=0; x<bmp.getWidth(); ++x)
					for(int y=0; y<bmp.getHeight(); ++y)
					{
						const Colour4f col = dxt_image->vec3SampleTiled((float)x / bmp.getWidth(), -(float)y / bmp.getHeight());
						bmp.getPixel(x, y)[0] = (uint8)(col[0] * 255.f);
						bmp.getPixel(x, y)[1] = (uint8)(col[1] * 255.f);
						bmp.getPixel(x, y)[2] = (uint8)(col[2] * 255.f);
						if(dxt_image->getN() == 4)
							bmp.getPixel(x, y)[3] = (uint8)(col[3] * 255.f);
					}

				PNGDecoder::write(bmp, "compressed_vec3SampleTiled.png");
			}*/
		}

		conPrint("DXTImageMapTests::test() done.");
	}
	catch(ImFormatExcep& e)
	{
		failTest(e.what());
	}
}


#endif // BUILD_TESTS
