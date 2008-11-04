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


typedef union {
	__m128 v;
	float f[4];
	unsigned int i[4];
} Vec4;


/*=====================================================================
MollerTrumboreTri
-----------------

=====================================================================*/
class MollerTrumboreTri
{
public:
	MollerTrumboreTri();

	~MollerTrumboreTri();


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

		const Vec3f orig = ray.startPosF();
		const Vec3f dir = ray.unitDirF();
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

	/*inline static __m128 updateBestData(
		__m128 u,
		__m128 v,
		__m128 tri_indices,
		__m128 best_t,
		unsigned int offset
		)
	{

		// t0_results = [t0_best_v, t0_best_v, t0_best_u, t0_best_u]
		__m128 t_results = _mm_shuffle_ps(u, v, _MM_SHUFFLE_PS(0, 0, 0, 0));

		// t0_results = [t0_best_v, tri_index, t0_best_v, t0_best_u]
		t_results = _mm_shuffle_ps(t_results, tri_indices, _MM_SHUFFLE_PS(0, 0, 2, 0));

		__m128 closer_mask = copyToAll<0>(_mm_and_ps(hit, _mm_cmplt_ps(t, best_t))); // did ray hit tri 0, and is the hit t < best_t?
		best_data = condMov(best_data, closer_mask);
	)*/


