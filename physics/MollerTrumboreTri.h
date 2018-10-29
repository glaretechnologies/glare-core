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


template <typename real>
static inline bool rayTriScalar(const Vec3<real>& v0, const Vec3<real>& e1, const Vec3<real>& e2,
								const Vec3<real>& ray_orig, const Vec3<real>& ray_dir,
								const real t_min,
								real& dist_out, real& u_out, real& v_out)
{
	const Vec3<real> pvec = crossProduct(ray_dir, e2);

	const real det = dot(e1, pvec);
	const real inv_det = 1 / det;

	const Vec3<real> tvec = ray_orig - v0;

	const real u = dot(tvec, pvec) * inv_det;
	if(u < 0 || u > 1)
		return false;

	const Vec3<real> qvec = crossProduct(tvec, e1);

	const real v = dot(ray_dir, qvec) * inv_det;
	if(v < 0 || (u + v) > 1)
		return false;

	const real t = dot(e2, qvec) * inv_det;

	if(t < t_min)// || t >= ray.far)
		return false;

	dist_out = t;
	u_out = u;
	v_out = v;

	return true;
}


/*=====================================================================
MollerTrumboreTri
-----------------

=====================================================================*/
class MollerTrumboreTri
{
public:
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


	/*inline unsigned int rayIntersectSSE(const Ray& ray, float ray_t_max, float& dist_out, float& u_out, float& v_out) const //non zero if hit
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
	}*/


	inline unsigned int rayIntersect(const Ray& ray, float ray_t_max, float epsilon, float& dist_out, float& u_out, float& v_out) const //non zero if hit
	{
#if 1
		const Vec3f v0(data);
		const Vec3f e1(data + 3);
		const Vec3f e2(data + 6);

		const Vec3f orig(ray.startPosF().x[0], ray.startPosF().x[1], ray.startPosF().x[2]);
		const Vec3f dir(ray.unitDirF().x[0], ray.unitDirF().x[1], ray.unitDirF().x[2]);
		const Vec3f pvec = crossProduct(dir, e2);

#if USE_LAUNCH_NORMAL
		const Vec3f launch_n(ray.launchNormal().x[0], ray.launchNormal().x[1], ray.launchNormal().x[2]);
#endif

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

		//TEMP:
		//ray_t_min = crossProduct(e1, e2).length() * 0.0000001f;
		/*
		t_min = C * max(len_1, len_2)
		t_min^2 = C^2 * max(len_1, len_2)^2
		t_min^2 = C^2 * max(len_1^2, len_2^2)
		*/

#if USE_LAUNCH_NORMAL
		if(t < 0.0f)
			return 0;

		/*
		t = e1 x e2 / ||e1 x e2||

		suppose (t, launch_n) = 0.9f

		(e1 x e2 / ||e1 x e2||, launch_n) = 0.9f
		(e1 x e2, launch_n) / ||e1 x e2|| = 0.9f
		(e1 x e2, launch_n) = 0.9f * ||e1 x e2||


		*/
		const Vec3f tri_normal = crossProduct(e1, e2);// * data[9]; //normalise(crossProduct(e1, e2));
		//assert(tri_normal.isUnitLength());
		if(dot(tri_normal, launch_n) > (0.9f * data[9]))
		{
			// If the normal of this tri and the launch normal are nearly the same
			const float C = 0.0005f;
			const float ray_t_min_sqd = myMax(e1.length2(), e2.length2()) * (C * C);

			if(t*t < ray_t_min_sqd)
				return 0;
		}
		/*else
		{
		// Else the normal of this triangle and the launch triangle are significantly different.
		// So we want to intersect with this triangle, even if the t value is very small.

		//const float C = 0.0000f;//0.00005f;
		//const float ray_t_min_sqd = myMax(e1.length2(), e2.length2()) * (C * C);

		//if(t*t < ray_t_min_sqd)
		//	return 0;
		}*/
#else
		/*const float C = 0.00005f;
		const float ray_t_min_sqd = myMax(e1.length2(), e2.length2()) * (C * C);

		if(t*t < ray_t_min_sqd)
		return 0;*/

		// NEW
		if(t < epsilon)
			return 0;
#endif



		if(t >= ray_t_max)
			return 0;

		dist_out = t;
		u_out = u;
		v_out = v;

		return 1;
#else	
		const Vec3d v0(data[0], data[1], data[2]);
		const Vec3d e1(data[3], data[4], data[5]);
		const Vec3d e2(data[6], data[7], data[8]);

		const Vec3d orig(ray.startPosF().x[0], ray.startPosF().x[1], ray.startPosF().x[2]);
		const Vec3d dir(ray.unitDirF().x[0], ray.unitDirF().x[1], ray.unitDirF().x[2]);
		const Vec3d pvec = crossProduct(dir, e2);

		const double det = dot(e1, pvec);

		const double inv_det = 1.0 / det;

		const Vec3d tvec = orig - v0;

		const double u = dot(tvec, pvec) * inv_det;
		if(u < 0.0 || u > 1.0)
			return 0;

		const Vec3d qvec = crossProduct(tvec, e1);

		const double v = dot(dir, qvec) * inv_det;
		if(v < 0.0 || (u + v) > 1.0)
			return 0;

		const double t = dot(e2, qvec) * inv_det;

		if(t < 0.0)
			return 0;

		const double C = 0.0001;
		const double ray_t_min_sqd = myMax(e1.length2(), e2.length2()) * (C * C);

		if(t*t < ray_t_min_sqd)
			return 0;

		/*const double use_ray_t_min = 0.0001;

		if(t < use_ray_t_min) // 0.0f)
		return 0;*/

		if(t >= ray_t_max)
			return 0;

		dist_out = t;
		u_out = u;
		v_out = v;

		return 1;
#endif
	}

