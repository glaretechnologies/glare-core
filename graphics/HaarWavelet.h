/*=====================================================================
HaarWavelet.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-07-27 23:09:37 +0100
=====================================================================*/
#pragma once


#include "../utils/Array2D.h"
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
	static void transform(const Array2D<Real>& a, Array2D<Real>& out);

	static void test();
};

