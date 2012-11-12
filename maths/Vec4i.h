/*=====================================================================
Vec4i.h
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2012-11-10 21:02:10 +0000
=====================================================================*/
#pragma once


#include "../maths/SSE.h"
#include "../utils/platform.h"


/*=====================================================================
Vec4i
-------------------

=====================================================================*/
class Vec4i
{
public:
	INDIGO_STRONG_INLINE Vec4i() {}
	INDIGO_STRONG_INLINE explicit Vec4i(int32 x_, int32 y_, int32 z_, int32 w_) : v(_mm_set_epi32(w_, z_, y_, x_)) {}
	INDIGO_STRONG_INLINE Vec4i(__m128i v_) : v(v_) {}
	INDIGO_STRONG_INLINE explicit Vec4i(int32 f) : v(_mm_set1_epi32(f)) {}

	union
	{
		int32 x[4];
		__m128i v;
	};

};
