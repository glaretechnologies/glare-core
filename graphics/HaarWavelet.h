/*=====================================================================
HaarWavelet.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-07-27 23:09:37 +0100
=====================================================================*/
#pragma once


#include "../utils/array2d.h"
#include <vector>


/*=====================================================================
HaarWavelet
-------------------

=====================================================================*/
class HaarWavelet
{
public:
	typedef float Real;


	static void transform(const std::vector<Real>& v, std::vector<Real>& out);
	static void inverseTransform(const std::vector<Real>& v, std::vector<Real>& out);
	static void transform(const Array2d<Real>& a, Array2d<Real>& out);

	static void test();
};

