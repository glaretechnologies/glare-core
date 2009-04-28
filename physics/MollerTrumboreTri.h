/*=====================================================================
MollerTrumboreTri.h
-------------------
File created by ClassTemplate on Mon Nov 03 16:40:40 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MOLLERTRUMBORETRI_H_666_
#define __MOLLERTRUMBORETRI_H_666_


#include "../maths/vec3.h"
#include "../maths/SSE.h"
#include "../simpleraytracer/ray.h"


namespace js
{


/*=====================================================================
MollerTrumboreTri
-----------------

=====================================================================*/
class MollerTrumboreTri
{
public:
	inline MollerTrumboreTri(){}

	inline ~MollerTrumboreTri(){}


	void set(const Vec3f& v0, const Vec3f& v1, const Vec3f& v2);


	// returns mask == 0xFFFFFF ? b : a
	static inline __m128 condMov(__m128 a, __m128 b, __m128 mask)
	{
		b = _mm_and_ps(b, mask);
		a = _mm_andnot_ps(mask, a);
		return _mm_or_ps(a, b);
	}


	template <int i>
	static inline __m128 copyToAll(__m128 a)
	{
		return _mm_shuffle_ps(a, a, _MM_SHUFFLE(i, i, i, i));
	}


	inline unsigned int rayIntersectSSE(const Ray& ray, float ray_t_max, float& dist_out, float& u_out, float& v_out) const //non zero if hit
	{
		const Vec4f v0(data[0], data[1], data[2], 1.0f);
		const Vec4f e1(data[3], data[4], data[5], 0.0f);
		const Vec4f e2(data[6], data[7], data[8], 0.0f);

		const Vec4f pvec(crossProduct(ray.unitDir(), e2));

		const float det = dot(e1, pvec);

		const float inv_det = 1.0f / det;

		const Vec4f tvec = ray.startPos() - v0;

		const float u = dot(tvec, pvec) * inv_det;
		if(u < 0.0f || u > 1.0f)
			return 0;

		const Vec4f qvec = crossProduct(tvec, e1);

		const float v = dot(ray.unitDir(), qvec) * inv_det;
		if(v < 0.0f || (u + v) > 1.0f)
			return 0;

		const float t = dot(e2, qvec) * inv_det;

		if(t < 0.0f)
			return 0;

		if(t >= ray_t_max)
			return 0;

		dist_out = t;
		u_out = u;
		v_out = v;

		return 1;
	}


	inline unsigned int rayIntersect(const Ray& ray, float ray_t_max, float& dist_out, float& u_out, float& v_out) const //non zero if hit
	{
		const Vec3f v0(data);
		const Vec3f e1(data + 3);
		const Vec3f e2(data + 6);

		const Vec3f orig(ray.startPosF().x[0], ray.startPosF().x[1], ray.startPosF().x[2]);
		const Vec3f dir(ray.unitDirF().x[0], ray.unitDirF().x[1], ray.unitDirF().x[2]);
		const Vec3f pvec = crossProduct(dir, e2);

		const float det = dot(e1, pvec);

		const float inv_det = 1.0f / det;

		const Vec3f tvec = orig - v0;

		const float u = dot(tvec, pvec) * inv_det;
		if(u < 0.0f || u > 1.0f)
			return 0;

		const Vec3f qvec = crossProduct(tvec, e1);

		const float v = dot(dir, qvec) * inv_det;
		if(v < 0.0f || (u + v) > 1.0f)
			return 0;

		const float t = dot(e2, qvec) * inv_det;

		if(t < 0.0f)
			return 0;

		if(t >= ray_t_max)
			return 0;

		dist_out = t;
		u_out = u;
		v_out = v;

		return 1;
	}


