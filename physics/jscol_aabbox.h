/*=====================================================================
aabbox.h
--------
File created by ClassTemplate on Thu Nov 18 03:48:29 2004
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "../maths/Vec4f.h"
#include "../maths/SSE.h"
#include "../maths/mathstypes.h"
#include "../utils/Platform.h"
#include <limits>


namespace js
{


/*=====================================================================
AABBox
------
Axis aligned bounding box.
Single-precision floating point.
Must be 16-byte aligned.
=====================================================================*/
SSE_CLASS_ALIGN AABBox
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	INDIGO_STRONG_INLINE AABBox();
	INDIGO_STRONG_INLINE AABBox(const Vec4f& _min, const Vec4f& _max);

	inline bool operator == (const AABBox& rhs) const;

	inline bool contains(const Vec4f& p) const;

	INDIGO_STRONG_INLINE void enlargeToHoldPoint(const Vec4f& p);
	INDIGO_STRONG_INLINE void enlargeToHoldAABBox(const AABBox& aabb);
	inline bool containsAABBox(const AABBox& aabb) const;


	INDIGO_STRONG_INLINE int rayAABBTrace(const Vec4f& raystartpos, const Vec4f& recip_unitraydir,  float& near_hitd_out, float& far_hitd_out) const;

	INDIGO_STRONG_INLINE void rayAABBTrace(const __m128 pos, const __m128 inv_dir, __m128& near_t_out, __m128& far_t_out) const;

	static AABBox emptyAABBox(); // Returns empty AABBox, (inf, -inf) as (min, max) resp.

	inline float volume() const;
	inline float getSurfaceArea() const;
	bool invariant() const;
	static void test();

	inline float axisLength(unsigned int axis) const { return max_.x[axis] - min_.x[axis]; }
	inline unsigned int longestAxis() const;

	INDIGO_STRONG_INLINE const Vec4f centroid() const;

	Vec4f min_;
	Vec4f max_;
};


AABBox::AABBox()
{
	static_assert(sizeof(AABBox) == 32, "sizeof(AABBox) == 32");
}


AABBox::AABBox(const Vec4f& _min, const Vec4f& _max)
:	min_(_min), 
	max_(_max)
{
}


void AABBox::enlargeToHoldPoint(const Vec4f& p)
{
	min_.v = _mm_min_ps(min_.v, p.v);
	max_.v = _mm_max_ps(max_.v, p.v);
}


void AABBox::enlargeToHoldAABBox(const AABBox& aabb)
{
	min_.v = _mm_min_ps(min_.v, aabb.min_.v);
	max_.v = _mm_max_ps(max_.v, aabb.max_.v);
}


bool AABBox::contains(const Vec4f& p) const
{
	return	(p.x[0] >= min_.x[0]) && (p.x[0] <= max_.x[0]) &&
			(p.x[1] >= min_.x[1]) && (p.x[1] <= max_.x[1]) &&
			(p.x[2] >= min_.x[2]) && (p.x[2] <= max_.x[2]);
}


bool AABBox::containsAABBox(const AABBox& other) const
{
	return 
		max_.x[0] >= other.max_.x[0] && 
		max_.x[1] >= other.max_.x[1] && 
		max_.x[2] >= other.max_.x[2] && 
		min_.x[0] <= other.min_.x[0] && 
		min_.x[1] <= other.min_.x[1] && 
		min_.x[2] <= other.min_.x[2];
}


// From http://www.flipcode.com/archives/SSE_RayBox_Intersection_Test.shtml

const float SSE_ALIGN
	ps_cst_plus_inf[4]	= {  std::numeric_limits<float>::infinity(),  std::numeric_limits<float>::infinity(),  std::numeric_limits<float>::infinity(),  std::numeric_limits<float>::infinity() },
	ps_cst_minus_inf[4]	= { -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity() };

#define rotatelps(ps)		_mm_shuffle_ps((ps),(ps), 0x39)	// a,b,c,d -> b,c,d,a
#define muxhps(low,high)	_mm_movehl_ps((low),(high))	// low{a,b,c,d}|high{e,f,g,h} = {c,d,g,h}
#define minss			_mm_min_ss
#define maxss			_mm_max_ss


