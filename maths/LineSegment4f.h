/*=====================================================================
LineSegment4f.h
---------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Vec4f.h"
#include <assert.h>


/*=====================================================================
LineSegment4f
-------------

=====================================================================*/
class LineSegment4f
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	inline LineSegment4f(){}
	inline LineSegment4f(const Vec4f& a, const Vec4f& b);

	Vec4f a;
	Vec4f b;
};


LineSegment4f::LineSegment4f(const Vec4f& a_, const Vec4f& b_)
:	a(a_), b(b_)
{}
