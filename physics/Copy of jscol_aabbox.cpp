/*=====================================================================
aabbox.cpp
----------
File created by ClassTemplate on Thu Nov 18 03:48:29 2004Code By Nicholas Chapman.
=====================================================================*/
#include "jscol_aabbox.h"


#include "../simpleraytracer/ray.h"
//#include <xmmintrin.h>
#include <float.h>


#define SSE_RAY_AABB_TEST 1

#ifdef SSE_RAY_AABB_TEST
#include "../maths/SSE.h"
#endif




namespace js
{




/*AABBox::AABBox()
{
	
}*/

AABBox::AABBox(const Vec3& min_, const Vec3& max_)
:	min(min_), 
	max(max_)
{
	
}

AABBox::~AABBox()
{
	
}

void AABBox::enlargeToHoldPoint(const Vec3& p)
{
	for(int c=0; c<3; ++c)//for each component
	{
		if(p[c] < min[c])
			min[c] = p[c];

		//NOTE: make this else if?
		if(p[c] > max[c])
			max[c] = p[c];
	}
}


bool AABBox::rayAABBTrace(const Ray& ray, float& near_hitd_out, float& far_hitd_out) const
{
	float largestneardist;
	float smallestfardist;

	//-----------------------------------------------------------------
	//test x
	//-----------------------------------------------------------------
	if(ray.unitdir.x > 0.0f)
	{
		//dist along ray = dist to travel / speed in that dir
		largestneardist = (min.x - ray.startpos.x) / ray.unitdir.x;
		smallestfardist = (max.x - ray.startpos.x) / ray.unitdir.x;
	}
	else
	{
		smallestfardist = (min.x - ray.startpos.x) / ray.unitdir.x;
		largestneardist = (max.x - ray.startpos.x) / ray.unitdir.x;
	}
		
	//-----------------------------------------------------------------
	//test y
	//-----------------------------------------------------------------
	float ynear;
	float yfar;

	if(ray.unitdir.y > 0.0f)
	{
		ynear = (min.y - ray.startpos.y) / ray.unitdir.y;
		yfar = (max.y - ray.startpos.y) / ray.unitdir.y;
	}
	else
	{
		yfar = (min.y - ray.startpos.y) / ray.unitdir.y;
		ynear = (max.y - ray.startpos.y) / ray.unitdir.y;
	}

	if(ynear > largestneardist)
		largestneardist = ynear;

	if(yfar < smallestfardist)
		smallestfardist = yfar;

	if(largestneardist > smallestfardist)
		return false;//-1.0f;//false;

	//-----------------------------------------------------------------
	//test z
	//-----------------------------------------------------------------
	float znear;
	float zfar;

	if(ray.unitdir.z > 0.0f)
	{
		znear = (min.z - ray.startpos.z) / ray.unitdir.z;
		zfar = (max.z - ray.startpos.z) / ray.unitdir.z;
	}
	else
	{
		zfar = (min.z - ray.startpos.z) / ray.unitdir.z;
		znear = (max.z - ray.startpos.z) / ray.unitdir.z;
	}

	if(znear > largestneardist)
		largestneardist = znear;

	if(zfar < smallestfardist)
		smallestfardist = zfar;

	if(largestneardist > smallestfardist)
		return false;//return -1.0f;//return false;
	else
	{
		//return myMax(0.0f, largestneardist);//largestneardist <= raystart.getDist(rayend);//Vec3(rayend - raystart).length());
		near_hitd_out = myMax(0.0f, largestneardist);

		//NOTE: this correct?
		far_hitd_out = myMax(near_hitd_out, smallestfardist);

		return true;
	}

	/*float largestneardist;
	float smallestfardist;

	//-----------------------------------------------------------------
	//test x
	//-----------------------------------------------------------------
	if(ray.unitdir.x > 0.0f)
	{
		//dist along ray = dist to travel / speed in that dir
		largestneardist = (min.x - ray.startpos.x) / ray.unitdir.x;
		smallestfardist = (max.x - ray.startpos.x) / ray.unitdir.x;
	}
	else
	{
		smallestfardist = (min.x - ray.startpos.x) / ray.unitdir.x;
		largestneardist = (max.x - ray.startpos.x) / ray.unitdir.x;
	}
		
	//-----------------------------------------------------------------
	//test y
	//-----------------------------------------------------------------
	float ynear;
	float yfar;

	if(ray.unitdir.y > 0.0f)
	{
		ynear = (min.y - ray.startpos.y) / ray.unitdir.y;
		yfar = (max.y - ray.startpos.y) / ray.unitdir.y;
	}
	else
	{
		yfar = (min.y - ray.startpos.y) / ray.unitdir.y;
		ynear = (max.y - ray.startpos.y) / ray.unitdir.y;
	}

	if(ynear > largestneardist)
		largestneardist = ynear;

	if(yfar < smallestfardist)
		smallestfardist = yfar;

	if(largestneardist > smallestfardist)
		return false;//-1.0f;//false;

	//-----------------------------------------------------------------
	//test z
	//-----------------------------------------------------------------
	float znear;
	float zfar;

	if(ray.unitdir.z > 0.0f)
	{
		znear = (min.z - ray.startpos.z) / ray.unitdir.z;
		zfar = (max.z - ray.startpos.z) / ray.unitdir.z;
	}
	else
	{
		zfar = (min.z - ray.startpos.z) / ray.unitdir.z;
		znear = (max.z - ray.startpos.z) / ray.unitdir.z;
	}

	if(znear > largestneardist)
		largestneardist = znear;

	if(zfar < smallestfardist)
		smallestfardist = zfar;

	if(largestneardist > smallestfardist)
		return false;//return -1.0f;//return false;
	else
	{
		//return myMax(0.0f, largestneardist);//largestneardist <= raystart.getDist(rayend);//Vec3(rayend - raystart).length());
		near_hitd_out = myMax(0.0f, largestneardist);

		//NOTE: this correct?
		far_hitd_out = myMax(near_hitd_out, smallestfardist);

		return true;
	}*/
}

#ifdef SSE_RAY_AABB_TEST


bool AABBox::rayAABBTrace(const PaddedVec3& raystartpos, const PaddedVec3& recip_unitraydir, 
						  float& near_hitd_out, float& far_hitd_out) const
{
	const SSE4Vec startpos = load4Vec(&raystartpos.x);//ray origin
	const SSE4Vec recipdir = load4Vec(&recip_unitraydir.x);//unit ray direction

	//const SSE4Vec aabb_min = load4Vec(&min.x);
	//const SSE4Vec aabb_max = load4Vec(&max.x);

	SSE4Vec dist1 = mult4Vec(sub4Vec(load4Vec(&min.x), startpos), recipdir);//distances to smaller coord planes
	SSE4Vec dist2 = mult4Vec(sub4Vec(load4Vec(&max.x), startpos), recipdir);//distances to larger coord planes

	SSE4Vec tmin = min4Vec(dist1, dist2);//interval minima, one for each axis slab
	SSE4Vec tmax = max4Vec(dist1, dist2);//interval maxima, one for each axis slab

	//get the max of the interval minima
	//get the min of the interval minima

	//if max > min
		//return true
	//else return false

	SSE_ALIGN PaddedVec3 tmin_f;
	SSE_ALIGN PaddedVec3 tmax_f;
	store4Vec(tmin, &tmin_f.x);
	store4Vec(tmax, &tmax_f.x);

	//get the maximum of the 3 interval minima
	const float zero_f = 0.0f;
	tmin = max4Vec( loadScalarLow(&zero_f), 
		max4Vec( max4Vec(loadScalarLow(&tmin_f.x), loadScalarLow(&tmin_f.y)), loadScalarLow(&tmin_f.z) ) );

	//get the minimum of the 3 interval maxima
	tmax = min4Vec( min4Vec(loadScalarLow(&tmax_f.x), loadScalarLow(&tmax_f.y)), loadScalarLow(&tmax_f.z) );
	
	storeLowWord(tmin, &near_hitd_out);
	storeLowWord(tmax, &far_hitd_out);

	//near_hitd_out = myMax(tmin_f.x, myMax(tmin_f.y, tmin_f.z));
	//far_hitd_out = myMin(tmax_f.x, myMin(tmax_f.y, tmax_f.z));

	//near_hitd_out = myMax(0.0f, near_hitd_out);

	return near_hitd_out <= far_hitd_out;
}



#else

bool AABBox::rayAABBTrace(const PaddedVec3& raystartpos, const PaddedVec3& recip_unitraydir, 
						  float& near_hitd_out, float& far_hitd_out) const
{

	/*SSE4Vec lmin,lmax;
	{
		SSE4Vec l1 = _mm_mul_ss(_mm_sub_ss(_mm_load_ss(&this->min.x), _mm_load_ss(&raystartpos.x)), _mm_load_ss(&recip_unitraydir.x));
		SSE4Vec l2 = _mm_mul_ss(_mm_sub_ss(_mm_load_ss(&this->max.x), _mm_load_ss(&raystartpos.x)), _mm_load_ss(&recip_unitraydir.x));
		lmin = _mm_min_ss(l1,l2);
		lmax = _mm_max_ss(l1,l2);
	}
	{
		SSE4Vec l1 = _mm_mul_ss(_mm_sub_ss(_mm_load_ss(&this->min.y), _mm_load_ss(&raystartpos.y)), _mm_load_ss(&recip_unitraydir.y));
		SSE4Vec l2 = _mm_mul_ss(_mm_sub_ss(_mm_load_ss(&this->max.y), _mm_load_ss(&raystartpos.y)), _mm_load_ss(&recip_unitraydir.y));
		lmin = _mm_max_ss(lmin, _mm_min_ss(l1,l2));
		lmax = _mm_min_ss(lmax, _mm_max_ss(l1,l2));
	}
 
	int ret;
	{
		SSE4Vec l1 = _mm_mul_ss(_mm_sub_ss(_mm_load_ss(&this->min.z), _mm_load_ss(&raystartpos.z)), _mm_load_ss(&recip_unitraydir.z));
		SSE4Vec l2 = _mm_mul_ss(_mm_sub_ss(_mm_load_ss(&this->max.z), _mm_load_ss(&raystartpos.z)), _mm_load_ss(&recip_unitraydir.z));
		lmax = _mm_min_ss(lmax, _mm_max_ss(l1,l2));
		ret = _mm_comigt_ss(lmax, _mm_setzero_ps());
		lmin = _mm_max_ss(lmin, _mm_min_ss(l1,l2));
		ret &= _mm_comige_ss(lmax,lmin); 
	}
	return (bool)ret;*/











	float largestneardist;
	float smallestfardist;

	//-----------------------------------------------------------------
	//test x
	//-----------------------------------------------------------------
	if(recip_unitraydir.x > 0.0f)//if heading in positive dir along x axis
	{
//		if(max.x < raystartpos.x)//if max x of AABB is left of ray start x
//			return false;

		//dist along ray = dist to travel / speed in that dir
		largestneardist = (min.x - raystartpos.x) * recip_unitraydir.x;
		smallestfardist = (max.x - raystartpos.x) * recip_unitraydir.x;
	}
	else//else heading to left (or up/down)
	{
//		if(min.x > raystartpos.x)//if min x of AABB is right of ray start x
//			return false;

		smallestfardist = (min.x - raystartpos.x) * recip_unitraydir.x;
		largestneardist = (max.x - raystartpos.x) * recip_unitraydir.x;
	}

	//NEW:
	if(smallestfardist < 0.0f)//if exit from box is behind ray start
		return false;
		
	//-----------------------------------------------------------------
	//test y
	//-----------------------------------------------------------------
	float ynear;
	float yfar;

	if(recip_unitraydir.y > 0.0f)
	{
		ynear = (min.y - raystartpos.y) * recip_unitraydir.y;
		yfar = (max.y - raystartpos.y) * recip_unitraydir.y;
	}
	else
	{
		yfar = (min.y - raystartpos.y) * recip_unitraydir.y;
		ynear = (max.y - raystartpos.y) * recip_unitraydir.y;
	}

	if(yfar < 0.0f)//if heading away from slab
		return false;

	if(ynear > largestneardist)
		largestneardist = ynear;

	if(yfar < smallestfardist)
		smallestfardist = yfar;

	if(largestneardist > smallestfardist)
		return false;//-1.0f;//false;

	//-----------------------------------------------------------------
	//test z
	//-----------------------------------------------------------------
	float znear;
	float zfar;

	if(recip_unitraydir.z > 0.0f)
	{
		znear = (min.z - raystartpos.z) * recip_unitraydir.z;
		zfar = (max.z - raystartpos.z) * recip_unitraydir.z;
	}
	else
	{
		zfar = (min.z - raystartpos.z) * recip_unitraydir.z;
		znear = (max.z - raystartpos.z) * recip_unitraydir.z;
	}

	if(zfar < 0.0f)//if heading away from slab
		return false;


	if(znear > largestneardist)
		largestneardist = znear;

	if(zfar < smallestfardist)
		smallestfardist = zfar;

	//NEWCODE:
	//consider the interval start to be 0 at least.
	if(largestneardist < 0.0f)
		largestneardist = 0.0f;

	if(largestneardist > smallestfardist)
		return false;//return -1.0f;//return false;
	else
	{
		/*//return myMax(0.0f, largestneardist);//largestneardist <= raystart.getDist(rayend);//Vec3(rayend - raystart).length());
		near_hitd_out = myMax(0.0f, largestneardist);

		//NOTE: this correct?
		far_hitd_out = myMax(near_hitd_out, smallestfardist);*/


		//if(largestneardist < 0.0f)//if are inside AABB
		//	near_hitd_out = largestneardist;//TEMP WAS 0
		//else
			near_hitd_out = largestneardist;

		//assert(smallestfardist >= 0.0f);
		far_hitd_out = smallestfardist;

		assert(!isNAN(near_hitd_out));
		assert(!isNAN(far_hitd_out));

		return true;
	}
}

#endif

/*
#ifdef __GNUC__
	#define _MM_ALIGN16 __attribute__ ((aligned (16)))
#endif

// turn those verbose intrinsics into something readable.
#define loadps(mem)		_mm_load_ps((const float * const)(mem))
#define storess(ss,mem)		_mm_store_ss((float * const)(mem),(ss))
#define minss			_mm_min_ss
#define maxss			_mm_max_ss
#define minps			_mm_min_ps
#define maxps			_mm_max_ps
#define mulps			_mm_mul_ps
#define subps			_mm_sub_ps
#define rotatelps(ps)		_mm_shuffle_ps((ps),(ps), 0x39)	// a,b,c,d -> b,c,d,a
#define muxhps(low,high)	_mm_movehl_ps((low),(high))	// low{a,b,c,d}|high{e,f,g,h} = {c,d,g,h}


static const float flt_plus_inf = -logf(0);	// let's keep C and C++ compilers happy.
static const float _MM_ALIGN16
	ps_cst_plus_inf[4]	= {  flt_plus_inf,  flt_plus_inf,  flt_plus_inf,  flt_plus_inf },
	ps_cst_minus_inf[4]	= { -flt_plus_inf, -flt_plus_inf, -flt_plus_inf, -flt_plus_inf };

struct vec4 { float x,y,z,pad; };

bool AABBox::TBPIntersect(const Vec3& raystartpos_, const float* recip_unitraydir, 
						  float& near_hitd_out, float& far_hitd_out) 
{
	const vec4 _MM_ALIGN16 raystartpos = { raystartpos_.x, raystartpos_.y, raystartpos_.z };
	//const vec4 _MM_ALIGN16 recip_unitraydir = { recip_unitraydir_.x, recip_unitraydir_.y, recip_unitraydir_.z };
	//const vec4 _MM_ALIGN16 min4 = { min.x, min.y, min.z };
	//const vec4 _MM_ALIGN16 max4 = { max.x, max.y, max.z };

	// you may already have those values hanging around somewhere
	const __m128
		plus_inf	= loadps(ps_cst_plus_inf),
		minus_inf	= loadps(ps_cst_minus_inf);

	// use whatever's apropriate to load.
	const __m128
		box_min	= loadps(&(this->min)),//&min4),
		box_max	= loadps(&(this->max)),//&max4),
		pos	= loadps(&raystartpos),
		inv_dir	= loadps(recip_unitraydir);

	// use a div if inverted directions aren't available
	const __m128 l1 = mulps(subps(box_min, pos), inv_dir);
	const __m128 l2 = mulps(subps(box_max, pos), inv_dir);

	// the order we use for those min/max is vital to filter out
	// NaNs that happens when an inv_dir is +/- inf and
	// (box_min - pos) is 0. inf * 0 = NaN
	const __m128 filtered_l1a = minps(l1, plus_inf);
	const __m128 filtered_l2a = minps(l2, plus_inf);

	const __m128 filtered_l1b = maxps(l1, minus_inf);
	const __m128 filtered_l2b = maxps(l2, minus_inf);

	// now that we're back on our feet, test those slabs.
	__m128 lmax = maxps(filtered_l1a, filtered_l2a);
	__m128 lmin = minps(filtered_l1b, filtered_l2b);

	// unfold back. try to hide the latency of the shufps & co.
	const __m128 lmax0 = rotatelps(lmax);
	const __m128 lmin0 = rotatelps(lmin);
	lmax = minss(lmax, lmax0);
	lmin = maxss(lmin, lmin0);

	const __m128 lmax1 = muxhps(lmax,lmax);
	const __m128 lmin1 = muxhps(lmin,lmin);
	lmax = minss(lmax, lmax1);
	lmin = maxss(lmin, lmin1);

	const bool ret = _mm_comige_ss(lmax, _mm_setzero_ps()) & _mm_comige_ss(lmax,lmin);

	storess(lmin, &near_hitd_out);
	storess(lmax, &far_hitd_out);

	return  ret;
}*/

/*bool AABBox::ray_box_intersects_slabs_geimer_muller_sse_ss(
							const Vec3& raystartpos, const Vec3& recip_unitraydir, 
						  float& near_hitd_out, float& far_hitd_out) 
{
	__m128 lmin,lmax;

	{
		__m128 l1 = mulss(subss(loadss(&this->min.x), loadss(&raystartpos.x)), loadss(&recip_unitraydir.x));
		__m128 l2 = mulss(subss(loadss(&this->max.x), loadss(&raystartpos.x)), loadss(&recip_unitraydir.x));
		lmin = minss(l1,l2);
		lmax = maxss(l1,l2);
	}
	{
		__m128 l1 = mulss(subss(loadss(&this->min.y), loadss(&raystartpos.y)), loadss(&recip_unitraydir.y));
		__m128 l2 = mulss(subss(loadss(&this->max.y), loadss(&raystartpos.y)), loadss(&recip_unitraydir.y));
		lmin = maxss(lmin, minss(l1,l2));
		lmax = minss(lmax, maxss(l1,l2));
	}
 
	bool ret;
	{
		__m128 l1 = mulss(subss(loadss(&this->min.z), loadss(&raystartpos.z)), loadss(&recip_unitraydir.z));
		__m128 l2 = mulss(subss(loadss(&this->max.z), loadss(&raystartpos.z)), loadss(&recip_unitraydir.z));
		lmax = minss(lmax, maxss(l1,l2));
		ret = _mm_comigt_ss(lmax, _mm_setzero_ps());
		lmin = maxss(lmin, minss(l1,l2));
		ret &= _mm_comige_ss(lmax,lmin); 
	}
	return  ret;
}*/
/*
bool_t AABBox::ray_box_intersects_slabs_geimer_muller_sse_ss(const rt::ray_t &ray) 
{
		__m128 lmin,lmax;
		{
			__m128 l1 = mulss(subss(loadss(&box.minX), loadss(&ray.pos.x)), loadss(&ray.rcp_dir.x));
			__m128 l2 = mulss(subss(loadss(&box.maxX), loadss(&ray.pos.x)), loadss(&ray.rcp_dir.x));
			lmin = minss(l1,l2);
			lmax = maxss(l1,l2);
		}
		{
			__m128 l1 = mulss(subss(loadss(&box.minY), loadss(&ray.pos.y)), loadss(&ray.rcp_dir.y));
			__m128 l2 = mulss(subss(loadss(&box.maxY), loadss(&ray.pos.y)), loadss(&ray.rcp_dir.y));
			lmin = maxss(lmin, minss(l1,l2));
			lmax = minss(lmax, maxss(l1,l2));
		}
 
		bool_t ret;
		{
			__m128 l1 = mulss(subss(loadss(&box.minY), loadss(&ray.pos.z)), loadss(&ray.rcp_dir.z));
			__m128 l2 = mulss(subss(loadss(&box.maxY), loadss(&ray.pos.z)), loadss(&ray.rcp_dir.z));
			lmax = minss(lmax, maxss(l1,l2));
			ret = _mm_comigt_ss(lmax, _mm_setzero_ps());
			lmin = maxss(lmin, minss(l1,l2));
			ret &= _mm_comige_ss(lmax,lmin); 
		}
		return  ret;
	}
*/




float AABBox::getSurfaceArea() const
{
	const Vec3 diff = max - min;

	return diff.x*diff.y + diff.x*diff.z + diff.y*diff.z;
}


bool AABBox::invariant()
{
	return max.x >= min.x && max.y >= min.y && max.z >= min.z;
}



} //end namespace js






