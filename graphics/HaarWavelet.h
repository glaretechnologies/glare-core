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
namespace HaarWavelet
{
	typedef float Real;


	void transform(const std::vector<Real>& v, std::vector<Real>& out);
	void inverseTransform(const std::vector<Real>& v, std::vector<Real>& out);
	void transform(Array2d<Real>& a);

	void test();
}