	// Reference implementation: Intersects one ray with one triangle.
	inline bool referenceIntersect(
		const Ray& ray,
		float* u_out,
		float* v_out,
		float* t_out
		) const
	{
		const Vec3f v0(data);
		const Vec3f e1(data + 3);
		const Vec3f e2(data + 6);

		const Vec3f orig(ray.startPosF().x[0], ray.startPosF().x[1], ray.startPosF().x[2]);
		const Vec3f dir(ray.unitDirF().x[0], ray.unitDirF().x[1], ray.unitDirF().x[2]);
		const Vec3f pvec = crossProduct(dir, e2);

		const float det = dot(e1, pvec);

		const float inv_det = 1.0f / det;

		const Vec3f tvec = orig - v0;

		const float u = dot(tvec, pvec) * inv_det;
		if(u < 0.0f || u > 1.0f)
			return false;

		const Vec3f qvec = crossProduct(tvec, e1);

		const float v = dot(dir, qvec) * inv_det;
		if(v < 0.0f || (u + v) > 1.0f)
			return false;

		const float t = dot(e2, qvec) * inv_det;

		if(t < 0.0f)
			return false;

		*t_out = t;
		*u_out = u;
		*v_out = v;

		return true;
	}


	inline static __m128 loadDataItem(
		const float* t0data,
		const float* t1data,
		const float* t2data,
		const float* t3data,
		unsigned int offset
		)
	{
		// t0data = [t0_v0x, t0_v0y, t0_v0z, t0_e1x, t0_e1y, t0_e1z, t0_e2x, t0_e2y, t0_e2z]
		const __m128 a = _mm_load_ss(t0data + offset);		// a = [0, 0, 0, t0]
		const __m128 b = _mm_load_ss(t1data + offset);		// b = [0, 0, 0, t1]
		const __m128 c = _mm_load_ss(t2data + offset);		// c = [0, 0, 0, t2]
		const __m128 d = _mm_load_ss(t3data + offset);		// d = [0, 0, 0, t3]

		// tmp1 = [t1, t1, t0, t0]
		// tmp2 = [t3, t3, t2, t2]
		const __m128 tmp1 = _mm_shuffle_ps(a, b, _MM_SHUFFLE(0, 0, 0, 0));
		const __m128 tmp2 = _mm_shuffle_ps(c, d, _MM_SHUFFLE(0, 0, 0, 0));

		// [t3, t2, t1, t0]
		return _mm_shuffle_ps(tmp1, tmp2, _MM_SHUFFLE(2, 0, 2, 0));
	}


