/*=====================================================================
SRGBUtils.cpp
-------------
Copyright Glare Technologies Limited 2018 - 
=====================================================================*/
#include "SRGBUtils.h"


// See 'Specification of sRGB'  (https://www.color.org/srgb.pdf), B.1. ('Inverting the color component transfer function')
static inline float sRGBComponentToLinear(float x)
{
	if(x <= 0.04045f)
		return x * (1 / 12.92f);
	else
		return std::pow((x + 0.055f) * (1.f / 1.055f), 2.4f);
}

Colour3f toLinearSRGB(const Colour3f& c)
{
	return Colour3f(
		sRGBComponentToLinear(c.r),
		sRGBComponentToLinear(c.g),
		sRGBComponentToLinear(c.b)
	);
}


// See 'Specification of sRGB'  (https://www.color.org/srgb.pdf), '8. Color component transfer function'
static inline float sRGBComponentToNonLinear(float x)
{
	if(x <= 0.0031308f)
		return x * 12.92f;
	else
		return 1.055f * std::pow(x, 1.f / 2.4f) - 0.055f;
}


Colour3f toNonLinearSRGB(const Colour3f& c)
{
	return Colour3f(
		sRGBComponentToNonLinear(c.r),
		sRGBComponentToNonLinear(c.g),
		sRGBComponentToNonLinear(c.b)
	);
}


#if BUILD_TESTS


#include "TestUtils.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"


// From https://en.wikipedia.org/wiki/SRGB
float referenceSRGBToLinearSRGB(float c)
{
	if(c <= 0.04045f)
		return c / 12.92f;
	else
		return std::pow((c + 0.055f) / 1.055f, 2.4f);
}


static Colour4f referenceSRGBToLinearSRGB(const Colour4f& c)
{
	Colour4f res;
	for(int z=0; z<4; ++z)
		res[z] = referenceSRGBToLinearSRGB(c[z]);
	return res;
}


// From https://en.wikipedia.org/wiki/SRGB
float referenceLinearSRGBToNonLinearSRGB(float c)
{
	if(c <= 0.0031308f)
		return c * 12.92f;
	else
		return 1.055f * std::pow(c, 1 / 2.4f) - 0.055f;
}


static Colour4f referenceLinearSRGBToNonLinearSRGB(const Colour4f& c)
{
	Colour4f res;
	for(int z=0; z<4; ++z)
		res[z] = referenceLinearSRGBToNonLinearSRGB(c[z]);
	return res;
}


void testSRGBUtils()
{
	conPrint("testSRGBUtils()");

	//---------------------- Test toNonLinearSRGB, toLinearSRGB -------------------------
	{
		const int N = 1000;
		for(int i=0; i<N; ++i)
		{
			const float x = (float)i / (N - 1);

			const Colour3f col_linear(x, x * 0.5f, x * 0.32f);
			const Colour3f col_nonlinear = toNonLinearSRGB(col_linear);
			const Colour3f col_linear_primed = toLinearSRGB(col_nonlinear);
			testAssert(epsEqual(col_linear.r, col_linear_primed.r));
			testAssert(epsEqual(col_linear.g, col_linear_primed.g));
			testAssert(epsEqual(col_linear.b, col_linear_primed.b));
		}
	}


	//---------------------- Test fastApproxSRGBToLinearSRGB() -------------------------
	testAssert(fastApproxSRGBToLinearSRGB(Colour4f(0.f)) == Colour4f(0.f));
	testAssert(fastApproxSRGBToLinearSRGB(Colour4f(1.f)) == Colour4f(1.f));
	
	// Test error of approximation is not too large
	{
		const int N = 1000;
		float max_error = 0.f;
		for(int i=0; i<N; ++i)
		{
			const float x = (float)i / N;
			const Colour4f ref = referenceSRGBToLinearSRGB(Colour4f(x));
			const Colour4f approx = fastApproxSRGBToLinearSRGB(Colour4f(x));
			const float error = horizontalMax(abs(ref - approx).v);
			max_error = myMax(error, max_error);
		}
		conPrint("max error for fastApproxSRGBToLinearSRGB(): " + toString(max_error));
		testAssert(max_error < 2.0e-3f);
	}


	//---------------------- Test fastApproxLinearSRGBToNonLinearSRGB() -------------------------
	testAssert(fastApproxLinearSRGBToNonLinearSRGB(Colour4f(0.f)) == Colour4f(0.f));

	//const Colour4f nonlin = fastApproxLinearSRGBToNonLinearSRGB(Colour4f(1.f));
	//conPrint(nonlin.toString());
	testAssert(epsEqual(fastApproxLinearSRGBToNonLinearSRGB(Colour4f(1.f)), Colour4f(1.f), 2.0e-7f));
	
	// Test error of approximation is not too large
	{
		const int N = 1000;
		float max_error = 0.f;
		for(int i=0; i<N; ++i)
		{
			const float x = (float)i / N;
			const Colour4f ref = referenceLinearSRGBToNonLinearSRGB(Colour4f(x));
			const Colour4f approx = fastApproxLinearSRGBToNonLinearSRGB(Colour4f(x));
			const float error = horizontalMax(abs(ref - approx).v);
			max_error = myMax(error, max_error);
		}
		conPrint("max error for fastApproxLinearSRGBToNonLinearSRGB(): " + toString(max_error));
		testAssert(max_error < 1.0e-3f);
	}





	//----------------------------- Perf testing -------------------------------
	if(false)
	{
		const int trials = 10000;
		const int N = 100000;
		{
		
			Timer timer;
			Colour4f sum(0.0f);
			double min_elapsed = 1.0e20f;
			for(int t=0; t<trials; ++t)
			{
				for(int i=0; i<N; ++i)
				{
					const float x = (float)i * 0.0000001f;
					sum += fastApproxLinearSRGBToNonLinearSRGB(Colour4f(x));
				}
				min_elapsed = myMin(min_elapsed, timer.elapsed());
			}
			//const double cycles = min_elapsed / (double)N;
			conPrint("fastApproxLinearSRGBToNonLinearSRGB() time: " + toString(min_elapsed * 1.09 / N) + " ns");
			conPrint("sum: " + toString(sum));
		}
	}

	conPrint("testSRGBUtils() done.");
}


#endif