/*=====================================================================
aabbox.h
--------
File created by ClassTemplate on Thu Nov 18 03:48:29 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __AABBOX_H_666_
#define __AABBOX_H_666_


#include "../utils/platform.h"
#include "../maths/PaddedVec3.h"
#include "../maths/SSE.h"
#include "../maths/mathstypes.h"

namespace js
{

/*=====================================================================
AABBox
------
Axis aligned bounding box.
single-precision floating point.
Must be 16-byte aligned.
=====================================================================*/
class AABBox
{
public:
	/*=====================================================================
	AABBox
	------
	
	=====================================================================*/
	inline AABBox();
	inline AABBox(const Vec3f& _min, const Vec3f& _max);
	inline ~AABBox();

	inline bool operator == (const AABBox& rhs) const;

	inline void enlargeToHoldPoint(const Vec3f& p);
	inline void enlargeToHoldAABBox(const AABBox& aabb);

	DO_FORCEINLINE
	int rayAABBTrace(const PaddedVec3& raystartpos, 
		const PaddedVec3& recip_unitraydir, 
		float& near_hitd_out, float& far_hitd_out) const;
	
	//bool slowRayAABBTrace(const Ray& ray, float& near_hitd_out, float& far_hitd_out) const;
	//bool TBPIntersect(const Vec3f& raystartpos, const float* recip_unitraydir, float& near_hitd_out, float& far_hitd_out); 

	inline float getSurfaceArea() const;
	bool invariant();
	static void test();

	//void writeModel(std::ostream& stream, int& num_verts) const;

	inline float axisLength(unsigned int axis) const { return max_[axis] - min_[axis]; }
	inline unsigned int longestAxis() const;

	// Moller's code:
	// http://www.cs.lth.se/home/Tomas_Akenine_Moller/code/tribox3.txt
	//int triBoxOverlap(float boxcenter[3], float boxhalfsize[3], float triverts[3][3]);
	//int triangleBoxOverlap(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2) const;

	SSE_ALIGN PaddedVec3 min_;
	SSE_ALIGN PaddedVec3 max_;
};


AABBox::AABBox()
{
}

AABBox::AABBox(const Vec3f& _min, const Vec3f& _max)
:	min_(_min), 
	max_(_max)
{
}

AABBox::~AABBox()
{
}

void AABBox::enlargeToHoldPoint(const Vec3f& p)
{
	for(unsigned int c=0; c<3; ++c)//for each component
	{
		if(p[c] < min_[c])
			min_[c] = p[c];

		if(p[c] > max_[c])
			max_[c] = p[c];
	}
}

void AABBox::enlargeToHoldAABBox(const AABBox& aabb)
{
	for(int unsigned c=0; c<3; ++c)//for each axis
	{
		if(aabb.min_[c] < min_[c])
			min_[c] = aabb.min_[c];

		if(aabb.max_[c] > max_[c])
			max_[c] = aabb.max_[c];
	}
}

/*
float AABBox::getXDims() const
{
	return max.x - min.x;
}

float AABBox::getYDims() const
{
	return max.y - min.y;
}

float AABBox::getZDims() const
{
	return max.z - min.z;
}*/


const float flt_plus_inf = -logf(0);	// let's keep C and C++ compilers happy.
const float SSE_ALIGN
	ps_cst_plus_inf[4]	= {  flt_plus_inf,  flt_plus_inf,  flt_plus_inf,  flt_plus_inf },
	ps_cst_minus_inf[4]	= { -flt_plus_inf, -flt_plus_inf, -flt_plus_inf, -flt_plus_inf };

#define rotatelps(ps)		_mm_shuffle_ps((ps),(ps), 0x39)	// a,b,c,d -> b,c,d,a
#define muxhps(low,high)	_mm_movehl_ps((low),(high))	// low{a,b,c,d}|high{e,f,g,h} = {c,d,g,h}
#define minss			_mm_min_ss
#define maxss			_mm_max_ss

