/*=====================================================================
SRGBUtils.h
-----------
Copyright Glare Technologies Limited 2018 - 
=====================================================================*/
#pragma once


#include "Colour4f.h"


/*=====================================================================
SRGBUtils
---------
// See https://en.wikipedia.org/wiki/SRGB for sRGB conversion stuff.
=====================================================================*/

void testSRGBUtils();


float referenceSRGBToLinearSRGB(float c);
float referenceLinearSRGBToNonLinearSRGB(float c);



// See http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html, expression for C_lin_3.
inline Colour4f fastApproxSRGBToLinearSRGB(const Colour4f& c)
{
	Colour4f c2 = c * c;
	return c * c2 * 0.305306011f + c2 * 0.682171111f + c * 0.012522878f;
}


/*
Coefficients of the pow(1/2.4) part chosen using a search that minimises error over [0, 1].
The max error of the pow approximation is ~0.00092.

Max error for the overall fastApproxLinearSRGBToNonLinearSRGB() over [0, 1] is 0.0009690821.

Runs (when called in a loop, so constants hoisted) in about 11 cycles, on my
Intel(R) Core(TM) i7-6700HQ.
(3.3 ns at 3.29Ghz)

This is faster than any function using pow.  pow4f (fast approx pow) from SSE.h seems to take about 27 cycles by itself.
*/
inline Colour4f fastApproxLinearSRGBToNonLinearSRGB(const Colour4f& c)
{
	const Colour4f sqrt_c = sqrt(c);
	const Colour4f nonlinear = c*(c*0.0404024488 + Colour4f(-0.19754737849999998)) + 
		sqrt_c*1.0486722787999998 + sqrt(sqrt_c)*0.1634726509 - Colour4f(0.055);

	const Colour4f linear = c * 12.92f;

	return select(linear, nonlinear, Colour4f(_mm_cmple_ps(c.v, Colour4f(0.0031308f).v)));
}
