/*=====================================================================
edgetri.h
---------
File created by ClassTemplate on Mon Nov 22 04:32:53 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __EDGETRI_H_666_
#define __EDGETRI_H_666_


#include "../maths/vec3.h"
#include "../maths/PaddedVec3.h"
#include "../maths/SSE.h"
#include "../simpleraytracer/ray.h"



namespace js
{


/*=====================================================================
EdgeTri
-------
Small, compact triangle class.
Designed for fast Trumbore-Moller ray/tri collision
=====================================================================*/
class EdgeTri
{
public:
	/*=====================================================================
	EdgeTri
	-------
	
	=====================================================================*/
	inline EdgeTri(){}
	EdgeTri(const Vec3& v0, const Vec3& v1, const Vec3& v2);

	~EdgeTri();

	void set(const Vec3& v0, const Vec3& v1, const Vec3& v2);

#ifdef DEBUG
	inline
#else
	__forceinline 
#endif
	bool rayIntersect(const Ray& ray, float ray_t_max, float& dist_out, float& u_out, float& v_out);
	//float rayIntersect(const Ray& ray, float& u_out, float& v_out);

/*#ifdef DEBUG
	inline
#else
	__forceinline 
#endif*/
	inline
	void rayBundleIntersect(const PaddedVec3& rayorigin, 
						const SSE4Vec& raydirs_x,
						const SSE4Vec& raydirs_y,
						const SSE4Vec& raydirs_z,
						SSE4Vec& maxdists,
						SSE4Vec& dists_out,
						SSE4Vec& u_out,
						SSE4Vec& v_out,
						SSE4Vec& hitmask_out);

	inline const Vec3 getVertex(int index) const;
	inline const Vec3& v0() const { return vert0; }
	inline const Vec3 v1() const { return vert0 + edge1; }
	inline const Vec3 v2() const { return vert0 + edge2; }

	inline const Vec3 getNormal() const;


	/*Vec3 vert0;
	Vec3 edge1; //v1 - v0
	Vec3 edge2; //v2 - v0*/
	SSE_ALIGN PaddedVec3 vert0;
	SSE_ALIGN PaddedVec3 edge1;
	SSE_ALIGN PaddedVec3 edge2;


//	int num_leaf_tris;
//	char padding[12];//pad to 48 bytes

};


const Vec3 EdgeTri::getVertex(int index) const
{
	if(index == 0)
		return vert0;
	else if(index == 1)
		return vert0 + edge1;
	else if(index == 2)
		return vert0 + edge2;
	else
	{
		assert(0);
		return Vec3(0,0,0);
	}
}


const Vec3 EdgeTri::getNormal() const
{
	return normalise(::crossProduct(edge1, edge2));
}



const float EPSILON = 0.0000001f;
SSE_ALIGN const float EPSILON_f[4] = { EPSILON, EPSILON, EPSILON, EPSILON };
SSE_ALIGN const float NEG_EPSILON_f[4] = { -EPSILON, -EPSILON, -EPSILON, -EPSILON };

