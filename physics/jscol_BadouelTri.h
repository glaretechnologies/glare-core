/*=====================================================================
BadouelTri.h
------------
File created by ClassTemplate on Sun Jul 03 08:28:36 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __BADOUELTRI_H_666_
#define __BADOUELTRI_H_666_


#include "../maths/vec3.h"
#include "../maths/SSE.h"
#include "../simpleraytracer/ray.h"
//#include "../indigo/globals.h"
//#include "../utils/stringutils.h"


namespace js
{


/*=====================================================================
BadouelTri
----------
Should be 48 bytes
=====================================================================*/
class BadouelTri
{
public:
	BadouelTri();
	~BadouelTri();

	void set(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2);

	inline unsigned int rayIntersect(const Ray& ray, float ray_t_max, float& dist_out, float& u_out, float& v_out) const; //non zero if hit
	inline unsigned int referenceIntersect(const Ray& ray, float ray_t_max, float& dist_out, float& u_out, float& v_out) const; //non zero if hit

	inline const Vec3f& getNormal() const { return normal; }

private:
	SSE_ALIGN Vec3f normal;
	float dist;

	uint32 project_axis_1;
	uint32 project_axis_2;

	float v0_1;
	float v0_2;
	float t11, t12, t21, t22;
};


const float BADOUEL_MIN_DIST = 0.00000001f;//this is to avoid denorms


unsigned int BadouelTri::referenceIntersect(const Ray& ray, float ray_t_max, float& dist_out, float& u_out, float& v_out) const
{
	const float denom = dot(ray.unitDirF(), this->normal);
	if(denom == 0.0f)
		return 0;

	const float raydist = (this->dist - dot(ray.startPosF(), this->normal)) / denom; // Signed distance until ray intersects triangle plane.
	if(raydist < BADOUEL_MIN_DIST || raydist >= ray_t_max)
		return 0;

	const float u = ray.startPosF()[project_axis_1] + ray.unitDirF()[project_axis_1] * raydist - v0_1;
	const float v = ray.startPosF()[project_axis_2] + ray.unitDirF()[project_axis_2] * raydist - v0_2;
	const float alpha = t11*u + t12*v;
	const float beta = t21*u + t22*v;
	assert(!isNAN(alpha) && !isNAN(beta));

	dist_out = raydist;
	u_out = alpha;
	v_out = beta;

	return (alpha >= 0.0f && beta >= 0.0f && (alpha + beta) <= 1.0f) ? 1 : 0;
}