int AABBox::rayAABBTrace(const Vec4f& raystartpos, const Vec4f& recip_unitraydir, 
						  float& near_hitd_out, float& far_hitd_out) const
{
	assertSSEAligned(&raystartpos);
	assertSSEAligned(&recip_unitraydir);
	assertSSEAligned(&min_);
	assertSSEAligned(&max_);

	// you may already have those values hanging around somewhere
	const SSE4Vec
		plus_inf	= load4Vec(ps_cst_plus_inf),
		minus_inf	= load4Vec(ps_cst_minus_inf);
	
	// Load using _mm_set1_ps, to avoid having ps_cst_plus_inf memory in every translaton unit that includes jscol_aabbox.h
	//const SSE4Vec plus_inf = _mm_set1_ps(std::numeric_limits<float>::infinity());
	//const SSE4Vec minus_inf = _mm_set1_ps(-std::numeric_limits<float>::infinity());

	// use whatever's apropriate to load.
	const SSE4Vec
		box_min	= min_.v, // load4Vec(&min_.x),
		box_max	= max_.v, // load4Vec(&max_.x),
		pos	= raystartpos.v, // load4Vec(&raystartpos.x),
		inv_dir	= recip_unitraydir.v; // load4Vec(&recip_unitraydir.x);

	// use a div if inverted directions aren't available
	const SSE4Vec l1 = mult4Vec(sub4Vec(box_min, pos), inv_dir);
	const SSE4Vec l2 = mult4Vec(sub4Vec(box_max, pos), inv_dir);

	// the order we use for those min/max is vital to filter out
	// NaNs that happens when an inv_dir is +/- inf and
	// (box_min - pos) is 0. inf * 0 = NaN

	// Nick's notes:
	// "Note that if only one value is a NaN for this instruction, the source operand (second operand) value 
	// (either NaN or valid floating-point value) is written to the result."
	// So if l1 is a NaN, then filtered_l1a := +Inf
	const SSE4Vec filtered_l1a = min4Vec(l1, plus_inf);
	const SSE4Vec filtered_l2a = min4Vec(l2, plus_inf);

	const SSE4Vec filtered_l1b = max4Vec(l1, minus_inf);
	const SSE4Vec filtered_l2b = max4Vec(l2, minus_inf);

	// now that we're back on our feet, test those slabs.
	SSE4Vec lmax = max4Vec(filtered_l1a, filtered_l2a);
	SSE4Vec lmin = min4Vec(filtered_l1b, filtered_l2b);

	// unfold back. try to hide the latency of the shufps & co.
	const SSE4Vec lmax0 = rotatelps(lmax);
	const SSE4Vec lmin0 = rotatelps(lmin);
	lmax = minss(lmax, lmax0);
	lmin = maxss(lmin, lmin0);

	const SSE4Vec lmax1 = muxhps(lmax,lmax);
	const SSE4Vec lmin1 = muxhps(lmin,lmin);
	lmax = minss(lmax, lmax1);
	lmin = maxss(lmin, lmin1);

	const int ret = _mm_comige_ss(lmax, _mm_setzero_ps()) & _mm_comige_ss(lmax,lmin);

	storeLowWord(lmin, &near_hitd_out);
	storeLowWord(lmax, &far_hitd_out);

	return ret;
}


void AABBox::rayAABBTrace(const __m128 pos , const __m128 inv_dir, __m128& near_t_out, __m128& far_t_out) const
{
	assertSSEAligned(&min_);
	assertSSEAligned(&max_);

	// use whatever's apropriate to load.
	const SSE4Vec
		box_min	= min_.v, // load4Vec(&min_.x),
		box_max	= max_.v; // load4Vec(&max_.x);

	// use a div if inverted directions aren't available
	const SSE4Vec l1 = mult4Vec(sub4Vec(box_min, pos), inv_dir); // l1.x = (box_min.x - pos.x) / dir.x [distances along ray to slab minimums]
	const SSE4Vec l2 = mult4Vec(sub4Vec(box_max, pos), inv_dir); // l1.x = (box_max.x - pos.x) / dir.x [distances along ray to slab maximums]

	// now that we're back on our feet, test those slabs.
	SSE4Vec lmax = max4Vec(l1, l2); //max4Vec(filtered_l1a, filtered_l2a);
	SSE4Vec lmin = min4Vec(l1, l2); //min4Vec(filtered_l1b, filtered_l2b);

	// unfold back. try to hide the latency of the shufps & co.
	const SSE4Vec lmax0 = rotatelps(lmax);
	const SSE4Vec lmin0 = rotatelps(lmin);
	lmax = minss(lmax, lmax0);
	lmin = maxss(lmin, lmin0);

	const SSE4Vec lmax1 = muxhps(lmax,lmax);
	const SSE4Vec lmin1 = muxhps(lmin,lmin);
	lmax = minss(lmax, lmax1);
	lmin = maxss(lmin, lmin1);

	near_t_out = lmin;
	far_t_out = lmax;
}


unsigned int AABBox::longestAxis() const
{
	if(axisLength(0) > axisLength(1))
		return axisLength(0) > axisLength(2) ? 0 : 2;
	else //else 1 >= 0
		return axisLength(1) > axisLength(2) ? 1 : 2;
}


inline const Vec4f AABBox::centroid() const
{
	return Vec4f(min_ + max_) * 0.5f;
}


bool AABBox::operator == (const AABBox& rhs) const
{
	return min_ == rhs.min_ && max_ == rhs.max_;
}


inline bool epsEqual(const AABBox& a, const AABBox& b, float eps = (float)NICKMATHS_EPSILON)
{
	return epsEqual(a.min_, b.min_) && epsEqual(a.max_, b.max_);
}


float AABBox::volume() const
{
	const Vec4f diff(max_ - min_);

	return diff[0] * diff[1] * diff[2];
}


float AABBox::getSurfaceArea() const
{
	const Vec4f diff(max_ - min_);

	return 2.0f * (diff.x[0]*diff.x[1] + diff.x[0]*diff.x[2] + diff.x[1]*diff.x[2]);
}


} //end namespace js