	inline static void intersectTris(
		const Ray* const ray,
		const float* const t0data,
		const float* const t1data,
		const float* const t2data,
		const float* const t3data,
		UnionVec4* u_out,
		UnionVec4* v_out,
		UnionVec4* t_out,
		UnionVec4* hit_out
		)
	{
		// TODO: try shuffling here, might be faster.
		const __m128 orig_x = _mm_load_ps1(ray->startPosF().x);
		const __m128 orig_y = _mm_load_ps1(ray->startPosF().x+1);
		const __m128 orig_z = _mm_load_ps1(ray->startPosF().x+2);
		const __m128 dir_x = _mm_load_ps1(ray->unitDirF().x);
		const __m128 dir_y = _mm_load_ps1(ray->unitDirF().x+1);
		const __m128 dir_z = _mm_load_ps1(ray->unitDirF().x+2);

		const __m128 one  = _mm_set_ps1(1.0f);
		const __m128 zero = _mm_set_ps1(0.0f);

		const __m128 v0x = loadDataItem(t0data, t1data, t2data, t3data, 0);
		const __m128 v0y = loadDataItem(t0data, t1data, t2data, t3data, 1);
		const __m128 v0z = loadDataItem(t0data, t1data, t2data, t3data, 2);

		const __m128 edge1_x = loadDataItem(t0data, t1data, t2data, t3data, 3);
		const __m128 edge1_y = loadDataItem(t0data, t1data, t2data, t3data, 4);
		const __m128 edge1_z = loadDataItem(t0data, t1data, t2data, t3data, 5);

		const __m128 edge2_x = loadDataItem(t0data, t1data, t2data, t3data, 6);
		const __m128 edge2_y = loadDataItem(t0data, t1data, t2data, t3data, 7);
		const __m128 edge2_z = loadDataItem(t0data, t1data, t2data, t3data, 8);

		#ifdef DEBUG
		// Check data was loaded correctly.
		SSE_ALIGN float temp[4]; 
		_mm_store_ps(temp, v0x); 
		assert(temp[0] == t0data[0]);
		assert(temp[1] == t1data[0]);
		assert(temp[2] == t2data[0]);
		assert(temp[3] == t3data[0]);
		_mm_store_ps(temp, v0y); 
		assert(temp[0] == t0data[1]);
		assert(temp[1] == t1data[1]);
		assert(temp[2] == t2data[1]);
		assert(temp[3] == t3data[1]);
		#endif

		/* cross(v1, v2):
		(v1.y * v2.z) - (v1.z * v2.y),
		(v1.z * v2.x) - (v1.x * v2.z),
		(v1.x * v2.y) - (v1.y * v2.x)
		*/

		// pvec = cross(dir, edge2)
		const __m128 pvec_x = _mm_sub_ps(_mm_mul_ps(dir_y, edge2_z), _mm_mul_ps(dir_z, edge2_y));
		const __m128 pvec_y = _mm_sub_ps(_mm_mul_ps(dir_z, edge2_x), _mm_mul_ps(dir_x, edge2_z));
		const __m128 pvec_z = _mm_sub_ps(_mm_mul_ps(dir_x, edge2_y), _mm_mul_ps(dir_y, edge2_x));


		// det = dot(edge1, pvec)
		const __m128 det = _mm_add_ps(_mm_mul_ps(edge1_x, pvec_x), _mm_add_ps(_mm_mul_ps(edge1_y, pvec_y), _mm_mul_ps(edge1_z, pvec_z)));

		// TODO?: det ~= 0 test

		const __m128 inv_det = _mm_div_ps(one, det);

		// tvec = orig - vert0
		const __m128 tvec_x = _mm_sub_ps(orig_x, v0x);
		const __m128 tvec_y = _mm_sub_ps(orig_y, v0y);
		const __m128 tvec_z = _mm_sub_ps(orig_z, v0z);

		// u = dot(tvec, pvec) * inv_det
		const __m128 u = _mm_mul_ps(
			_mm_add_ps(_mm_mul_ps(tvec_x, pvec_x), _mm_add_ps(_mm_mul_ps(tvec_y, pvec_y), _mm_mul_ps(tvec_z, pvec_z))),
			inv_det
			);

		// qvec = cross(tvec, edge1)
		const __m128 qvec_x = _mm_sub_ps(_mm_mul_ps(tvec_y, edge1_z), _mm_mul_ps(tvec_z, edge1_y));
		const __m128 qvec_y = _mm_sub_ps(_mm_mul_ps(tvec_z, edge1_x), _mm_mul_ps(tvec_x, edge1_z));
		const __m128 qvec_z = _mm_sub_ps(_mm_mul_ps(tvec_x, edge1_y), _mm_mul_ps(tvec_y, edge1_x));

		// v = dot(dir, qvec) * inv_det
		const __m128 v = _mm_mul_ps(
			_mm_add_ps(_mm_mul_ps(dir_x, qvec_x), _mm_add_ps(_mm_mul_ps(dir_y, qvec_y), _mm_mul_ps(dir_z, qvec_z))),
			inv_det
			);

		// t = dot(edge2, qvec) * inv_det
		const __m128 t = _mm_mul_ps(
			_mm_add_ps(_mm_mul_ps(edge2_x, qvec_x), _mm_add_ps(_mm_mul_ps(edge2_y, qvec_y), _mm_mul_ps(edge2_z, qvec_z))),
			inv_det
			);

		// hit = (t >= 0.0 && u >= 0.0 && u <= 1.0 && v >= 0.0 && u+v <= 1.0)
		const __m128 hit = _mm_and_ps(
				_mm_cmpge_ps(t, zero),
				_mm_and_ps(
					_mm_and_ps(_mm_cmpge_ps(u, zero), _mm_cmple_ps(u, one)),
					_mm_and_ps(_mm_cmpge_ps(v, zero), _mm_cmple_ps(_mm_add_ps(u, v), one))
				)
			);

		hit_out->v = hit;
		u_out->v = u;
		v_out->v = v;
		t_out->v = t;
	}	


	inline const Vec3f getNormal() const
	{
		return normalise(crossProduct(Vec3f(data[3], data[4], data[5]), Vec3f(data[6], data[7], data[8])));
	}

	float data[9]; // 9 * 4 bytes = 36 bytes.
};


} //end namespace js


#endif //__MOLLERTRUMBORETRI_H_666_
