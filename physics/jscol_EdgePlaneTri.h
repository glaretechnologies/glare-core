/*=====================================================================
EdgePlaneTri.h
--------------
File created by ClassTemplate on Mon Jun 27 00:20:26 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __EDGEPLANETRI_H_666_
#define __EDGEPLANETRI_H_666_



#include "../maths/vec3.h"
#include "../maths/SSE.h"
#include "../simpleraytracer/ray.h"

namespace js
{




/*=====================================================================
EdgePlaneTri
------------

=====================================================================*/
class EdgePlaneTri
{
public:
	/*=====================================================================
	EdgePlaneTri
	------------
	
	=====================================================================*/
	EdgePlaneTri();

	~EdgePlaneTri();

	void set(const Vec3& v0, const Vec3& v1, const Vec3& v2);
	void set(const Vec3& normal, float dist, const Vec3& v0, const Vec3& v1, const Vec3& v2);


	__forceinline float rayIntersect(const Ray& ray);

	inline
	void rayBundleIntersect(const Vec3& rayorigin, 
						const SSE4Vec& raydirs_x,
						const SSE4Vec& raydirs_y,
						const SSE4Vec& raydirs_z,
						SSE4Vec& dists_out,
						SSE4Vec& u_out,
						SSE4Vec& v_out,
						SSE4Vec& hitmask_out);


	SSE_ALIGN Vec3 normal;
	float dist;
	SSE_ALIGN Vec3 e0normal;
	float e0dist;
	SSE_ALIGN Vec3 e1normal;
	float e1dist;
	SSE_ALIGN Vec3 e2normal;
	float e2dist;
};

__forceinline float EdgePlaneTri::rayIntersect(const Ray& ray)
{
#ifdef USE_SSE
	const SSE4Vec trinormal = load4Vec(&this->normal.x);
	const SSE4Vec raystartpos = load4Vec(&ray.startpos.x);
	const SSE4Vec rayunitdir = load4Vec(&ray.unitdir.x);

	const float raydist = (this->dist - dotSSE(raystartpos, trinormal)) / dotSSE(rayunitdir, trinormal);

	if(raydist < 0.0f)//if ray heading away form tri plane
		return raydist;

	const SSE4Vec hitpos = add4Vec( raystartpos, multByScalar(rayunitdir, &raydist) );

	//------------------------------------------------------------------------
	//now test against edge planes
	//------------------------------------------------------------------------
	if(	(dotSSE( hitpos, load4Vec(&e0normal.x) ) > e0dist) ||
		(dotSSE( hitpos, load4Vec(&e1normal.x) ) > e1dist) ||
		(dotSSE( hitpos, load4Vec(&e2normal.x) ) > e2dist))
		return -1.0f;

	/*float planedist = dotSSE( hitpos, load4Vec(&e0normal.x) ) - e0dist;
	SSE4Vec d = loadScalarLow(&planedist);

	planedist = dotSSE( hitpos, load4Vec(&e1normal.x) ) - e1dist;
	d = max4Vec( d, loadScalarLow(&planedist) );

	planedist = dotSSE( hitpos, load4Vec(&e2normal.x) ) - e2dist;
	d = max4Vec( d, loadScalarLow(&planedist) );

	storeLowWord(d, &planedist);
	if(planedist > 0.0f)
		return -1.0f;*/

	return raydist;

#endif
	return 0;
}
/*
SSE_ALIGN const float zero_f[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
SSE_ALIGN const float one_f[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

void EdgePlaneTri::rayBundleIntersect(const Vec3& rayorigin, 
						const SSE4Vec& raydirs_x,
						const SSE4Vec& raydirs_y,
						const SSE4Vec& raydirs_z,
						SSE4Vec& dists_out,
						SSE4Vec& u_out,
						SSE4Vec& v_out,
						SSE4Vec& hitmask_out)
{


	//	const float raydist = (this->dist - dotSSE(raystartpos, trinormal)) / dotSSE(rayunitdir, trinormal);

	const SSE4Vec raydir_dot_n = add4Vec(
		mult4Vec(raydirs_x, loadScalarCopy(&normal.x)),
		add4Vec(
		mult4Vec(raydirs_y, loadScalarCopy(&normal.y)),
		mult4Vec(raydirs_z, loadScalarCopy(&normal.z))));

	const SSE4Vec recip_dots = div4Vec(load4Vec(one_f), raydir_dot_n);

	const SSE4Vec trinormal = load4Vec(&this->normal.x);
	const SSE4Vec raystartpos = load4Vec(&rayorigin.x);

	const SSE4Vec raydist = mult4Vec( sub4Vec( loadScalarCopy(&dist), dotSSE(raystartpos, trinormal) ), 
		recip_dots );

	//if(raydist < 0.0f)//if ray heading away form tri plane
	//	return raydist;


	hitmask_out = lessThan4Vec(loadScalarCopy(zero_f), raydist);
	
	//const SSE4Vec hitpos = add4Vec( raystartpos, multByScalar(rayunitdir, &raydist) );

	const SSE4Vec hitpos_x = add4Vec( loadScalarCopy(&rayorigin.x), mult4Vec(raydirs_x, raydist) );
	const SSE4Vec hitpos_y = add4Vec( loadScalarCopy(&rayorigin.y), mult4Vec(raydirs_y, raydist) );
	const SSE4Vec hitpos_z = add4Vec( loadScalarCopy(&rayorigin.z), mult4Vec(raydirs_z, raydist) );

	//------------------------------------------------------------------------
	//now test against edge planes
	//------------------------------------------------------------------------
	if(	(dotSSE( hitpos, load4Vec(&e0normal.x) ) > e0dist) ||
		(dotSSE( hitpos, load4Vec(&e1normal.x) ) > e1dist) ||
		(dotSSE( hitpos, load4Vec(&e2normal.x) ) > e2dist))
		return -1.0f;



}
*/




} //end namespace js


#endif //__EDGEPLANETRI_H_666_




