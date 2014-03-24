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
#include "MollerTrumboreTri.h"
#include "../indigo/globals.h"
#include "../utils/StringUtils.h"


class ThreadContext;


namespace js
{


class BVHImpl
{
public:

	template <class T, class HitInfoType>
	inline static BVH::Real traceRay(const BVH& bvh, const Ray& ray, BVH::Real ray_max_t, ThreadContext& thread_context, js::TriTreePerThreadData& context, 
		HitInfoType& hitinfo_out)
	{
		assertSSEAligned(&ray);
		//assert(ray.unitDir().isUnitLength());
		assert(ray_max_t >= 0.0);

		const __m128 raystartpos = ray.startPosF().v;
		const __m128 inv_dir = ray.getRecipRayDirF().v;

		//const float use_min_t = ray.startPosF().length() * TREE_EPSILON_FACTOR;
		const float use_min_t = ray.minT();

		/*
		// Test against root AABB
		__m128 near_t, far_t;
		bvh.root_aabb.rayAABBTrace(raystartpos, inv_dir, near_t, far_t);
		near_t = _mm_max_ss(near_t, _mm_load_ss(&use_min_t));
		
		const float ray_max_t_f = (float)ray_max_t;
		far_t = _mm_min_ss(far_t, _mm_load_ss(&ray_max_t_f));

		if(_mm_comile_ss(near_t, far_t) == 0) // if(!(near_t <= far_t) == if near_t > far_t
			return -1.0;
		*/

		context.bvh_stack[0] = 0;

		// This is the distance along the ray to the minimum of the closest hit so far and the maximum length of the ray
		float closest_dist = (float)ray_max_t;

		int stacktop = 0; // Index of node on top of stack
		while(stacktop >= 0)
		{
			// Pop node off stack
			unsigned int current = context.bvh_stack[stacktop];
			__m128 tmin = _mm_load_ss(&use_min_t); //near_t;
			__m128 tmax = _mm_load_ss(&closest_dist);

			stacktop--;

			// While current is indexing a suitable internal node...
			while(1)
			{
				//TEMP:
				//conPrint("current: " + toString(current));

				_mm_prefetch((const char*)(&bvh.nodes[0] + bvh.nodes[current].getLeftChildIndex()), _MM_HINT_T0);
				_mm_prefetch((const char*)(&bvh.nodes[0] + bvh.nodes[current].getRightChildIndex()), _MM_HINT_T0);

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
				box_min = _mm_shuffle_ps(box_min, c,	_MM_SHUFFLE(0, 0, 2, 0)); // box_min = [lmin.z, lmin.z, lmin.y, lmin.x]
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
								// Traverse to closest child, push furthest child onto stack.
								stacktop++;
								assert(stacktop < (int)context.bvh_stack.size());

								float near_tmin;
								float near_tmax;
								float far_tmin;
								float far_tmax;
								_mm_store_ss(&near_tmin, left_near_t);
								_mm_store_ss(&near_tmax, left_far_t);
								_mm_store_ss(&far_tmin, right_near_t);
								_mm_store_ss(&far_tmax, right_far_t);

								uint32 near_idx = bvh.nodes[current].getLeftChildIndex();
								uint32 far_idx = bvh.nodes[current].getRightChildIndex();
								if(near_tmin > far_tmin)
								{
									// If right is actually closest
									mySwap(near_tmin, far_tmin);
									mySwap(near_tmax, far_tmax);
									mySwap(near_idx, far_idx);
								}

								context.bvh_stack[stacktop] = far_idx;
								current = near_idx;
								tmin = _mm_load_ss(&near_tmin);
								tmax = _mm_load_ss(&near_tmax);

								/*const SSE4Vec mask = _mm_cmplt_ss(left_near_t, right_near_t); // mask = left_near_t < right_near_t ? 0xFFFFFFFF : 0x0
								
								tmin = setMasked(left_near_t, right_near_t, mask); // tmin = mask ? left_near_t : right_near_t
								tmax = setMasked(left_far_t, right_far_t, mask);

								const uint32 left_idx = bvh.nodes[current].getLeftChildIndex();
								const uint32 right_idx = bvh.nodes[current].getRightChildIndex();
								const SSE4Vec left = _mm_load_ss((const float*)&left_idx);
								const SSE4Vec right = _mm_load_ss((const float*)&right_idx);
								const SSE4Vec near = setMasked(left, right, mask);
								const SSE4Vec far = setMasked(right, left, mask);

								_mm_store_ss((float*)&current, near); // current = near
								_mm_store_ss((float*)&context.bvh_stack[stacktop], far); // context.bvh_stack[stacktop] = far*/

							} else {
								if(T::testAgainstTriangles(bvh, bvh.nodes[current].getLeftGeomIndex(), bvh.nodes[current].getLeftNumGeom(), ray, use_min_t, hitinfo_out, closest_dist, thread_context))
									return 1.0f;

								current = bvh.nodes[current].getRightChildIndex(); tmin = right_near_t; tmax = right_far_t; // next = R
							}
						} else { // Else if right child doesn't exist
							if(bvh.nodes[current].isLeftLeaf() == 0) { // If left child exists
								if(T::testAgainstTriangles(bvh, bvh.nodes[current].getRightGeomIndex(), bvh.nodes[current].getRightNumGeom(), ray, use_min_t, hitinfo_out, closest_dist, thread_context))
									return 1.0f;
								current = bvh.nodes[current].getLeftChildIndex(); tmin = left_near_t; tmax = left_far_t; // next = L
							} else {
								if(T::testAgainstTriangles(bvh, bvh.nodes[current].getLeftGeomIndex(), bvh.nodes[current].getLeftNumGeom(), ray, use_min_t, hitinfo_out, closest_dist, thread_context))
									return 1.0f;
								if(T::testAgainstTriangles(bvh, bvh.nodes[current].getRightGeomIndex(), bvh.nodes[current].getRightNumGeom(), ray, use_min_t, hitinfo_out, closest_dist, thread_context))
									return 1.0f;
								break;
							}
						}
					} else { // Else only right AABB is hit
						if(bvh.nodes[current].isRightLeaf() == 0) { // If right child exists
							current = bvh.nodes[current].getRightChildIndex(); tmin = right_near_t; tmax = right_far_t; // next = R
						} else {
							if(T::testAgainstTriangles(bvh, bvh.nodes[current].getRightGeomIndex(), bvh.nodes[current].getRightNumGeom(), ray, use_min_t, hitinfo_out, closest_dist, thread_context))
								return 1.0f;
							break;
						}
					}
				} else { // Else ray missed right AABB
					if(_mm_comile_ss(left_near_t, left_far_t) != 0) { // if(ray hits left AABB)
						if(bvh.nodes[current].isLeftLeaf() == 0) { // If left child exists
							current = bvh.nodes[current].getLeftChildIndex(); tmin = left_near_t; tmax = left_far_t; // next = L
						} else {
							if(T::testAgainstTriangles(bvh, bvh.nodes[current].getLeftGeomIndex(), bvh.nodes[current].getLeftNumGeom(), ray, use_min_t, hitinfo_out, closest_dist, thread_context))
								return 1.0f;
							break;
						}
					} else { // Else ray missed left AABB (and right AABB)
						break;
					}
				}
			}
		}

		if(closest_dist < (float)ray_max_t)
			return closest_dist;
		else
			return -1.0f; // Missed all tris
	}
};


} //end namespace js


#endif //__BVHIMPL_H_666_
