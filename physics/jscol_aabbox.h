/*=====================================================================
jscol_aabbox.h
--------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "../simpleraytracer/ray.h"
#include "../maths/Matrix4f.h"
#include <limits>
#include <string>


namespace js
{


/*=====================================================================
AABBox
------
Axis-aligned bounding box.
Single-precision floating point.
=====================================================================*/
class AABBox
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	GLARE_STRONG_INLINE AABBox();
	GLARE_STRONG_INLINE AABBox(const Vec4f& _min, const Vec4f& _max);

	GLARE_STRONG_INLINE AABBox& operator = (const AABBox& rhs) { min_ = rhs.min_; max_ = rhs.max_; return *this; }
	inline bool operator == (const AABBox& rhs) const;
	inline bool operator != (const AABBox& rhs) const;

	inline bool contains(const Vec4f& p) const;

	GLARE_STRONG_INLINE void enlargeToHoldPoint(const Vec4f& p);
	GLARE_STRONG_INLINE void enlargeToHoldAABBox(const AABBox& aabb);
	inline bool containsAABBox(const AABBox& aabb) const;

	inline int disjoint(const AABBox& other) const; // Returns a non-zero value if AABBs are disjoint.
	inline bool intersectsAABB(const AABBox& other) const;

	GLARE_STRONG_INLINE int isEmpty() const; // Returns non-zero if this AABB is empty (e.g. if it has an upper bound < lower bound)

	GLARE_STRONG_INLINE int rayAABBTrace(const Vec4f& raystartpos, const Vec4f& recip_unitraydir,  float& near_hitd_out, float& far_hitd_out) const;

	// Returns non-zero if the ray intersected this AABB.  If the ray intersects the AABB, updates ray min_t and max_t, clipping out segments of the ray outside the AABB.
	inline int traceRay(Ray& ray) const;

	inline static AABBox emptyAABBox(); // Returns empty AABBox, (inf, -inf) as (min, max) resp.

	inline float volume() const;
	inline float getSurfaceArea() const;
	inline float getHalfSurfaceArea() const;

	inline float axisLength(unsigned int axis) const { return max_.x[axis] - min_.x[axis]; }
	inline unsigned int longestAxis() const;
	inline float longestLength() const; // Length along longest axis

	GLARE_STRONG_INLINE const Vec4f centroid() const;

	// Get the AABB which encloses the AABB corners transformed by the matrix M.
	inline AABBox transformedAABB(const Matrix4f& M) const;
	inline AABBox transformedAABBFast(const Matrix4f& M) const; // Faster, but with possible precision issues.

	inline Vec4f getClosestPointInAABB(const Vec4f& p) const;

	inline float distanceToPoint(const Vec4f& p) const; // Get distance from p to closest point in AABB to p

	const std::string toString() const;
	const std::string toStringNSigFigs(int n) const;

	bool invariant() const;
	static void test();

	Vec4f min_;
	Vec4f max_;
};


inline void getAABBCornerVerts(const js::AABBox& aabb, Vec4f* verts_out);


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
	min_ = min(min_, p);
	max_ = max(max_, p);
}


