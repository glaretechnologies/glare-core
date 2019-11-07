/*=====================================================================
Hilbert.h
---------
Copyright Glare Technologies Limited 2019 -
Generated at Fri Nov 26 14:59:10 +1300 2010
=====================================================================*/
#pragma once


#include "../maths/vec2.h"
#include <vector>


/*=====================================================================
Hilbert
-------------------
Generates a space-filling Hilbert curve.

To cover a grid with dimensons w*h,
a curve with side of dimensions roundUpToPowerOf2(max(w,h)) is needed.
THis corresponds to a depth of log_2(roundUpToPowerOf2(max(w,h))),
and will generate roundUpToPowerOf2(max(w,h))^2 indices.
=====================================================================*/
class Hilbert
{
public:
	// will generate 4^depth indices, on a square grid with sides of length 2^depth.
	static void generate(int depth, std::vector<Vec2i>& indices_out);

	static void test();
};
