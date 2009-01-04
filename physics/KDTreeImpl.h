/*=====================================================================
KDTreeImpl.h
------------
File created by ClassTemplate on Sat Nov 01 16:36:52 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __KDTREEIMPL_H_666_
#define __KDTREEIMPL_H_666_

#if 0

#include "jscol_TriTreePerThreadData.h"
#include "KDTree.h"
#include "../simpleraytracer/ray.h"
#include "../raytracing/hitinfo.h"


namespace js
{


/*=====================================================================
KDTreeImpl
----------

=====================================================================*/
class KDTreeImpl
{
public:

	template <class T, class HitInfoType>
	inline static double traceRay(const KDTree& kd, const Ray& ray, double ray_max_t, ThreadContext& thread_context, js::TriTreePerThreadData& context,
		const Object* object, HitInfoType& hitinfo_out)
	{
		assertSSEAligned(&ray);
		assert(ray.unitDir().isUnitLength());
		assert(ray_max_t >= 0.0);

		#ifdef RECORD_TRACE_STATS
		kd.num_traces++;
		#endif

		//hitinfo_out.sub_elem_index = 0;
		//hitinfo_out.sub_elem_coords.set(0.0, 0.0);

		//assert(!nodes.empty());
		//assert(root_aabb);

		#ifdef DO_PREFETCHING
		// Prefetch first node
		// _mm_prefetch((const char *)(&nodes[0]), _MM_HINT_T0);
		#endif

		#ifdef RECORD_TRACE_STATS
		kd.num_root_aabb_hits++;
		#endif

		#ifdef USE_LETTERBOX
		context.tri_hash->clear();
		#endif

		const __m128 raystartpos = _mm_load_ps(&ray.startPosF().x);
		const __m128 inv_dir = _mm_load_ps(&ray.getRecipRayDirF().x);

		__m128 near_t, far_t;
		kd.root_aabb->rayAABBTrace(raystartpos, inv_dir, near_t, far_t);
		near_t = _mm_max_ss(near_t, zeroVec());

		const float ray_max_t_f = (float)ray_max_t;
		far_t = _mm_min_ss(far_t, _mm_load_ss(&ray_max_t_f));

		if(_mm_comile_ss(near_t, far_t) == 0) // if(!(near_t <= far_t) == if near_t > far_t
			return -1.0;

		context.nodestack[0].node = KDTree::ROOT_NODE_INDEX;
		_mm_store_ss(&context.nodestack[0].tmin, near_t);
		_mm_store_ss(&context.nodestack[0].tmax, far_t);

		SSE_ALIGN unsigned int ray_child_indices[8];
		TreeUtils::buildFlatRayChildIndices(ray, ray_child_indices);

		float closest_dist = (float)ray_max_t; // std::numeric_limits<float>::max();

		int stacktop = 0;//index of node on top of stack

		while(stacktop >= 0)
		{
			//pop node off stack
			unsigned int current = context.nodestack[stacktop].node;
			assert(current < kd.nodes.size());
			__m128 tmin = _mm_load_ss(&context.nodestack[stacktop].tmin);
			__m128 tmax = _mm_load_ss(&context.nodestack[stacktop].tmax);

			stacktop--;

			tmax = _mm_min_ss(tmax, _mm_load_ss(&closest_dist));

			while(kd.nodes[current].getNodeType() != KDTreeNode::NODE_TYPE_LEAF)
			{
				//_mm_prefetch((const char*)(nodes + nodes[current].getLeftChildIndex()), _MM_HINT_T0);
				_mm_prefetch((const char*)(&kd.nodes[0] + kd.nodes[current].getPosChildIndex()), _MM_HINT_T0);
				#ifdef DO_PREFETCHING
				//_mm_prefetch((const char *)(&nodes[nodes[current].getPosChildIndex()]), _MM_HINT_T0);
				#endif

				const unsigned int splitting_axis = kd.nodes[current].getSplittingAxis();
				/*const SSE4Vec t = mult4Vec(
					sub4Vec(
						loadScalarCopy(&nodes[current].data2.dividing_val),
						load4Vec(&ray.startPosF().x)
						),
					load4Vec(&ray.getRecipRayDirF().x)
					);
				const float t_split = t.m128_f32[splitting_axis];*/
				const __m128 t_split =
					_mm_mul_ss(
						_mm_sub_ss(
							_mm_load_ss(&kd.nodes[current].data2.dividing_val),
							_mm_load_ss(&ray.startPosF().x + splitting_axis)
						),
						_mm_load_ss(&ray.getRecipRayDirF().x + splitting_axis)
					);

				const unsigned int child_nodes[2] = {current + 1, kd.nodes[current].getPosChildIndex()};

				if(_mm_comigt_ss(t_split, tmax) != 0)  // t_split > tmax) // Whole interval is on near cell
				{
					current = child_nodes[ray_child_indices[splitting_axis]];
				}
				else
				{
					if(_mm_comigt_ss(tmin, t_split) != 0) // tmin > t_split) // whole interval is on far cell.
						current = child_nodes[ray_child_indices[splitting_axis + 4]]; // farnode;
					else
					{
						// Ray hits plane - double recursion, into both near and far cells.
						const unsigned int nearnode = child_nodes[ray_child_indices[splitting_axis]];
						const unsigned int farnode = child_nodes[ray_child_indices[splitting_axis + 4]];

						// Push far node onto stack to process later
						stacktop++;
						assert(stacktop < context.nodestack_size);
						context.nodestack[stacktop].node = farnode;
						_mm_store_ss(&context.nodestack[stacktop].tmin, t_split);
						_mm_store_ss(&context.nodestack[stacktop].tmax, tmax);

						#ifdef DO_PREFETCHING
						// Prefetch pushed child
						_mm_prefetch((const char *)(&nodes[farnode]), _MM_HINT_T0);
						#endif

						// Process near child next
						current = nearnode;
						tmax = t_split;
					}
				}
			} // End while current node is not a leaf..

			// 'current' is a leaf node..

			#ifdef RECORD_TRACE_STATS
			this->total_num_leafs_touched++;
			#endif
			//prefetch all data
			//for(int i=0; i<nodes[current].num_leaf_tris; ++i)
			//	_mm_prefetch((const char *)(leafgeom[nodes[current].positive_child + i]), _MM_HINT_T0);

			//unsigned int leaf_geom_index = kd.nodes[current].getLeafGeomIndex();
			//const unsigned int num_leaf_tris = nodes[current].getNumLeafGeom();

			if(T::testAgainstTriangles( // If early out return
				tmax,
				kd,
				kd.nodes[current].getLeafGeomIndex(),
				kd.nodes[current].getNumLeafGeom(),
				hitinfo_out,
				closest_dist,
				ray,
				object,
				thread_context
				))
			{
				return closest_dist;
			}


		} // End while stacktop >= 0

		//assert(closest_dist == std::numeric_limits<float>::max());
		//return -1.0; // Missed all tris
		//assert(closest_dist == std::numeric_limits<float>::max() || Maths::inRange(closest_dist, aabb_exitdist, aabb_exitdist + (float)NICKMATHS_EPSILON));
		return closest_dist < (float)ray_max_t/*std::numeric_limits<float>::max()*/ ? closest_dist : -1.0;

	}
};


} //end namespace js

#endif

#endif //__KDTREEIMPL_H_666_
