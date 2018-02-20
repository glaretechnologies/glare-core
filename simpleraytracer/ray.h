/*=====================================================================
Copyright Glare Technologies Limited 2013 -
=====================================================================*/
#pragma once


#include "../maths/Vec4f.h"
#include "../maths/mathstypes.h"
#include "../maths/SSE.h"


// Modified from o:\indigo\trunk\embree\common\simd\smmintrin_emu.h
INDIGO_STRONG_INLINE __m128 glare_mm_blendv_ps(__m128 value, __m128 input, __m128 mask) { 
    return _mm_or_ps(_mm_and_ps(mask, input), _mm_andnot_ps(mask, value)); 
}

INDIGO_STRONG_INLINE __m128 glare_set_if_zero(const Vec4f& a, const Vec4f& b) {
    return glare_mm_blendv_ps(a.v, b.v, _mm_cmpeq_ps (a.v, _mm_setzero_ps()));
}


/*=====================================================================
Ray
---
Need not be normalised.
=====================================================================*/
SSE_CLASS_ALIGN Ray
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	INDIGO_STRONG_INLINE Ray() {}

	INDIGO_STRONG_INLINE Ray(const Vec4f& startpos_, const Vec4f& unitdir_, float min_t_, float max_t_, bool shadow_trace_ = true)
	:	startpos_f(startpos_),
		unitdir_f(unitdir_),
		min_t(min_t_),
		max_t(max_t_),
		shadow_trace(shadow_trace_)
	{
		assert(epsEqual(startpos_.x[3], 1.0f));
		assert(epsEqual(unitdir_.x[3], 0.0f));
		assert(SSE::isSSEAligned(this));
		assert(max_t_ >= min_t_);

		// Compute reciprocal ray direction, but avoid infinities.  Code modified from Embree Ray class.
		recip_unitdir_f = Vec4f(_mm_div_ps(
			_mm_load_ps(one_4vec),
			glare_set_if_zero(unitdir_, Vec4f(std::numeric_limits<float>::min())) // if unitdir is zero, use std::numeric_limits<float>::min() instead.
		));
	}

	INDIGO_STRONG_INLINE const Vec4f& startPos() const { return startpos_f; }
	INDIGO_STRONG_INLINE const Vec4f& unitDir() const { return unitdir_f; }

	INDIGO_STRONG_INLINE const Vec4f& startPosF() const { return startpos_f; }
	INDIGO_STRONG_INLINE const Vec4f& unitDirF() const { return unitdir_f; }

	INDIGO_STRONG_INLINE const Vec4f& getRecipRayDirF() const { return recip_unitdir_f; }

	INDIGO_STRONG_INLINE const Vec4f pointf(const float t) const { return startpos_f + (unitdir_f * t); }

	INDIGO_STRONG_INLINE static float computeMinT(float origin_error, float cos_theta)
	{
		return 4 * origin_error * myMin(20.0f, 1 / cos_theta);
	}

	INDIGO_STRONG_INLINE const float minT() const { return min_t; }
	INDIGO_STRONG_INLINE const float maxT() const { return max_t; }


	INDIGO_STRONG_INLINE void setStartPos(const Vec4f& s) { startpos_f = s; }
	INDIGO_STRONG_INLINE void setMinT(float min_t_) { min_t = min_t_; }
	INDIGO_STRONG_INLINE void setMaxT(float max_t_) { max_t = max_t_; }

private:
	Vec4f startpos_f;
	Vec4f unitdir_f;
	Vec4f recip_unitdir_f;
	float min_t;
	float max_t;
public:
	bool shadow_trace;
};