	inline static void intersectTris(
		/*const Vec4* orig_x_,
		const Vec4* orig_y_,
		const Vec4* orig_z_,
		const Vec4* dir_x_,
		const Vec4* dir_y_,
		const Vec4* dir_z_,*/
		const Ray* const ray,
		const float* const t0data,
		const float* const t1data,
		const float* const t2data,
		const float* const t3data,
		//Vec4* best_t_,
		//Vec4* best_tri_index_,
		//const Vec4* const tri_indices_,
		//Vec4* best_u_,
		//Vec4* best_v_
		//Vec4* best_data_ // [0, best_tri_index, best_v, best_u]
		Vec4* u_out,
		Vec4* v_out,
		Vec4* t_out,
		Vec4* hit_out
		)
	{
		const __m128 orig_x = _mm_load_ps1(&ray->startPosF().x);
		const __m128 orig_y = _mm_load_ps1(&ray->startPosF().y);
		const __m128 orig_z = _mm_load_ps1(&ray->startPosF().z);
		const __m128 dir_x = _mm_load_ps1(&ray->unitDirF().x);
		const __m128 dir_y = _mm_load_ps1(&ray->unitDirF().y);
		const __m128 dir_z = _mm_load_ps1(&ray->unitDirF().z);

		const __m128 one  = _mm_set_ps1(1.0f);
		const __m128 zero = _mm_set_ps1(0.0f);


		// t0data = [t0_v0x, t0_v0y, t0_v0z, t0_e1x, t0_e1y, t0_e1z, t0_e2x, t0_e2y, t0_e2z]
		// t1data = [t1_v0x, t1_v0y, t1_v0z, t1_e1x, t1_e1y, t1_e1z, t1_e2x, t1_e2y, t1_e2z]

		/*__m128 v0x, v0y, v0z, e1x, e1y, e1z, e2x, e2y, e2z;

		{
		const __m128 a = _mm_load_ss(t0data);		// a = [0, 0, 0, t0_v0x]
		const __m128 b = _mm_load_ss(t1data);		// b = [0, 0, 0, t1_v0x]
		const __m128 c = _mm_load_ss(t2data);		// c = [0, 0, 0, t2_v0x]
		const __m128 d = _mm_load_ss(t3data);		// d = [0, 0, 0, t3_v0x]

		// tmp1 = [0, t2_v0x, 0, t0_v0x]
		// tmp2 = [t3_v0x, 0, t1_v0x, 0]
		const __m128 tmp1 = _mm_shuffle_ps(a, c, _MM_SHUFFLE(0, 0, 0, 0));
		const __m128 tmp2 = _mm_shuffle_ps(b, d, _MM_SHUFFLE(0, 0, 0, 0));

		// v0x = [t3_v0x, t2_v0x, t1_v0x, t0_v0x]
		v0x = _mm_shuffle_ps(tmp1, tmp2, _MM_SHUFFLE(3, 1, 2, 0));
		}*/
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



#if 0
		const __m128 t0_v0 = _mm_load_ps(t0data);
		const __m128 t0_e1 = _mm_load_ps(t0data + 4);
		const __m128 t0_e2 = _mm_load_ps(t0data + 8);

		const __m128 t1_v0 = _mm_load_ps(t1data);
		const __m128 t1_e1 = _mm_load_ps(t1data + 4);
		const __m128 t1_e2 = _mm_load_ps(t1data + 8);

		const __m128 t2_v0 = _mm_load_ps(t2data);
		const __m128 t2_e1 = _mm_load_ps(t2data + 4);
		const __m128 t2_e2 = _mm_load_ps(t2data + 8);

		const __m128 t3_v0 = _mm_load_ps(t3data);
		const __m128 t3_e1 = _mm_load_ps(t3data + 4);
		const __m128 t3_e2 = _mm_load_ps(t3data + 8);

		// tmp1 = [t1_v0.y, t1_v0.x, t0_v0.y, t0_v0.x]
		// tmp2 = [t3_v0.y, t3_v0.x, t2_v0.y, t2_v0.x]
		const __m128 tmp1 = _mm_shuffle_ps(t0_v0, t1_v0, _MM_SHUFFLE(1, 0, 1, 0));
		const __m128 tmp2 = _mm_shuffle_ps(t2_v0, t3_v0, _MM_SHUFFLE(1, 0, 1, 0));

		// v0x =  [t3_v0.x, t2_v0.x, t1_v0.x, t0_v0.x]
		// v0y =  [t3_v0.y, t2_v0.y, t1_v0.y, t0_v0.y]
		const __m128 v0x = _mm_shuffle_ps(tmp1, tmp2, _MM_SHUFFLE(2, 0, 2, 0));
		const __m128 v0y = _mm_shuffle_ps(tmp1, tmp2, _MM_SHUFFLE(3, 1, 3, 1));

		// tmp3 = [t1_v0.w, t1_v0.z, t0_v0.w, t0_v0.z]
		// tmp4 = [t3_v0.w, t3_v0.z, t2_v0.w, t2_v0.z]
		const __m128 tmp3 = _mm_shuffle_ps(t0_v0, t1_v0, _MM_SHUFFLE(3, 2, 3, 2));
		const __m128 tmp4 = _mm_shuffle_ps(t2_v0, t3_v0, _MM_SHUFFLE(3, 2, 3, 2));

		// v0z =  [t3_v0.z, t2_v0.z, t1_v0.z, t0_v0.z]
		const __m128 v0z = _mm_shuffle_ps(tmp3, tmp4, _MM_SHUFFLE(2, 0, 2, 0));


		// tmp5 = [t1_e1.y, t1_e1.x, t0_e1.y, t0_e1.x]
		// tmp6 = [t3_e1.y, t3_e1.x, t2_e1.y, t2_e1.x]
		const __m128 tmp5 = _mm_shuffle_ps(t0_e1, t1_e1, _MM_SHUFFLE(1, 0, 1, 0));
		const __m128 tmp6 = _mm_shuffle_ps(t2_e1, t3_e1, _MM_SHUFFLE(1, 0, 1, 0));

		// e1x =  [t3_e1.x, t2_e1.x, t1_e1.x, t0_e1.x]
		// e1y =  [t3_e1.y, t2_e1.y, t1_e1.y, t0_e1.y]
		const __m128 edge1_x = _mm_shuffle_ps(tmp5, tmp6, _MM_SHUFFLE(2, 0, 2, 0));
		const __m128 edge1_y = _mm_shuffle_ps(tmp5, tmp6, _MM_SHUFFLE(3, 1, 3, 1));

		// tmp7 = [t1_e1.w, t1_e1.z, t0_e1.w, t0_e1.z]
		// tmp8 = [t3_e1.w, t3_e1.z, t2_e1.w, t2_e1.z]
		const __m128 tmp7 = _mm_shuffle_ps(t0_e1, t1_e1, _MM_SHUFFLE(3, 2, 3, 2));
		const __m128 tmp8 = _mm_shuffle_ps(t2_e1, t3_e1, _MM_SHUFFLE(3, 2, 3, 2));

		// e1z =  [t3_e1.z, t2_e1.z, t1_e1.z, t0_e1.z]
		const __m128 edge1_z = _mm_shuffle_ps(tmp7, tmp8, _MM_SHUFFLE(2, 0, 2, 0));



		// tmp9 =  [t1_e2.y, t1_e2.x, t0_e2.y, t0_e2.x]
		// tmp10 = [t3_e2.y, t3_e2.x, t2_e2.y, t2_e2.x]
		const __m128 tmp9 = _mm_shuffle_ps(t0_e2, t1_e2, _MM_SHUFFLE(1, 0, 1, 0));
		const __m128 tmp10 = _mm_shuffle_ps(t2_e2, t3_e2, _MM_SHUFFLE(1, 0, 1, 0));

		// e2x =  [t3_e2.x, t2_e2.x, t1_e2.x, t0_e2.x]
		// e2y =  [t3_e2.y, t2_e2.y, t1_e2.y, t0_e2.y]
		const __m128 edge2_x = _mm_shuffle_ps(tmp9, tmp10, _MM_SHUFFLE(2, 0, 2, 0));
		const __m128 edge2_y = _mm_shuffle_ps(tmp9, tmp10, _MM_SHUFFLE(3, 1, 3, 1));

		// tmp11 = [t1_e2.w, t1_e2.z, t0_e2.w, t0_e2.z]
		// tmp12 = [t3_e2.w, t3_e2.z, t2_e2.w, t2_e2.z]
		const __m128 tmp11 = _mm_shuffle_ps(t0_e2, t1_e2, _MM_SHUFFLE(3, 2, 3, 2));
		const __m128 tmp12 = _mm_shuffle_ps(t2_e2, t3_e2, _MM_SHUFFLE(3, 2, 3, 2));

		// e2z =  [t3_e2.z, t2_e2.z, t1_e2.z, t0_e2.z]
		const __m128 edge2_z = _mm_shuffle_ps(tmp11, tmp12, _MM_SHUFFLE(2, 0, 2, 0));
#endif
		/*const __m128 orig_x = orig_x_->v; // _mm_load_ps(orig_x_);
		const __m128 orig_y = orig_y_->v; // _mm_load_ps(orig_y_);
		const __m128 orig_z = orig_z_->v; // _mm_load_ps(orig_z_);

		const __m128 dir_x = dir_x_->v; // _mm_load_ps(dir_x_);
		const __m128 dir_y = dir_y_->v; // _mm_load_ps(dir_y_);
		const __m128 dir_z = dir_z_->v;  // _mm_load_ps(dir_z_);*/
// 

		/* v1 x v2:
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

		// TODO: det ~= 0 test

		//const __m128 one = _mm_load_ps(one_4vec);

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

		// if(u < 0.0 || u > 1.0) return 0
		// hit = (t >= 0.0 && u >= 0.0 && u <= 1.0 && v >= 0.0 && u+v <= 1.0)
		const __m128 hit = _mm_and_ps(
				_mm_cmpge_ps(t, zero),
				_mm_and_ps(
					_mm_and_ps(_mm_cmpge_ps(u, zero), _mm_cmple_ps(u, one)),
					_mm_and_ps(_mm_cmpge_ps(v, zero), _mm_cmple_ps(_mm_add_ps(u, v), one))
				)
			);

		//__m128 best_t = best_t_->v; // _mm_load_ps(best_t_);


		hit_out->v = hit;
		u_out->v = u;
		v_out->v = v;
		t_out->v = t;






		//const __m128 hit_and_closer = _mm_and_ps(hit, _mm_cmplt_ps(t, best_t));

		// [best_u, best_v, best_triindex, dummy]

		//__m128 best_t = _mm_load_ps(&best_t_);
		//__m128 best_u = best_u_->v; // _mm_load_ps(best_u_);
		//__m128 best_v = best_v_->v; // _mm_load_ps(best_v_);
		//__m128 best_tri_index = best_tri_index_->v; // _mm_load_ps((const float*)best_tri_index_);
		//const __m128 tri_indices = tri_indices_->v; // _mm_load_ps((const float*)tri_indices_);




		/*__m128 best_data = best_data_->v;

		//Vec4* best_data // [0, best_tri_index, best_v, best_u]

		
		{
		// results = [t0_best_v, t0_best_v, t0_best_u, t0_best_u]
		// results = [t0_best_v, tri_index, t0_best_v, t0_best_u]
		__m128 results = _mm_shuffle_ps(u, v, _MM_SHUFFLE(0, 0, 0, 0));
		results = _mm_shuffle_ps(results, tri_indices, _MM_SHUFFLE(0, 0, 2, 0));

		const __m128 closer_mask = copyToAll<0>(_mm_and_ps(hit, _mm_cmplt_ps(t, best_t))); // did ray hit tri 0, and is the hit t < best_t?
		best_data = condMov(best_data, results, closer_mask);
		best_t = condMov(best_t, copyToAll<0>(t), closer_mask);
		}

		{
		// results = [t1_best_v, t1_best_v, t1_best_u, t1_best_u]
		// results = [t1_tri_index, t1_tri_index, t1_best_v, t1_best_u]
		__m128 results = _mm_shuffle_ps(u, v, _MM_SHUFFLE(1, 1, 1, 1));
		results = _mm_shuffle_ps(results, tri_indices, _MM_SHUFFLE(1, 1, 2, 0));

		const __m128 closer_mask = copyToAll<1>(_mm_and_ps(hit, _mm_cmplt_ps(t, best_t))); // did ray hit tri 0, and is the hit t < best_t?
		best_data = condMov(best_data, results, closer_mask);
		best_t = condMov(best_t, copyToAll<1>(t), closer_mask);
		}

		{
		__m128 results = _mm_shuffle_ps(u, v, _MM_SHUFFLE(2, 2, 2, 2));
		results = _mm_shuffle_ps(results, tri_indices, _MM_SHUFFLE(2, 2, 2, 0));

		const __m128 closer_mask = copyToAll<2>(_mm_and_ps(hit, _mm_cmplt_ps(t, best_t))); // did ray hit tri 0, and is the hit t < best_t?
		best_data = condMov(best_data, results, closer_mask);
		best_t = condMov(best_t, copyToAll<2>(t), closer_mask);
		}

		{
		__m128 results = _mm_shuffle_ps(u, v, _MM_SHUFFLE(3, 3, 3, 3));
		results = _mm_shuffle_ps(results, tri_indices, _MM_SHUFFLE(3, 3, 2, 0));

		const __m128 closer_mask = copyToAll<3>(_mm_and_ps(hit, _mm_cmplt_ps(t, best_t))); // did ray hit tri 0, and is the hit t < best_t?
		best_data = condMov(best_data, results, closer_mask);
		best_t = condMov(best_t, copyToAll<3>(t), closer_mask);
		}

		best_t_->v = best_t;
		best_data_->v = best_data;*/


		/*__m128 closer;
		closer = copyToAll<0>(_mm_and_ps(hit, _mm_cmplt_ps(t, best_t))); // did ray hit tri 0, and is the hit t < best_t?
		
		best_t = condMov(best_t, copyToAll<0>(t), closer);
		best_u = condMov(best_u, copyToAll<0>(u), closer);
		best_v = condMov(best_v, copyToAll<0>(v), closer);
		best_tri_index = condMov(best_tri_index, copyToAll<0>(tri_indices), closer);

		closer = copyToAll<1>(_mm_and_ps(hit, _mm_cmplt_ps(t, best_t)));
		best_t = condMov(best_t, copyToAll<1>(t), closer);
		best_u = condMov(best_u, copyToAll<1>(u), closer);
		best_v = condMov(best_v, copyToAll<1>(v), closer);
		best_tri_index = condMov(best_tri_index, copyToAll<1>(tri_indices), closer);

		closer = copyToAll<2>(_mm_and_ps(hit, _mm_cmplt_ps(t, best_t)));
		best_t = condMov(best_t, copyToAll<2>(t), closer);
		best_u = condMov(best_u, copyToAll<2>(u), closer);
		best_v = condMov(best_v, copyToAll<2>(v), closer);
		best_tri_index = condMov(best_tri_index, copyToAll<2>(tri_indices), closer);

		closer = copyToAll<3>(_mm_and_ps(hit, _mm_cmplt_ps(t, best_t)));
		best_t = condMov(best_t, copyToAll<3>(t), closer);
		best_u = condMov(best_u, copyToAll<3>(u), closer);
		best_v = condMov(best_v, copyToAll<3>(v), closer);
		best_tri_index = condMov(best_tri_index, copyToAll<3>(tri_indices), closer);

		// Store back
		//_mm_store_ps(best_t_, best_t);
		//_mm_store_ps(best_u_, best_u);
		//_mm_store_ps(best_v_, best_v);
		//_mm_store_ps((float*)best_tri_index_, best_tri_index);

		best_t_->v = best_t;
		best_u_->v = best_u;
		best_v_->v = best_v;
		best_tri_index_->v = best_tri_index;*/
	}	

	inline const Vec3f getNormal()
	{
		//return Vec3f(data[3], data[7], data[11]);
		return normalise(crossProduct(Vec3f(data[3], data[4], data[5]), Vec3f(data[6], data[7], data[8])));
	}

	//float data[12]; // 12 * 4 bytes = 48 bytes
	float data[9];
};



} //end namespace js


#endif //__MOLLERTRUMBORETRI_H_666_