unsigned int BadouelTri::rayIntersect(const Ray& ray, float ray_t_max, float& dist_out, float& u_out, float& v_out) const
{
	assert(SSE::isSSEAligned(&normal.x));
	assert(SSE::isSSEAligned(&ray));

	const __m128 raydir = _mm_load_ps(&ray.unitDirF().x);
	const __m128 n = _mm_load_ps(&normal.x);
	const __m128 d = dotSSEIn4Vec(raydir, n);

	const __m128 raystartpos = _mm_load_ps(&ray.startPosF().x);
	const __m128 raydist_numerator = _mm_sub_ss(_mm_load_ss(&this->dist), dotSSEIn4Vec(raystartpos, n));

	if(_mm_comieq_ss(d, zeroVec()) != 0) // if(d.m128_f32[0] == 0.0f)
		return 0;

	const __m128 raydist_v = _mm_div_ss(raydist_numerator, d);

	//if(raydist < BADOUEL_MIN_DIST || raydist >= ray_t_max) // if ray heading away form tri plane
	//	return 0;
	const __m128 res = _mm_or_ps(
		_mm_cmplt_ss(raydist_v, _mm_load_ss(&BADOUEL_MIN_DIST)), // raydist < BADOUEL_MIN_DIST ? 0xFFFFFFFF : 0x0
		_mm_cmple_ss(_mm_load_ss(&ray_t_max), raydist_v) // ray_t_max <= raydist ? 0xFFFFFFFF : 0x0
		);

	SSE_ALIGN UnionVec4 res_;
	_mm_store_ss(res_.f, res);
	if(res_.i[0] != 0) // if(res.m128_i32[0] != 0)
		return 0;

	const __m128 uv_vec_v = _mm_add_ps(raystartpos, _mm_mul_ps(raydir, shuffle4Vec(raydist_v, raydist_v, SHUF_X, SHUF_X, SHUF_X, SHUF_X)));

	SSE_ALIGN float uv_vec[4];
	_mm_store_ps(uv_vec, uv_vec_v);


	const float u = uv_vec[project_axis_1] - v0_1;  // ray.startPosF()[project_axis_1] + ray.unitDirF()[project_axis_1] * raydist - v0_1;
	const float v = uv_vec[project_axis_2] - v0_2;  // ray.startPosF()[project_axis_2] + ray.unitDirF()[project_axis_2] * raydist - v0_2;
	const float alpha = t11*u + t12*v;
	const float beta = t21*u + t22*v;
	assert(!isNAN(alpha) && !isNAN(beta));

	_mm_store_ss(&dist_out, raydist_v); // dist_out = raydist_v.m128_f32[0];
	u_out = alpha;
	v_out = beta;

#if defined(WIN32) && !defined(WIN64)
	const float one = 1.0;
	SSE_ALIGN unsigned int hit;
	_asm
	{
		movss	xmm0, alpha		; xmm0 := alpha
		movss	xmm1, beta		; xmm1 := beta
		;;;movaps	xmm6, one_4vec	; xmm6 := 1.0
		movss	xmm6, one		; xmm6 := 1.0
		xorps   xmm2, xmm2		; xmm2 := xmm2 ^ xmm2 = 0.0
		movaps	xmm3, xmm2		; xmm3 := 0.0
		cmpless	xmm3, xmm0		; xmm3 := (0.0 <= alpha) ? 0xFFFFFFFF : 0x0
		movaps	xmm4, xmm2		; xmm4 := 0.0
		cmpless	xmm4, xmm1		; xmm4 := (0.0 <= beta) ? 0xFFFFFFFF : 0x0
		andps	xmm3, xmm4		; xmm3 := (xmm3 && xmm4) == (0.0 <= alpha) && (0.0 <= beta) ? 0xFFFFFFFF : 0x0
		movaps  xmm5, xmm0		; xmm5 := alpha
		addss	xmm5, xmm1		; xmm5 := alpha + beta
		cmpless xmm5, xmm6		; xmm5 := alpha + beta <= 1.0 ? 0xFFFFFFFF : 0x0
		andps	xmm3, xmm5		; xmm3 := (xmm3 && xmm5) == (0.0 <= alpha) && (0.0 <= beta) && (alpha + beta <= 1.0) ? 0xFFFFFFFF : 0x0
		movss	hit, xmm3		; hit := xmm3
	}
	return hit;
#elif defined(WIN64)
	return unsigned int(alpha >= 0.0f && beta >= 0.0f && (alpha + beta) <= 1.0f);

	/*return _mm_and_ps( // (alpha >= 0.0) && (beta >= 0.0) && (alpha + beta <= 1.0) ? 0xFFFFFFFF : 0x0
		_mm_and_ps( // (alpha >= 0.0) && (beta >= 0.0) ? 0xFFFFFFFF : 0x0
			_mm_cmpge_ss(loadScalarLow(&alpha), zeroVec()), // alpha >= 0.0 ? 0xFFFFFFFF : 0x0
			_mm_cmpge_ss(loadScalarLow(&beta), zeroVec()) // beta >= 0.0 ? 0xFFFFFFFF : 0x0
			),
		_mm_cmple_ss( // alpha + beta <= 1.0 ? 0xFFFFFFFF : 0x0
			_mm_add_ss(loadScalarLow(&alpha), loadScalarLow(&beta)), //alpha + beta
			_mm_load_ps(one_4vec) // 1.0
			)
		).m128_u32[0];*/
#else
	// GCC
	//return unsigned int(alpha >= 0.0f && beta >= 0.0f && (alpha + beta) <= 1.0f);
	return (unsigned int)(alpha >= 0.0f && beta >= 0.0f && (alpha + beta) <= 1.0f); //TEMP
#endif
}


} //end namespace js


#endif //__BADOUELTRI_H_666_