DO_FORCEINLINE
int AABBox::rayAABBTrace(const PaddedVec3& raystartpos, const PaddedVec3& recip_unitraydir, 
						  float& near_hitd_out, float& far_hitd_out) const
{
	assertSSEAligned(&raystartpos);
	assertSSEAligned(&recip_unitraydir);
	assertSSEAligned(&min_);
	assertSSEAligned(&max_);

	//assert(::epsEqual(Vec3(1.0f / recip_unitraydir.x, 1.0f / recip_unitraydir.y, 1.0f / recip_unitraydir.z).length(), 1.0f));

	// you may already have those values hanging around somewhere
	const SSE4Vec
		plus_inf	= load4Vec(ps_cst_plus_inf),
		minus_inf	= load4Vec(ps_cst_minus_inf);

	// use whatever's apropriate to load.
	const SSE4Vec
		box_min	= load4Vec(&min_.x),
		box_max	= load4Vec(&max_.x),
		pos	= load4Vec(&raystartpos.x),
		inv_dir	= load4Vec(&recip_unitraydir.x);

	// use a div if inverted directions aren't available
	const SSE4Vec l1 = mult4Vec(sub4Vec(box_min, pos), inv_dir);
	const SSE4Vec l2 = mult4Vec(sub4Vec(box_max, pos), inv_dir);

	// the order we use for those min/max is vital to filter out
	// NaNs that happens when an inv_dir is +/- inf and
	// (box_min - pos) is 0. inf * 0 = NaN
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


/*

	const SSE4Vec startpos = load4Vec(&raystartpos.x);//ray origin
	const SSE4Vec recipdir = load4Vec(&recip_unitraydir.x);//unit ray direction

	SSE4Vec dist1 = mult4Vec(sub4Vec(load4Vec(&min.x), startpos), recipdir);//distances to smaller coord planes
	SSE4Vec dist2 = mult4Vec(sub4Vec(load4Vec(&max.x), startpos), recipdir);//distances to larger coord planes

	SSE4Vec tmin = min4Vec(dist1, dist2);//interval minima, one for each axis slab
	SSE4Vec tmax = max4Vec(dist1, dist2);//interval maxima, one for each axis slab

	//save SSE registers
	SSE_ALIGN PaddedVec3 tmin_f;
	SSE_ALIGN PaddedVec3 tmax_f;
	store4Vec(tmin, &tmin_f.x);//save minima
	store4Vec(tmax, &tmax_f.x);//save maxima

	//get the maximum of the 3 interval minimam, and 0
	const float zero_f = 0.0f;
	tmin = max4Vec( loadScalarLow(&zero_f), 
		max4Vec( max4Vec(loadScalarLow(&tmin_f.x), loadScalarLow(&tmin_f.y)), loadScalarLow(&tmin_f.z) ) );

	//get the minimum of the 3 interval maxima
	tmax = min4Vec( min4Vec(loadScalarLow(&tmax_f.x), loadScalarLow(&tmax_f.y)), loadScalarLow(&tmax_f.z) );

	//save the final interval minimum and maximum
	storeLowWord(tmin, &near_hitd_out);
	storeLowWord(tmax, &far_hitd_out);

	//compare the minimum and maximum,
	//save the comparison result
	int result;
	storeLowWord(lessThanLowWord(tmin, tmax), (float*)&result);


	return result;//near_hitd_out <= far_hitd_out;*/
}


//SSE_ALIGN const float zero_4vec_f[4] = { 0.0f, 0.0f, 0.0f, 0.0f };


unsigned int AABBox::longestAxis() const
{
	if(axisLength(0) > axisLength(1))
		return axisLength(0) > axisLength(2) ? 0 : 2;
	else //else 1 >= 0
		return axisLength(1) > axisLength(2) ? 1 : 2;
}

bool AABBox::operator == (const AABBox& rhs) const
{
	return min_ == rhs.min_ && max_ == rhs.max_;
}

inline bool epsEqual(const AABBox& a, const AABBox& b, float eps = (float)NICKMATHS_EPSILON)
{
	return epsEqual(a.min_, b.min_, eps) && epsEqual(a.max_, b.max_, eps);
}


float AABBox::getSurfaceArea() const
{
	const Vec3f diff = max_ - min_;

	return 2.0f * (diff.x*diff.y + diff.x*diff.z + diff.y*diff.z);
}



} //end namespace js

#endif //__AABBOX_H_666_








