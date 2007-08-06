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
	/*=====================================================================
	BadouelTri
	----------
	
	=====================================================================*/
	BadouelTri();

	~BadouelTri();

	void set(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2);

	inline unsigned int rayIntersect(const Ray& ray, float ray_t_max, float& dist_out, float& u_out, float& v_out); //non zero if hit

	inline const Vec3f& getNormal() const { return normal; }

private:
	SSE_ALIGN Vec3f normal;
	float dist;

	unsigned int project_axis_1;
	unsigned int project_axis_2;

	float v0_1;
	float v0_2;
	float t11, t12, t21, t22;

};

const float BADOUEL_MIN_DIST = 0.00000001f;//this is to avoid denorms


unsigned int BadouelTri::rayIntersect(const Ray& ray, float ray_t_max, float& dist_out, float& u_out, float& v_out)
{

	//------------------------------------------------------------------------
	//SSE version
	//------------------------------------------------------------------------
	/*const SSE4Vec trinormal = load4Vec(&this->normal.x);
	const SSE4Vec raystartpos = load4Vec(&ray.startpos.x);
	const SSE4Vec rayunitdir = load4Vec(&ray.unitdir.x);

	//SSE4Vec hit = zeroVec();

	//const float raydist = (this->dist - dotSSE(raystartpos, trinormal)) / dotSSE(rayunitdir, trinormal);
	const SSE4Vec raydist = div4Vec( sub4Vec(loadScalarCopy(&this->dist), dotSSEIn4Vec(raystartpos, trinormal)),
							dotSSEIn4Vec(rayunitdir, trinormal));
								

//	if(raydist < 0.0f)//if ray heading away form tri plane
//		return false;

	//const SSE4Vec hitpos = add4Vec( raystartpos, multByScalar(rayunitdir, &raydist) );
	const SSE4Vec hitpos = add4Vec( raystartpos, mult4Vec(rayunitdir, raydist) );

	const float u = hitpos.m128_f32[project_axis_1] - v0_1;
	const float v = hitpos.m128_f32[project_axis_2] - v0_2;

	const float alpha_f = t11*u + t12*v;
	const float beta_f = t21*u + t22*v;

	const SSE4Vec alpha = loadScalarCopy(&alpha_f);
	const SSE4Vec beta = loadScalarCopy(&beta_f);

	if(allTrue(and4Vec(lessThan4Vec(zeroVec(), raydist), //0 < raydist
		and4Vec(lessThan4Vec(raydist, loadScalarCopy(&ray_t_max)),
		and4Vec(lessThan4Vec(zeroVec(), alpha), //0 < alpha
		and4Vec(lessThan4Vec(zeroVec(), beta),//0 < beta
		lessThan4Vec(add4Vec(alpha, beta), load4Vec(one_4vec))))))))//alpha + beta < 1
	{
		dist_out = raydist.m128_f32[0];
		u_out = u;
		v_out = v;
		return true;
	}
	else
	{
		return false;
	}*/

	//try and get the divide out of the way nice and early
	//const float recip_dot = 1.0f / dot(ray.unitdir, this->normal);

	//const float raydist = (this->dist - dot(ray.startpos, this->normal)) * recip_dot;

	//float raydist, u, v;
	//Vec3 hitpos;







#if 0 
	//pretty good version:
	//------------------------------------------------------------------------
	//non SSE version
	//------------------------------------------------------------------------
	const float denom = dot(ray.unitDirF(), this->normal);

	if(denom == 0.0f)
		return false;

	/*PaddedVec3 hitpos = ray.startPosF();
	const float raydist = (this->dist - dot(hitpos, this->normal)) / denom;

	if(raydist < BADOUEL_MIN_DIST || raydist >= ray_t_max)//1e20f)//if ray heading away form tri plane
		return false;
	
	hitpos.addMult(ray.unitDirF(), raydist);*/
	
	const float raydist = (this->dist - dot(ray.startPosF(), this->normal)) / denom;

	if(raydist < BADOUEL_MIN_DIST || raydist >= ray_t_max)//1e20f)//if ray heading away form tri plane
		return false;
	
	const PaddedVec3 hitpos = ray.startPosF() + ray.unitDirF() * raydist;

	const float u = hitpos[project_axis_1] - v0_1;
	const float v = hitpos[project_axis_2] - v0_2;

	const float alpha = t11*u + t12*v;
	const float beta = t21*u + t22*v;

#ifdef DEBUG
	if(isNAN(alpha) || isNAN(beta))
	{
		assert(0);
		return false;
	}
#endif

	if(alpha < 0.0f || beta < 0.0f ||(alpha + beta) > 1.0f)
		return false;
	else
	{
		dist_out = raydist;
		u_out = alpha;
		v_out = beta;
		return true;
	}
#endif


	//Latest Code

	const float denom = dot(ray.unitDirF(), this->normal);
	if(denom == 0.0f)
		return 0;

	const float raydist = (this->dist - dot(ray.startPosF(), this->normal)) / denom; //signed distance until ray intersects triangle plane
	if(raydist < BADOUEL_MIN_DIST || raydist >= ray_t_max)//if ray heading away form tri plane
		return 0;

	const float u = ray.startPosF()[project_axis_1] + ray.unitDirF()[project_axis_1] * raydist - v0_1;
	const float v = ray.startPosF()[project_axis_2] + ray.unitDirF()[project_axis_2] * raydist - v0_2;
	const float alpha = t11*u + t12*v;
	const float beta = t21*u + t22*v;
	assert(!isNAN(alpha) && !isNAN(beta));

	const float one = 1.0;
	dist_out = raydist;
	u_out = alpha;
	v_out = beta;

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






/*
	const float denom = dot(ray.unitDirF(), this->normal);

	if(denom != 0.0f)
	{
		const float raydist = (this->dist - dot(ray.startPosF(), this->normal)) / denom; //signed distance until ray intersects triangle plane

		if(raydist >= BADOUEL_MIN_DIST && raydist < ray_t_max)
		{
			const float u = ray.startPosF()[project_axis_1] + ray.unitDirF()[project_axis_1] * raydist - v0_1;
			const float v = ray.startPosF()[project_axis_2] + ray.unitDirF()[project_axis_2] * raydist - v0_2;
			const float alpha = t11*u + t12*v;
			const float beta = t21*u + t22*v;
			assert(!isNAN(alpha) && !isNAN(beta));
			
			const float one = 1.0;
			dist_out = raydist;
			u_out = alpha;
			v_out = beta;
	
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
		}
		else
		{
			return 0;
		}
	}
	else
	{
		return 0;
	}
*/






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

	/*
	if(alpha < 0.0f || beta < 0.0f || (alpha + beta) > 1.0f)
		return false;
	else
	{
		dist_out = raydist;
		u_out = alpha;
		v_out = beta;
		return true;
	}*/














	//------------------------------------------------------------------------
	//old crap
	//------------------------------------------------------------------------
	/*const float u0 = hitpos.m128_f32[project_axis_1] - u;
	const float v0 = hitpos.m128_f32[project_axis_2] - v;

	const float b = (v0*u1 - u0*v1) / (v2*u1 - u2*v1);
	if(b < 0.0f || b > 1.0f)
		return -1.0f;

	const float a = (u0 - b*u2) / u1;

	if(a < 0.0f || (a + b) > 1.0f)
		return -1.0f;

	u_out = a;
	v_out = b;
	return raydist;*/
}


} //end namespace js


#endif //__BADOUELTRI_H_666_