void AABBox::enlargeToHoldAABBox(const AABBox& aabb)
{
	min_ = min(min_, aabb.min_);
	max_ = max(max_, aabb.max_);
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


// Returns a non-zero value if ray intersects the AABB.  Near and far hit distances are stored in near_hitd_out and far_hitd_out.
// This code doesn't handle +-Inf components in recip_unitraydir.  These should be filtered out beforehand in Ray::Ray().
// If we need to handle Infs we can use the NaN filtering technique here: http://www.flipcode.com/archives/SSE_RayBox_Intersection_Test.shtml
// NOTE: Most recent code below is not thoroughly tested, as is used only in DisplacedQuad and MultiLevelGrid.
int AABBox::rayAABBTrace(const Vec4f& raystartpos, const Vec4f& recip_unitraydir, 
						  float& near_hitd_out, float& far_hitd_out) const
{
	assert(recip_unitraydir.isFinite());

	// Get ray t values to lower and upper AABB bounds.
	const Vec4f lower_t = mul(min_ - raystartpos, recip_unitraydir);
	const Vec4f upper_t = mul(max_ - raystartpos, recip_unitraydir);

	// Get min and max t values for each axis.
	const Vec4f maxt = max(lower_t, upper_t);
	const Vec4f mint = min(lower_t, upper_t);

	// Get min of maxt values over all axes, likewise get max of mint values over all axes.
	// TODO: interleave calculations below like TBB code? (or will compiler do it?)
	const Vec4f maxt_1 = shuffle<1, 2, 2, 2>(maxt, maxt);     // (maxt_1, maxt_2, maxt_2, maxt_2)
	const Vec4f maxt_2 = min(maxt, maxt_1);                   // (min(maxt_0, maxt_1), min(maxt_1, maxt_2), min(maxt_2, maxt_2), min(maxt_3, maxt_2))
	const Vec4f maxt_3 = shuffle<1, 1, 1, 1>(maxt_2, maxt_2); // (min(maxt_1, maxt_2), min(maxt_1, maxt_2), min(maxt_1, maxt_2), min(maxt_1, maxt_2))
	const Vec4f maxt_4 = min(maxt_2, maxt_3);                 // (min(maxt_0, maxt_1, maxt_2), ...)

	const Vec4f mint_1 = shuffle<1, 2, 2, 2>(mint, mint);     // (mint_1, mint_0, mint_3, mint_2)
	const Vec4f mint_2 = max(mint, mint_1);                   // (max(mint_0, mint_1), max(mint_1, mint_2), max(mint_2, mint_2), max(mint_3, mint_2))
	const Vec4f mint_3 = shuffle<1, 1, 1, 1>(mint_2, mint_2); // (max(mint_1, mint_2), max(mint_1, mint_2), max(mint_1, mint_2), max(mint_1, mint_2))
	const Vec4f mint_4 = max(mint_2, mint_3);                 // (max(mint_0, mint_1, mint_2), ...)

	near_hitd_out = elem<0>(mint_4); // near hit dist is max of min hit distances on all axes
	far_hitd_out  = elem<0>(maxt_4); // far hit dist is min of max hit distances on all axes.

	// If far hit dist is >= 0 and far hit dist is >= near hit dist, then the ray intersected the AABB.
	return _mm_comige_ss(maxt_4.v, _mm_setzero_ps()) & _mm_comige_ss(maxt_4.v, mint_4.v);
}


int AABBox::traceRay(Ray& ray) const
{
	assert(ray.recip_unitdir_f.isFinite());

	// Get ray t values to lower and upper AABB bounds.
	const Vec4f lower_t = mul(min_ - ray.startpos_f, ray.recip_unitdir_f);
	const Vec4f upper_t = mul(max_ - ray.startpos_f, ray.recip_unitdir_f);

	// Get min and max t values for each axis.
	const Vec4f maxt = max(lower_t, upper_t);
	const Vec4f mint = min(lower_t, upper_t);

	const Vec4f rminmax = loadVec4f(&ray.min_t); // (rmin_t, rmax_t, 0, 0)

	const Vec4f maxt_1 = shuffle<0, 0, 1, 1>(maxt, maxt);     // (maxt_0, maxt_0, maxt_1, maxt_1)
	const Vec4f maxt_2 = shuffle<2, 2, 1, 1>(maxt, rminmax);  // (maxt_2, maxt_2, rmax_t, rmax_t)
	const Vec4f maxt_3 = min(maxt_1, maxt_2);                 // (min(maxt_0, maxt_2), min(maxt_0, maxt_2), min(maxt_1, rmax_t), min(maxt_1, rmax_t))
	const Vec4f maxt_4 = shuffle<2, 0, 2, 0>(maxt_3, maxt_3); // (min(maxt_1, rmax_t),  min(maxt_0, maxt_2), min(maxt_1, rmax_t), min(maxt_0, maxt_2))
	const Vec4f maxt_5 = min(maxt_3, maxt_4);                 // (min(maxt_0, maxt_2, maxt_1, rmax_t), ...)

	const Vec4f mint_1 = shuffle<0, 0, 1, 1>(mint, mint);    
	const Vec4f mint_2 = shuffle<2, 2, 0, 0>(mint, rminmax);   
	const Vec4f mint_3 = max(mint_1, mint_2);                
	const Vec4f mint_4 = shuffle<2, 0, 2, 0>(mint_3, mint_3);
	const Vec4f mint_5 = max(mint_3, mint_4);                

	// if far hit dist is >= near hit dist, then the ray intersected the AABB.
	if(_mm_comige_ss(maxt_5.v, mint_5.v))
	{
		// Update ray near and far
		const Vec4f minmax = shuffle<0, 0, 0, 0>(mint_5, maxt_5); // (mint, mint, maxt, maxt)
		const Vec4f res    = shuffle<0, 2, 0, 2>(minmax, minmax); // (mint, maxt, mint, maxt)
		storeVec4f(res, &ray.min_t);
		return 1;
	}
	else
		return 0;
}


AABBox AABBox::emptyAABBox()
{
	return AABBox(
		Vec4f( std::numeric_limits<float>::infinity(),  std::numeric_limits<float>::infinity(),  std::numeric_limits<float>::infinity(), 1.0f),
		Vec4f(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1.0f)
	);
}


unsigned int AABBox::longestAxis() const
{
	if(axisLength(0) > axisLength(1))
		return axisLength(0) > axisLength(2) ? 0 : 2;
	else // else 1 >= 0
		return axisLength(1) > axisLength(2) ? 1 : 2;
}


float AABBox::longestLength() const
{
	return horizontalMax((max_ - min_).v);
}


inline const Vec4f AABBox::centroid() const
{
	return (min_ + max_) * 0.5f;
}


bool AABBox::operator == (const AABBox& rhs) const
{
	return min_ == rhs.min_ && max_ == rhs.max_;
}


bool AABBox::operator != (const AABBox& rhs) const
{
	return min_ != rhs.min_ || max_ != rhs.max_;
}


inline bool epsEqual(const AABBox& a, const AABBox& b, float /*eps*/ = (float)NICKMATHS_EPSILON)
{
	return epsEqual(a.min_, b.min_) && epsEqual(a.max_, b.max_);
}


GLARE_STRONG_INLINE js::AABBox intersection(const js::AABBox& a, const js::AABBox& b)
{
	return js::AABBox(max(a.min_, b.min_), min(a.max_, b.max_));
}


GLARE_STRONG_INLINE js::AABBox AABBUnion(const js::AABBox& a, const js::AABBox& b)
{
	return js::AABBox(min(a.min_, b.min_), max(a.max_, b.max_));
}


float AABBox::volume() const
{
	const Vec4f diff = max_ - min_;
	return diff[0] * diff[1] * diff[2];
}


float AABBox::getSurfaceArea() const
{
	const Vec4f diff = max_ - min_;
	const Vec4f res = mul(diff, swizzle<2, 0, 1, 3>(diff)); // = (diff.x[0]*diff.x[2], diff.x[1]*diff.x[0], diff.x[2]*diff.x[1], diff.x[3]*diff.x[3])
	return 2 * (res[0] + res[1] + res[2]);
}


float AABBox::getHalfSurfaceArea() const
{
	const Vec4f diff = max_ - min_;
	const Vec4f res = mul(diff, swizzle<2, 0, 1, 3>(diff)); // = (diff.x[0]*diff.x[2], diff.x[1]*diff.x[0], diff.x[2]*diff.x[1], diff.x[3]*diff.x[3])
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


/*
Faster, but doesn't result in exactly the same AABB as computing the AABB enclosing M * corner_i for each corner, for numerical reasons.

let
a = (m_11 m_12 m_13 m_14)  (min_x) = (m_11.min_x + m_12.min_y + m_13.min_z + m_14)
	(m_21 m_22 m_23 m_24)  (min_y)   (m_21.min_x + m_22.min_y + m_23.min_z + m_24)
	(                   )  (min_z)   (m_31.min_x + m_32.min_y + m_33.min_z + m_34)
	(					)  (    1)   (                                          1)

b = (m_11 m_12 m_13 m_14)  (min_x) = (m_11.min_x + m_12.min_y + m_13.max_z + m_14)
	(m_21 m_22 m_23 m_24)  (min_y)   (m_21.min_x + m_22.min_y + m_23.max_z + m_24)
	(                   )  (max_z)   (m_31.min_x + m_32.min_y + m_33.max_z + m_34)   # Note max_z here
	(					)  (    1)   (                                          1)

c = (m_11 m_12 m_13 m_14)  (min_x) = (m_11.min_x + m_12.max_y + m_13.min_z + m_14)
	(m_21 m_22 m_23 m_24)  (max_y)   (m_21.min_x + m_22.max_y + m_23.min_z + m_24)   # Note max_y here
	(                   )  (min_z)   (m_31.min_x + m_32.max_y + m_33.min_z + m_34)
	(					)  (    1)   (                                          1)

...

i = (m_11 m_12 m_13 m_14)  (max_x) = (m_11.max_x + m_12.max_y + m_13.max_z + m_14)
	(m_21 m_22 m_23 m_24)  (max_y)   (m_21.max_x + m_22.max_y + m_23.max_z + m_24)
	(                   )  (max_z)   (m_31.max_x + m_32.max_y + m_33.max_z + m_34)
	(					)  (    1)   (                                          1)


let min' = (min(a_x, b_x, c_x, ....., i_x) = (min((m_11.min_x + m_12.min_y + m_13.min_z + m_14), (m_11.min_x + m_12.min_y + m_13.max_z + m_14), ... , (m_11.max_x + m_12.max_y + m_13.max_z + m_14))
		   (min(a_y, b_y, c_y, ....., i_y)   ( ... )
		   (min(a_z, b_z, c_z, ....., i_z)   ( ... )
		   (1)                               (1)

 min' = (min(m_11.min_x, m_11.max_x) + min(m_12.min_y, m_12.max_y) + min(m_13.min_z, m_13.max_z) + m_14)
		(min(m_21.min_x, m_21.max_x) + min(m_22.min_y, m_22.max_y) + min(m_23.min_z, m_23.max_z) + m_24)
		(min(m_31.min_x, m_31.max_x) + min(m_32.min_y, m_32.max_y) + min(m_33.min_z, m_33.max_z) + m_34)
		(1)

		(m_11.min_x)
		(m_21.min_x)
		(m_31.min_x)
		(0)
= m_col_1.min_x

so min' = component_wise_min(m_col_1.min_x, m_col_1.max_x) + component_wise_min(m_col_2.min_y, m_col_2.max_y) + component_wise_min(m_col_3.min_y, m_col_3.max_y) + col_4

likewise

max' = component_wise_max(m_col_1.min_x, m_col_1.max_x) + component_wise_max(m_col_2.min_y, m_col_2.max_y) + component_wise_max(m_col_3.min_y, m_col_3.max_y) + col_4
*/
AABBox AABBox::transformedAABBFast(const Matrix4f& M) const 
{
	const Vec4f col_0 = M.getColumn(0);
	const Vec4f col_1 = M.getColumn(1);
	const Vec4f col_2 = M.getColumn(2);
	const Vec4f col_3 = M.getColumn(3);

	const Vec4f col_0_min_x = col_0 * elem<0>(min_);
	const Vec4f col_0_max_x = col_0 * elem<0>(max_);
	const Vec4f col_1_min_y = col_1 * elem<1>(min_);
	const Vec4f col_1_max_y = col_1 * elem<1>(max_);
	const Vec4f col_2_min_z = col_2 * elem<2>(min_);
	const Vec4f col_2_max_z = col_2 * elem<2>(max_);

	return js::AABBox(
		min(col_0_min_x, col_0_max_x) + min(col_1_min_y, col_1_max_y) + min(col_2_min_z, col_2_max_z) + col_3,
		max(col_0_min_x, col_0_max_x) + max(col_1_min_y, col_1_max_y) + max(col_2_min_z, col_2_max_z) + col_3
	);
}


void getAABBCornerVerts(const js::AABBox& aabb, Vec4f* verts_out)
{
	const Vec4f lo = unpacklo(aabb.min_, aabb.max_); // (min[0], max[0], min[1], max[1])

	verts_out[0] = aabb.min_;                                 // (min[0], min[1], min[2], 1)
	verts_out[1] = shuffle<1, 2, 2, 3>(lo,        aabb.min_); // (max[0], min[1], min[2], 1)
	verts_out[2] = shuffle<0, 3, 2, 3>(lo,        aabb.min_); // (min[0], max[1], min[2], 1)
	verts_out[3] = shuffle<0, 1, 2, 3>(aabb.max_, aabb.min_); // (max[0], max[1], min[2], 1)
	verts_out[4] = shuffle<0, 1, 2, 3>(aabb.min_, aabb.max_); // (min[0], min[1], max[2], 1)
	verts_out[5] = shuffle<1, 2, 2, 3>(lo,        aabb.max_); // (max[0], min[1], max[2], 1)
	verts_out[6] = shuffle<0, 3, 2, 3>(lo,        aabb.max_); // (min[0], max[1], max[2], 1)
	verts_out[7] = aabb.max_;                                 // (max[0], max[1], max[2], 1)
}


Vec4f AABBox::getClosestPointInAABB(const Vec4f& p) const
{
	return max(min_, min(p, max_));
}


float AABBox::distanceToPoint(const Vec4f& p) const // Get distance from p to closest point in AABB to p
{
	return p.getDist(getClosestPointInAABB(p));
}


} //end namespace js