SSE_ALIGN const float zero[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
SSE_ALIGN const float one[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
SSE_ALIGN const float missed_dists[4] = { 2e9f, 2e9f, 2e9f, 2e9f };

/*#ifdef DEBUG
	inline
#else
	__forceinline 
#endif*/

void EdgeTri::rayBundleIntersect(const PaddedVec3& rayorigin, 
						const SSE4Vec& raydirs_x,
						const SSE4Vec& raydirs_y,
						const SSE4Vec& raydirs_z,
						SSE4Vec& maxdists,
						SSE4Vec& dists_out,
						SSE4Vec& u_out,
						SSE4Vec& v_out,
						SSE4Vec& hitmask_out)
{
	assertSSEAligned(&edge1.x);
	assertSSEAligned(&edge2.x);

	//const SSE4Vec e1 = load4Vec(&edge1.x);
	//const SSE4Vec e2 = load4Vec(&edge2.x);
	
	//------------------------------------------------------------------------
	//calc pvec.  Different for each ray.
	//------------------------------------------------------------------------
	//	const SSE4Vec pvec = crossSSE( load4Vec(&ray.unitdir.x), e2 );

	const SSE4Vec pvec_x = sub4Vec( mult4Vec(raydirs_y, loadScalarCopy(&edge2.z)), mult4Vec(raydirs_z, loadScalarCopy(&edge2.y)) );
	const SSE4Vec pvec_y = sub4Vec( mult4Vec(raydirs_z, loadScalarCopy(&edge2.x)), mult4Vec(raydirs_x, loadScalarCopy(&edge2.z)) );
	const SSE4Vec pvec_z = sub4Vec( mult4Vec(raydirs_x, loadScalarCopy(&edge2.y)), mult4Vec(raydirs_y, loadScalarCopy(&edge2.x)) );

	//	const float det = dotSSE( e1, pvec );

	SSE4Vec det = mult4Vec( loadScalarCopy(&edge1.x), pvec_x );
	det = add4Vec(det, mult4Vec( loadScalarCopy(&edge1.y), pvec_y ) );
	det = add4Vec(det, mult4Vec( loadScalarCopy(&edge1.z), pvec_z ) );

	//	const float inv_det = 1.0f / det;//TEMP MOVED TO ABOVE FABS TEST

	//const SSE4Vec inv_det = recip4Vec(det);
	const SSE4Vec inv_det = div4Vec(load4Vec(one), det);

	//if(fabs(det) < EPSILON)//det > -EPSILON && det < EPSILON)
	//	return -1.0f;

	//NOTE: this does all of them at once?
	/*if(allLessThan(det, load4Vec(EPSILON_f)) && allLessThan(load4Vec(NEG_EPSILON_f), det))
	{
		dists_out = load4Vec(missed_dists);
		return;
	}*/
	hitmask_out = or4Vec(
		lessThan4Vec(load4Vec(EPSILON_f), det),//if det > EPSILON
		lessThan4Vec(det, load4Vec(NEG_EPSILON_f)));//if dev < -EPSILON

	if(noneTrue(hitmask_out))
		return;

	//tvec is raydir-independent
	const SSE4Vec tvec = sub4Vec( load4Vec(&rayorigin.x), load4Vec(&vert0.x) );

	//	u_out = dotSSE(tvec, pvec) * inv_det;

	SSE4Vec t_dot_p = load4Vec(zero);
	t_dot_p.m128_f32[0] = 
		(pvec_x.m128_f32[0] * tvec.m128_f32[0]) + 
		(pvec_y.m128_f32[0] * tvec.m128_f32[1]) + 
		(pvec_z.m128_f32[0] * tvec.m128_f32[2]);

	t_dot_p.m128_f32[1] = 
		(pvec_x.m128_f32[1] * tvec.m128_f32[0]) + 
		(pvec_y.m128_f32[1] * tvec.m128_f32[1]) + 
		(pvec_z.m128_f32[1] * tvec.m128_f32[2]);

	t_dot_p.m128_f32[2] = 
		(pvec_x.m128_f32[2] * tvec.m128_f32[0]) + 
		(pvec_y.m128_f32[2] * tvec.m128_f32[1]) + 
		(pvec_z.m128_f32[2] * tvec.m128_f32[2]);

	t_dot_p.m128_f32[3] = 
		(pvec_x.m128_f32[3] * tvec.m128_f32[0]) + 
		(pvec_y.m128_f32[3] * tvec.m128_f32[1]) + 
		(pvec_z.m128_f32[3] * tvec.m128_f32[2]);

	u_out = mult4Vec(t_dot_p, inv_det);

	//if(u_out < 0.0f || u_out > 1.0f)
	//	return -1.0f;

	//if u coord is out of range for all rays...
	/*if(allLessThan(u_out, load4Vec(zero)) || allLessThan(load4Vec(one), u_out))
	{
		dists_out = load4Vec(missed_dists);
		return;
	}*/
	hitmask_out = and4Vec(hitmask_out, and4Vec( 
		lessThan4Vec(load4Vec(zero), u_out), //0 < u
		lessThan4Vec(u_out, load4Vec(one))));//u < 1

	if(noneTrue(hitmask_out))
		return;

	//prepare to test V parameter

	//qvec is also raydir-independent
	const SSE4Vec qvec = crossSSE(tvec, load4Vec(&edge1.x));
	
	//calculate V parameter and test bounds
	//v_out = ray.unitdir.dot(qvec) * inv_det;
	//v_out = dotSSE( load4Vec(&ray.unitdir.x), qvec ) * inv_det;

	//if(v_out < 0.0f || u_out + v_out > 1.0f)
	//	return -1.0f;


	SSE4Vec dir_dot_q = load4Vec(zero);
	dir_dot_q.m128_f32[0] = 
		(raydirs_x.m128_f32[0] * qvec.m128_f32[0]) + 
		(raydirs_y.m128_f32[0] * qvec.m128_f32[1]) + 
		(raydirs_z.m128_f32[0] * qvec.m128_f32[2]);

	dir_dot_q.m128_f32[1] = 
		(raydirs_x.m128_f32[1] * qvec.m128_f32[0]) + 
		(raydirs_y.m128_f32[1] * qvec.m128_f32[1]) + 
		(raydirs_z.m128_f32[1] * qvec.m128_f32[2]);

	dir_dot_q.m128_f32[2] = 
		(raydirs_x.m128_f32[2] * qvec.m128_f32[0]) + 
		(raydirs_y.m128_f32[2] * qvec.m128_f32[1]) + 
		(raydirs_z.m128_f32[2] * qvec.m128_f32[2]);

	dir_dot_q.m128_f32[3] = 
		(raydirs_x.m128_f32[3] * qvec.m128_f32[0]) + 
		(raydirs_y.m128_f32[3] * qvec.m128_f32[1]) + 
		(raydirs_z.m128_f32[3] * qvec.m128_f32[2]);

	v_out = mult4Vec(dir_dot_q, inv_det);

	//if(v_out < 0.0f || u_out + v_out > 1.0f)
	//	return -1.0f;

	//if v coord is out of range for all rays...
	/*if(allLessThan(v_out, load4Vec(zero)) || allLessThan(load4Vec(one), add4Vec(u_out, v_out)))
	{
		dists_out = load4Vec(missed_dists);
		return;
	}*/
	hitmask_out = and4Vec(hitmask_out, and4Vec( 
		lessThan4Vec(load4Vec(zero), v_out), //0 < v
		lessThan4Vec(add4Vec(u_out, v_out), load4Vec(one))));//u+v < 1
	
	//NOTE: could test for all missing here.


	//	return dotSSE(e2, qvec) * inv_det;

	const float e2_dot_q = dotSSE(load4Vec(&edge2.x), qvec);

	dists_out = mult4Vec( loadScalarCopy(&e2_dot_q), inv_det);
}




//http://www.acm.org/jgt/papers/MollerTrumbore97/

//#define EPSILON 0.000001

//#define SSE_RAYTEST 1

//#define TEST_CULL 1


#ifdef DEBUG
	inline
#else
	__forceinline 
#endif
bool EdgeTri::rayIntersect(const Ray& ray, float ray_t_max, float& dist_out, float& u_out, float& v_out)
{
#ifdef SSE_RAYTEST
	assertSSEAligned(&edge1.x);
	assertSSEAligned(&edge2.x);
	assertSSEAligned(&ray.unitdir.x);
	assertSSEAligned(&ray.startpos.x);

	//SSE_ALIGN float temp[4];
	//temp[0] = vert0.0
	/*SSE_ALIGN PaddedVec3 temp;
	temp = vert0;
	const SSE4Vec v0 = load4Vec(&temp.x);
	temp = edge1;
	const SSE4Vec e1 = load4Vec(&temp.x);
	temp = edge2;
	const SSE4Vec e2 = load4Vec(&temp.x);*/

	const SSE4Vec e1 = load4Vec(&edge1.x);
	const SSE4Vec e2 = load4Vec(&edge2.x);


	const SSE4Vec pvec = crossSSE( load4Vec(&ray.unitdir.x), e2 );

	const float det = dotSSE( e1, pvec );

	const float inv_det = 1.0f / det;//TEMP MOVED TO ABOVE FABS TEST

	//cull test goes here.

	if(fabs(det) < EPSILON)//det > -EPSILON && det < EPSILON)
		return false;

	//was here


	//calculate distance from vert0 to ray origin
	//const Vec3 tvec = ray.startpos - vert0;
	SSE4Vec tvec = sub4Vec( load4Vec(&ray.startpos.x), load4Vec(&vert0.x) );

	//calculate U parameter and test bounds
	//u_out = tvec.dot(pvec) * inv_det;
	u_out = dotSSE(tvec, pvec) * inv_det;

	if(u_out < 0.0f || u_out > 1.0f)
		return false;

	//prepare to test V parameter
	//const Vec3 qvec = ::crossProduct(tvec, edge1);
	SSE4Vec qvec = crossSSE( tvec, e1 );


	//calculate V parameter and test bounds
	//v_out = ray.unitdir.dot(qvec) * inv_det;
	v_out = dotSSE( load4Vec(&ray.unitdir.x), qvec ) * inv_det;

	if(v_out < 0.0f || u_out + v_out > 1.0f)
		return false;



	//calculate t, ray intersects triangle
	//const float t = edge2.dot(qvec) * inv_det;
	const float t = dotSSE(e2, qvec) * inv_det;

	if(t < ray_t_max)
	{
		dist_out = t;
		return true;
	}
	else
	{
		return false;
	}

#else

	//float texu, texv;

	//find vectors for two edges sharing vert0
	//const Vec3 edge1 = v[1] - v[0];
	//const Vec3 edge2 = v[2] - v[0];

	//begin calculating determinant - also used to calculate U parameter
	const Vec3 pvec = ::crossProduct(ray.unitdir, edge2);
	//Vec3 pvec;
	//crossProduct(ray.unitdir, edge2, pvec);

	//if determinant is near zero, ray lies in plane of triangle
	const float det = edge1.dot(pvec);

#ifdef TEST_CULL           /* define TEST_CULL if culling is desired */
   if (det < EPSILON)
      return 0;

	/* calculate distance from vert0 to ray origin */
	//SUB(tvec, orig, vert0);
   const Vec3 tvec = ray.startpos - vert0;

   /* calculate U parameter and test bounds */
   u_out = tvec.dot(pvec);//DOT(tvec, pvec);
   if(u_out < 0.0 || u_out > det)
      return false;

   /* prepare to test V parameter */
   //CROSS(qvec, tvec, edge1);
   const Vec3 qvec = ::crossProduct(tvec, edge1);

    /* calculate V parameter and test bounds */
   //v_out = //DOT(dir, qvec);
	v_out = ray.unitdir.dot(qvec);
   if (v_out < 0.0 || u_out + v_out > det)
		return false;

   /* calculate t, scale parameters, ray intersects triangle */
   float t = edge2.dot(qvec);//DOT(edge2, qvec);
   const float inv_det = 1.0f / det;
   t *= inv_det;
   u_out *= inv_det;
   v_out *= inv_det;

#else                    /* the non-culling branch */

	if(det > -EPSILON && det < EPSILON)
		return false;

	const float inv_det = 1.0f / det;

	//calculate distance from vert0 to ray origin
	const Vec3 tvec = ray.startpos - vert0;
	//Vec3 tvec;
	//sub(ray.startpos, vert0, tvec);

	//calculate U parameter and test bounds
	u_out = tvec.dot(pvec) * inv_det;
	if(u_out < 0.0f || u_out > 1.0f)
		return false;

	//prepare to test V parameter
	const Vec3 qvec = ::crossProduct(tvec, edge1);
	//Vec3 qvec;
	//crossProduct(tvec, edge1, qvec);


	//calculate V parameter and test bounds
	v_out = ray.unitdir.dot(qvec) * inv_det;
	if(v_out < 0.0f || u_out + v_out > 1.0f)
		return false;

	//calculate t, ray intersects triangle
	const float t = edge2.dot(qvec) * inv_det;

#endif

	if(t >= ray_t_max)
		return false;

   //return 1;
	dist_out = t;
	return true;

#endif //SSE_RAYTEST
}



} //end namespace js



#endif //__EDGETRI_H_666_
