/*=====================================================================
BVHImpl.h
---------
File created by ClassTemplate on Thu Oct 30 22:43:04 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __BVHIMPL_H_666_
#define __BVHIMPL_H_666_


#include "../simpleraytracer/ray.h"
#include "jscol_TriTreePerThreadData.h"
#include "../raytracing/hitinfo.h"
#include "BVH.h"

class ThreadContext;


namespace js
{


class BVHImpl
{
public:

	template <class T, class HitInfoType>
	inline static double traceRay(const BVH& bvh, const Ray& ray, double ray_max_t, ThreadContext& thread_context, js::TriTreePerThreadData& context, 
		const Object* object, HitInfoType& hitinfo_out)
	{
		assertSSEAligned(&ray);
		assert(ray.unitDir().isUnitLength());
		assert(ray_max_t >= 0.0);

		//hitinfo_out.sub_elem_index = 0;
		//hitinfo_out.sub_elem_coords.set(0.0, 0.0);

		const __m128 raystartpos = _mm_load_ps(&ray.startPosF().x);
		const __m128 inv_dir = _mm_load_ps(&ray.getRecipRayDirF().x);

		__m128 near_t, far_t;
		bvh.root_aabb->rayAABBTrace(raystartpos, inv_dir, near_t, far_t);
		near_t = _mm_max_ss(near_t, zeroVec());
		
		const float ray_max_t_f = (float)ray_max_t;
		far_t = _mm_min_ss(far_t, _mm_load_ss(&ray_max_t_f));

		if(_mm_comile_ss(near_t, far_t) == 0) // if(!(near_t <= far_t) == if near_t > far_t
			return -1.0;

		context.nodestack[0].node = 0;
		_mm_store_ss(&context.nodestack[0].tmin, near_t);
		_mm_store_ss(&context.nodestack[0].tmax, far_t);

		float closest_dist = std::numeric_limits<float>::infinity();

		int stacktop = 0; // Index of node on top of stack
		while(stacktop >= 0)
		{
			// Pop node off stack
			unsigned int current = context.nodestack[stacktop].node;
			__m128 tmin = _mm_load_ss(&context.nodestack[stacktop].tmin);
			__m128 tmax = _mm_load_ss(&context.nodestack[stacktop].tmax);

			tmax = _mm_min_ss(tmax, _mm_load_ss(&closest_dist));

			stacktop--;

			// While current is indexing a suitable internal node...
			while(1)
			{
				_mm_prefetch((const char*)(bvh.nodes + bvh.nodes[current].getLeftChildIndex()), _MM_HINT_T0);
				_mm_prefetch((const char*)(bvh.nodes + bvh.nodes[current].getRightChildIndex()), _MM_HINT_T0);

				const __m128 a = _mm_load_ps(bvh.nodes[current].box);
				const __m128 b = _mm_load_ps(bvh.nodes[current].box + 4);
				const __m128 c = _mm_load_ps(bvh.nodes[current].box + 8);

				// Test ray against left child
				__m128 left_near_t, left_far_t;
				{
				// a = [rmax.x, rmin.x, lmax.x, lmin.x]
				// b = [rmax.y, rmin.y, lmax.y, lmin.y]
				// c = [rmax.z, rmin.z, lmax.z, lmin.z]
				__m128 
				box_min = _mm_shuffle_ps(a, b,			_MM_SHUFFLE(0, 0, 0, 0)); // box_min = [lmin.y, lmin.y, lmin.x, lmin.x]
				box_min = _mm_shuffle_ps(box_min, c,	_MM_SHUFFLE(0, 0, 2, 0)); // box_min = [lmin.z, lmin.z, lmin.x, lmin.x]
				__m128 
				box_max = _mm_shuffle_ps(a, b,			_MM_SHUFFLE(1, 1, 1, 1)); // box_max = [lmax.y, lmax.y, lmax.x, lmax.x]
				box_max = _mm_shuffle_ps(box_max, c,	_MM_SHUFFLE(1, 1, 2, 0)); // box_max = [lmax.z, lmax.z, lmax.y, lmax.x]

				#ifdef DEBUG
				SSE_ALIGN float temp[4];
				_mm_store_ps(temp, box_min);
				assert(temp[0] == bvh.nodes[current].box[0] && temp[1] == bvh.nodes[current].box[4] && temp[2] == bvh.nodes[current].box[8]);
				_mm_store_ps(temp, box_max);
				assert(temp[0] == bvh.nodes[current].box[1] && temp[1] == bvh.nodes[current].box[5] && temp[2] == bvh.nodes[current].box[9]);
				#endif

				const SSE4Vec l1 = mult4Vec(sub4Vec(box_min, raystartpos), inv_dir); // l1.x = (box_min.x - pos.x) / dir.x [distances along ray to slab minimums]
				const SSE4Vec l2 = mult4Vec(sub4Vec(box_max, raystartpos), inv_dir); // l1.x = (box_max.x - pos.x) / dir.x [distances along ray to slab maximums]

				SSE4Vec lmax = max4Vec(l1, l2);
				SSE4Vec lmin = min4Vec(l1, l2);

				const SSE4Vec lmax0 = rotatelps(lmax);
				const SSE4Vec lmin0 = rotatelps(lmin);
				lmax = minss(lmax, lmax0);
				lmin = maxss(lmin, lmin0);

				const SSE4Vec lmax1 = muxhps(lmax,lmax);
				const SSE4Vec lmin1 = muxhps(lmin,lmin);
				left_far_t = minss(lmax, lmax1);
				left_near_t = maxss(lmin, lmin1);
				}

				// Take the intersection of the current ray interval and the ray/BB interval
				left_near_t = _mm_max_ss(left_near_t, tmin);
				left_far_t = _mm_min_ss(left_far_t, tmax);

				// Test against right child
				__m128 right_near_t, right_far_t;
				{
				__m128 
				box_min = _mm_shuffle_ps(a, b,			_MM_SHUFFLE(2, 2, 2, 2)); // box_min = [rmin.y, rmin.y, rmin.x, rmin.x]
				box_min = _mm_shuffle_ps(box_min, c,	_MM_SHUFFLE(2, 2, 2, 0)); // box_min = [rmin.z, rmin.z, rmin.x, rmin.x]
				__m128 
				box_max = _mm_shuffle_ps(a, b,			_MM_SHUFFLE(3, 3, 3, 3)); // box_max = [rmax.y, rmax.y, rmax.x, rmax.x]
				box_max = _mm_shuffle_ps(box_max, c,	_MM_SHUFFLE(3, 3, 2, 0)); // box_max = [rmax.z, rmax.z, rmax.y, rmax.x]

				#ifdef DEBUG
				SSE_ALIGN float temp[4];
				_mm_store_ps(temp, box_min);
				assert(temp[0] == bvh.nodes[current].box[2] && temp[1] == bvh.nodes[current].box[6] && temp[2] == bvh.nodes[current].box[10]);
				_mm_store_ps(temp, box_max);
				assert(temp[0] == bvh.nodes[current].box[3] && temp[1] == bvh.nodes[current].box[7] && temp[2] == bvh.nodes[current].box[11]);
				#endif

				const SSE4Vec l1 = mult4Vec(sub4Vec(box_min, raystartpos), inv_dir); // l1.x = (box_min.x - pos.x) / dir.x [distances along ray to slab minimums]
				const SSE4Vec l2 = mult4Vec(sub4Vec(box_max, raystartpos), inv_dir); // l1.x = (box_max.x - pos.x) / dir.x [distances along ray to slab maximums]

				SSE4Vec lmax = max4Vec(l1, l2);
				SSE4Vec lmin = min4Vec(l1, l2);

				const SSE4Vec lmax0 = rotatelps(lmax);
				const SSE4Vec lmin0 = rotatelps(lmin);
				lmax = minss(lmax, lmax0);
				lmin = maxss(lmin, lmin0);

				const SSE4Vec lmax1 = muxhps(lmax,lmax);
				const SSE4Vec lmin1 = muxhps(lmin,lmin);
				right_far_t = minss(lmax, lmax1);
				right_near_t = maxss(lmin, lmin1);
				}
					
				// Take the intersection of the current ray interval and the ray/BB interval
				right_near_t = _mm_max_ss(right_near_t, tmin);
				right_far_t = _mm_min_ss(right_far_t, tmax);

				if(_mm_comile_ss(right_near_t, right_far_t) != 0) { // if(ray hits right AABB)
					if(_mm_comile_ss(left_near_t, left_far_t) != 0) { // if(ray hits left AABB)
						if(bvh.nodes[current].isRightLeaf() == 0) { // If right child exists
							if(bvh.nodes[current].isLeftLeaf() == 0) { // If left child exists
								// Push right child onto stack
								stacktop++;
								assert(stacktop < context.nodestack_size);
								context.nodestack[stacktop].node = bvh.nodes[current].getRightChildIndex();
								_mm_store_ss(&context.nodestack[stacktop].tmin, right_near_t);
								_mm_store_ss(&context.nodestack[stacktop].tmax, right_far_t);

								current = bvh.nodes[current].getLeftChildIndex(); tmin = left_near_t; tmax = left_far_t; // next = L
							} else {
								if(T::testAgainstTriangles(bvh.nodes[current].getLeftGeomIndex(), bvh.nodes[current].getLeftNumGeom(), hitinfo_out, bvh.intersect_tris, closest_dist, ray))
									return 1.0f;

								current = bvh.nodes[current].getRightChildIndex(); tmin = right_near_t; tmax = right_far_t; // next = R
							}
						} else { // Else if right child doesn't exist
							if(bvh.nodes[current].isLeftLeaf() == 0) { // If left child exists
								if(T::testAgainstTriangles(bvh.nodes[current].getRightGeomIndex(), bvh.nodes[current].getRightNumGeom(), hitinfo_out, bvh.intersect_tris, closest_dist, ray))
									return 1.0f;
								current = bvh.nodes[current].getLeftChildIndex(); tmin = left_near_t; tmax = left_far_t; // next = L
							} else {
								if(T::testAgainstTriangles(bvh.nodes[current].getLeftGeomIndex(), bvh.nodes[current].getLeftNumGeom(), hitinfo_out, bvh.intersect_tris, closest_dist, ray))
									return 1.0f;
								if(T::testAgainstTriangles(bvh.nodes[current].getRightGeomIndex(), bvh.nodes[current].getRightNumGeom(), hitinfo_out, bvh.intersect_tris, closest_dist, ray))
									return 1.0f;
								break;
							}
						}
					} else { // Else only right AABB is hit
						if(bvh.nodes[current].isRightLeaf() == 0) { // If right child exists
							current = bvh.nodes[current].getRightChildIndex(); tmin = right_near_t; tmax = right_far_t; // next = R
						} else {
							if(T::testAgainstTriangles(bvh.nodes[current].getRightGeomIndex(), bvh.nodes[current].getRightNumGeom(), hitinfo_out, bvh.intersect_tris, closest_dist, ray))
								return 1.0f;
							break;
						}
					}
				} else { // Else ray missed right AABB
					if(_mm_comile_ss(left_near_t, left_far_t) != 0) { // if(ray hits left AABB)
						if(bvh.nodes[current].isLeftLeaf() == 0) { // If left child exists
							current = bvh.nodes[current].getLeftChildIndex(); tmin = left_near_t; tmax = left_far_t; // next = L
						} else {
							if(T::testAgainstTriangles(bvh.nodes[current].getLeftGeomIndex(), bvh.nodes[current].getLeftNumGeom(), hitinfo_out, bvh.intersect_tris, closest_dist, ray))
								return 1.0f;
							break;
						}
					} else { // Else ray missed left AABB (and right AABB)
						break;
					}
				}
			}
		}

		if(closest_dist < std::numeric_limits<float>::infinity())
			return closest_dist;
		else
			return -1.0f; // Missed all tris
	}
};


} //end namespace js


#endif //__BVHIMPL_H_666_