	typedef double riReal;

	inline unsigned int rayIntersectReal(const Ray& ray, riReal ray_t_max, riReal epsilon, riReal& dist_out, float& u_out, float& v_out) const //non zero if hit
	{
		const riReal v0[3] = { data[0], data[1], data[2] };
		const riReal e1[3] = { data[3], data[4], data[5] };
		const riReal e2[3] = { data[6], data[7], data[8] };

		const riReal orig[3] = { ray.startPosF().x[0], ray.startPosF().x[1], ray.startPosF().x[2] };
		const riReal dir[3]  = { ray.unitDirF().x[0],  ray.unitDirF().x[1],  ray.unitDirF().x[2] };
		const riReal pvec[3] = // crossProduct(dir, e2);
		{
			dir[1] * e2[2] - dir[2] * e2[1],
			dir[2] * e2[0] - dir[0] * e2[2],
			dir[0] * e2[1] - dir[1] * e2[0]
		};

		const riReal inv_det = 1 / (e1[0] * pvec[0] + e1[1] * pvec[1] + e1[2] * pvec[2]);

		const riReal tvec[3] =
		{
			orig[0] - v0[0],
			orig[1] - v0[1],
			orig[2] - v0[2]
		};

		const riReal u = (tvec[0] * pvec[0] + tvec[1] * pvec[1] + tvec[2] * pvec[2]) * inv_det;
		if(u < 0 || u > 1)
			return 0;

		const riReal qvec[3] = //crossProduct(tvec, e1);
		{
			tvec[1] * e1[2] - tvec[2] * e1[1],
			tvec[2] * e1[0] - tvec[0] * e1[2],
			tvec[0] * e1[1] - tvec[1] * e1[0]
		};

		const riReal v = (dir[0] * qvec[0] + dir[1] * qvec[1] + dir[2] * qvec[2]) * inv_det;
		if(v < 0 || (u + v) > 1)
			return 0;

		const riReal t = (e2[0] * qvec[0] + e2[1] * qvec[1] + e2[2] * qvec[2]) * inv_det;

		// NEW
		if(t < epsilon)
			return 0;

		if(t >= ray_t_max)
			return 0;

		dist_out = t;
		u_out = (float)u;
		v_out = (float)v;

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
		//float use_min_t,
		float epsilon,
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

		//NEW:
#if USE_LAUNCH_NORMAL
		// tri_n = cross(edge1, edge2)
		const __m128 tri_n_x = _mm_sub_ps(_mm_mul_ps(edge1_y, edge2_z), _mm_mul_ps(edge1_z, edge2_y));
		const __m128 tri_n_y = _mm_sub_ps(_mm_mul_ps(edge1_z, edge2_x), _mm_mul_ps(edge1_x, edge2_z));
		const __m128 tri_n_z = _mm_sub_ps(_mm_mul_ps(edge1_x, edge2_y), _mm_mul_ps(edge1_y, edge2_x));

		const __m128 launch_n_x = _mm_load_ps1(ray->launchNormal().x);
		const __m128 launch_n_y = _mm_load_ps1(ray->launchNormal().x+1);
		const __m128 launch_n_z = _mm_load_ps1(ray->launchNormal().x+2);

		const __m128 tri_n_dot_launch_n = _mm_add_ps(
			_mm_mul_ps(tri_n_x, launch_n_x), 
			_mm_add_ps(
				_mm_mul_ps(tri_n_y, launch_n_y),
				_mm_mul_ps(tri_n_z, launch_n_z)
			));

		const __m128 tri_n_len = loadDataItem(t0data, t1data, t2data, t3data, 9);

		// if(dot(tri_normal, launch_n) > (0.9f * data[9]))
		const float thresh = 0.9f;
		const __m128 same_n = _mm_cmpge_ps(tri_n_dot_launch_n, _mm_mul_ps(_mm_load_ps1(&thresh), tri_n_len));


		//NEW
		const float C = 0.0005f; // 0.00005f;
		const float C_sqd = C * C;

		//// returns mask == 0xFFFFFF ? b : a
		const __m128 Cvec = condMov(zeroVec(), _mm_load_ps1(&C_sqd), same_n);


		const __m128 e1_len_sqd = _mm_add_ps(
			_mm_mul_ps(edge1_x, edge1_x),
			_mm_add_ps(
				_mm_mul_ps(edge1_y, edge1_y),
				_mm_mul_ps(edge1_z, edge1_z)
				)
			);

		const __m128 e2_len_sqd = _mm_add_ps(
			_mm_mul_ps(edge2_x, edge2_x),
			_mm_add_ps(
				_mm_mul_ps(edge2_y, edge2_y),
				_mm_mul_ps(edge2_z, edge2_z)
				)
			);

		const __m128 ray_t_min_sqd = _mm_mul_ps(
			_mm_max_ps(e1_len_sqd, e2_len_sqd),
			Cvec // _mm_load_ps1(&C_sqd)
			);
#endif

		const __m128 hit = _mm_and_ps(
#if USE_LAUNCH_NORMAL
				_mm_and_ps(
					_mm_cmpge_ps(t, zero), // t >= 0.0
					_mm_cmpge_ps(_mm_mul_ps(t, t), ray_t_min_sqd) // t*t >= ray_t_min_sqd
				),
#else
				_mm_cmpge_ps(t, _mm_load_ps1(&epsilon)), // t >= epsilon
#endif
				_mm_and_ps(
					_mm_and_ps(_mm_cmpge_ps(u, zero), _mm_cmple_ps(u, one)), // u >= 0 && u <= 1
					_mm_and_ps(_mm_cmpge_ps(v, zero), _mm_cmple_ps(_mm_add_ps(u, v), one)) // v >= 0 && u+v <= 1
				)
			);

		hit_out->v = hit;
		u_out->v = u;
		v_out->v = v;
		t_out->v = t;
	}	


	/*inline const Vec3f getNormal() const
	{
		return normalise(crossProduct(Vec3f(data[3], data[4], data[5]), Vec3f(data[6], data[7], data[8])));
	}*/

#if USE_LAUNCH_NORMAL
	float data[10];
#else
	float data[9]; // 9 * 4 bytes = 36 bytes.
#endif
};


} //end namespace js


#endif //__MOLLERTRUMBORETRI_H_666_
