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

	uint32 project_axis_1;
	uint32 project_axis_2;

	float v0_1;
	float v0_2;
	float t11, t12, t21, t22;
};


const float BADOUEL_MIN_DIST = 0.00000001f;//this is to avoid denorms

unsigned int BadouelTri::rayIntersect(const Ray& ray, float ray_t_max, float& dist_out, float& u_out, float& v_out)
{
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

#if defined(WIN32) && !defined(WIN64)
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
#else
	TODO
#endif
}


} //end namespace js


#endif //__BADOUELTRI_H_666_




