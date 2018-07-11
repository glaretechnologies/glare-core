/*=====================================================================
aabbox.h
--------
File created by ClassTemplate on Thu Nov 18 03:48:29 2004
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "../maths/Vec4f.h"
#include "../maths/SSE.h"
#include "../maths/Matrix4f.h"
#include "../maths/mathstypes.h"
#include "../utils/Platform.h"
#include <limits>
#include <string>


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

	INDIGO_STRONG_INLINE AABBox& operator = (const AABBox& rhs) { min_ = rhs.min_; max_ = rhs.max_; return *this; }
	inline bool operator == (const AABBox& rhs) const;

	inline bool contains(const Vec4f& p) const;

	INDIGO_STRONG_INLINE void enlargeToHoldPoint(const Vec4f& p);
	INDIGO_STRONG_INLINE void enlargeToHoldAABBox(const AABBox& aabb);
	inline bool containsAABBox(const AABBox& aabb) const;

	inline int disjoint(const AABBox& other) const; // Returns a non-zero value if AABBs are disjoint.
	inline bool intersectsAABB(const AABBox& other) const;

	INDIGO_STRONG_INLINE int isEmpty() const; // Returns non-zero if this AABB is empty (e.g. if it has an upper bound < lower bound)


	INDIGO_STRONG_INLINE int rayAABBTrace(const Vec4f& raystartpos, const Vec4f& recip_unitraydir,  float& near_hitd_out, float& far_hitd_out) const;

	INDIGO_STRONG_INLINE void rayAABBTrace(const __m128 pos, const __m128 inv_dir, __m128& near_t_out, __m128& far_t_out) const;

	static AABBox emptyAABBox(); // Returns empty AABBox, (inf, -inf) as (min, max) resp.

	inline float volume() const;
	inline float getSurfaceArea() const;
	inline float getHalfSurfaceArea() const;
	bool invariant() const;
	static void test();

	inline float axisLength(unsigned int axis) const { return max_.x[axis] - min_.x[axis]; }
	inline unsigned int longestAxis() const;

	INDIGO_STRONG_INLINE const Vec4f centroid() const;

	// Get the world space AABB, which encloses object-space AABB corners transformed to world space.
	inline AABBox transformedAABB(const Matrix4f& matrix) const;
	inline AABBox transformedAABBFast(const Matrix4f& matrix) const; // Faster, but with possible precision issues.


	const std::string toString() const;
	const std::string toStringNSigFigs(int n) const;

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
	return allTrue(parallelAnd(parallelGreaterEqual(p, min_), parallelLessEqual(p, max_)));
}


bool AABBox::containsAABBox(const AABBox& other) const
{
	return allTrue(parallelAnd(parallelGreaterEqual(max_, other.max_), parallelLessEqual(min_, other.min_)));
}


int AABBox::disjoint(const AABBox& other) const // Returns a non-zero value if AABBs are disjoint.
{
	// Compare the AABB bounds along each axis in parallel.
	__m128 lower_separation = _mm_cmplt_ps(max_.v, other.min_.v); // [max.x < other.min.x, max.y < other.min.y, ...]
	__m128 upper_separation = _mm_cmplt_ps(other.max_.v, min_.v) ;// [other.max.x < min.x, other.max.y < min.y, ...]
	__m128 either = _mm_or_ps(lower_separation, upper_separation); // Will have a bit set if there is a separation on any axis
	return _mm_movemask_ps(either); // Creates a 4-bit mask from the most significant bits
}


int AABBox::isEmpty() const
{
	__m128 separation = _mm_cmplt_ps(max_.v, min_.v); // [max.x < min.x, max.y < min.y, ...]
	return _mm_movemask_ps(separation); // Creates a 4-bit mask from the most significant bits
}


bool AABBox::intersectsAABB(const AABBox& other) const
{
	return disjoint(other) == 0;
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


INDIGO_STRONG_INLINE js::AABBox intersection(const js::AABBox& a, const js::AABBox& b)
{
	return js::AABBox(max(a.min_, b.min_), min(a.max_, b.max_));
}


INDIGO_STRONG_INLINE js::AABBox AABBUnion(const js::AABBox& a, const js::AABBox& b)
{
	return js::AABBox(min(a.min_, b.min_), max(a.max_, b.max_));
}


float AABBox::volume() const
{
	const Vec4f diff(max_ - min_);

	return diff[0] * diff[1] * diff[2];
}


float AABBox::getSurfaceArea() const
{
	const Vec4f diff(max_ - min_);
	const Vec4f res = mul(diff, swizzle<2, 0, 1, 3>(diff)); // = diff.x[0]*diff.x[2] + diff.x[1]*diff.x[0] + diff.x[2]*diff.x[1]
	return 2 * (res[0] + res[1] + res[2]);
}


float AABBox::getHalfSurfaceArea() const
{
	const Vec4f diff(max_ - min_);
	const Vec4f res = mul(diff, swizzle<2, 0, 1, 3>(diff)); // = diff.x[0]*diff.x[2] + diff.x[1]*diff.x[0] + diff.x[2]*diff.x[1]
	return res[0] + res[1] + res[2];
}


AABBox AABBox::transformedAABB(const Matrix4f& M) const
{
	// Factor out repeated multiplications of the columns of M with the components of min and max:
	const Vec4f c0 = M.getColumn(0);
	const Vec4f c1 = M.getColumn(1);
	const Vec4f c2 = M.getColumn(2);
	const Vec4f c3 = M.getColumn(3);
	const Vec4f c0_min_x = mul(c0, copyToAll<0>(min_));
	const Vec4f c0_max_x = mul(c0, copyToAll<0>(max_));
	const Vec4f c1_min_y = mul(c1, copyToAll<1>(min_));
	const Vec4f c1_max_y = mul(c1, copyToAll<1>(max_));
	const Vec4f c2_min_z = mul(c2, copyToAll<2>(min_));
	const Vec4f c2_max_z = mul(c2, copyToAll<2>(max_));

	// Note that c3 could just be added to the resulting bounds, however
	// due to rounding differences that would result in differences with the reference code,
	// and differences to the transforms of points on the bounds.
	// Basically we want to be consistent with what Matrix4f * vec4f multiplication does.
	const Vec4f M_min       = (c0_min_x + c1_min_y) + (c2_min_z + c3);
	js::AABBox M_bbox(M_min, M_min);
	M_bbox.enlargeToHoldPoint((c0_min_x + c1_min_y) + (c2_max_z + c3));
	M_bbox.enlargeToHoldPoint((c0_min_x + c1_max_y) + (c2_min_z + c3));
	M_bbox.enlargeToHoldPoint((c0_min_x + c1_max_y) + (c2_max_z + c3));
	M_bbox.enlargeToHoldPoint((c0_max_x + c1_min_y) + (c2_min_z + c3));
	M_bbox.enlargeToHoldPoint((c0_max_x + c1_min_y) + (c2_max_z + c3));
	M_bbox.enlargeToHoldPoint((c0_max_x + c1_max_y) + (c2_min_z + c3));
	M_bbox.enlargeToHoldPoint((c0_max_x + c1_max_y) + (c2_max_z + c3));
	return M_bbox;
}


// Faster, but with possible precision issues due to subtraction to compute 'diff'.
AABBox AABBox::transformedAABBFast(const Matrix4f& M) const 
{
	const Vec4f M_min = M.mul3Point(min_);
	const Vec4f diff = max_ - min_;

	// Transform edge vectors by M.  Because the edge vectors are axis-aligned, they only have one non-zero component, 
	// so they just pick out a single column of M.  
	// Therefore we can just do a single 4-vector multiply instead of a full matrix mult.
	const Vec4f M_e0 = mul(M.getColumn(0), copyToAll<0>(diff)); // e0 = (max_x - min_x, 0, 0)
	const Vec4f M_e1 = mul(M.getColumn(1), copyToAll<1>(diff)); // e1 = (0, max_y - min_y, 0)
	const Vec4f M_e2 = mul(M.getColumn(2), copyToAll<2>(diff)); // e2 = (0, 0, max_z - min_z)

	js::AABBox M_bbox(M_min, M_min);
	M_bbox.enlargeToHoldPoint(M_min               + M_e2);
	M_bbox.enlargeToHoldPoint(M_min        + M_e1       );
	M_bbox.enlargeToHoldPoint(M_min        + M_e1 + M_e2);
	M_bbox.enlargeToHoldPoint(M_min + M_e0              );
	M_bbox.enlargeToHoldPoint(M_min + M_e0        + M_e2);
	M_bbox.enlargeToHoldPoint(M_min + M_e0 + M_e1       );
	M_bbox.enlargeToHoldPoint(M_min + M_e0 + M_e1 + M_e2);
	return M_bbox;
}


} //end namespace js
