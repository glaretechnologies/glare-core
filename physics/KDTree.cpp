/*=====================================================================
tritree.cpp
-----------
File created by ClassTemplate on Fri Nov 05 01:09:27 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "KDTree.h"


#include "KDTreeNode.h"
#include "jscol_TriHash.h"
#include "jscol_TriTreePerThreadData.h"
#include "../utils/stringutils.h"
#include "../indigo/globals.h"
#include "../raytracing/hitinfo.h"
#include "../indigo/FullHitInfo.h"
#include "../indigo/DistanceHitInfo.h"
#include "../maths/SSE.h"
#include "../utils/platformutils.h"
#include "../simpleraytracer/raymesh.h"
#include "TreeUtils.h"
#include "OldKDTreeBuilder.h"
#include "NLogNKDTreeBuilder.h"
#include <zlib.h>
#include <limits>
#include <algorithm>
#include "../graphics/TriBoxIntersection.h"
#include "../indigo/TestUtils.h"
#include "../utils/timer.h"
//#include <iostream>
#include "KDTreeImpl.h"
#include "../indigo/PrintOutput.h"
#include "../indigo/ThreadContext.h"


#define DO_PREFETCHING 1


static const uint32 TREE_CACHE_MAGIC_NUMBER = 0xE727B363;
static const uint32 TREE_CACHE_VERSION = 1;


namespace js
{


//static const float EPSILON_FACTOR = 1.0e-4f;


//#define RECORD_TRACE_STATS 1
//#define USE_LETTERBOX 1


KDTree::KDTree(RayMesh* raymesh_)
{
	assert(SSE::isSSEAligned(this));
	checksum_ = 0;
	calced_checksum = false;
	intersect_tris = NULL;
	num_intersect_tris = 0;
	//max_depth = 0;
	num_cheaper_no_split_leafs = 0;
	num_inseparable_tri_leafs = 0;
	num_maxdepth_leafs = 0;
	num_under_thresh_leafs = 0;
//	build_checksum = 0;
	raymesh = raymesh_;

	// Insert DEFAULT_EMPTY_LEAF_NODE_INDEX node
	nodes.push_back(KDTreeNode(
		0, // leaf tri index
		0 // num leaf tris
		));

	// Insert root node
	nodes.push_back(KDTreeNode());

	assert((uint64)&nodes[0] % 8 == 0);//assert aligned

	numnodesbuilt = 0;

	num_traces = 0;
	num_root_aabb_hits = 0;
	total_num_nodes_touched = 0;
	total_num_leafs_touched = 0;
	total_num_tris_intersected = 0;
	total_num_tris_considered = 0;
	num_empty_space_cutoffs = 0;

	tree_specific_min_t = -666.0f;
}


KDTree::~KDTree()
{
	SSE::alignedSSEFree(intersect_tris);
	intersect_tris = NULL;
}


void KDTree::printTraceStats() const
{
	printVar(num_traces);
	conPrint("AABB hit fraction: " + toString((double)num_root_aabb_hits / (double)num_traces));
	conPrint("av num nodes touched: " + toString((double)total_num_nodes_touched / (double)num_traces));
	conPrint("av num leaves touched: " + toString((double)total_num_leafs_touched / (double)num_traces));
	conPrint("av num tris considered: " + toString((double)total_num_tris_considered / (double)num_traces));
	conPrint("av num tris tested: " + toString((double)total_num_tris_intersected / (double)num_traces));
}


/*class TraceRayFunctions
{
public:
	inline static bool testAgainstTriangles(__m128 tmax, const KDTree& kd, unsigned int leaf_geom_index, unsigned int num_leaf_tris, HitInfo& hitinfo_out, float& closest_dist, const Ray& ray,
		const Object* object, ThreadContext& thread_context)
	{
		for(unsigned int i=0; i<num_leaf_tris; ++i)
		{
			#ifdef RECORD_TRACE_STATS
			total_num_tris_considered++;
			#endif
			assert(leaf_geom_index < kd.leafgeom.size());
			const unsigned int triangle_index = kd.leafgeom[leaf_geom_index];

			#ifdef USE_LETTERBOX
			if(!context.tri_hash->containsTriIndex(triangle_index)) //If this tri has not already been intersected against
			{
			#endif
				#ifdef RECORD_TRACE_STATS
				total_num_tris_intersected++;
				#endif
				// Try prefetching next tri.
				//_mm_prefetch((const char *)((const char *)leaftri + sizeof(js::Triangle)*(triindex + 1)), _MM_HINT_T0);

				float u, v, raydist;
				if(kd.intersect_tris[triangle_index].rayIntersect(
					ray,
					closest_dist, //max t NOTE: because we're using the tri hash, this can't just be the current leaf volume interval.
					raydist, //distance out
					u, v)) //hit uvs out
				{
					assert(raydist < closest_dist);

					if(!object || object->isNonNullAtHit(thread_context, ray, (double)raydist, triangle_index, u, v)) // Do visiblity check for null materials etc..
					{
						closest_dist = raydist;
						hitinfo_out.sub_elem_index = triangle_index;
						hitinfo_out.sub_elem_coords.set(u, v);
					}
				}

			#ifdef USE_LETTERBOX
				// Add tri index to hash of tris already intersected against
				context.tri_hash->addTriIndex(triangle_index);
			}
			#endif
			leaf_geom_index++;
		}

		if(_mm_comile_ss(_mm_load_ss(&closest_dist), tmax) != 0) //closest_dist <= tmax)
		{
			// If intersection point lies before ray exit from this leaf volume, then finished.
			//return closest_dist;
			return true; // Rarly out from KDTreeImpl::traceRay()
		}

		return false; // Don't early out from KDTreeImpl::traceRay()
	}
};*/


// Returns distance till hit triangle, negative number if missed.
KDTree::DistType KDTree::traceRay(const Ray& ray, DistType ray_max_t, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri, HitInfo& hitinfo_out) const
{
	//return KDTreeImpl::traceRay<TraceRayFunctions>(*this, ray, ray_max_t, thread_context, context, object, hitinfo_out);

	assertSSEAligned(&ray);
	assert(ray.unitDir().isUnitLength());
	//assert(ray.minT() >= 0.0f);
	assert(ray_max_t >= 0.0f);

	const DistType epsilon = ray.startPos().length() * TREE_EPSILON_FACTOR;

	js::TriTreePerThreadData& context = thread_context.getTreeContext();

	#ifdef RECORD_TRACE_STATS
	this->num_traces++;
	#endif

	assert(!nodes.empty());

	#ifdef RECORD_TRACE_STATS
	this->num_root_aabb_hits++;
	#endif

	#ifdef USE_LETTERBOX
	context.tri_hash->clear();
	#endif

	assert(this->tree_specific_min_t >= 0.0f);
	const float use_min_t = 0.0f; // myMax(ray.minT(), this->tree_specific_min_t);

	__m128 near_t, far_t;
	root_aabb.rayAABBTrace(ray.startPosF().v, ray.getRecipRayDirF().v, near_t, far_t);
	near_t = _mm_max_ss(near_t, _mm_load_ss(&use_min_t)); // near_t = max(near_t, 0)

	const float ray_max_t_f = (float)ray_max_t;
	far_t = _mm_min_ss(far_t, _mm_load_ss(&ray_max_t_f)); // far_t = min(far_t, ray_max_t)

	if(_mm_comile_ss(near_t, far_t) == 0) // if(!(near_t <= far_t) == if near_t > far_t
		return (Real)-1.0;

	context.nodestack[0].node = ROOT_NODE_INDEX;
	_mm_store_ss(&context.nodestack[0].tmin, near_t);
	_mm_store_ss(&context.nodestack[0].tmax, far_t);

	SSE_ALIGN unsigned int ray_child_indices[8];
	TreeUtils::buildFlatRayChildIndices(ray, ray_child_indices);

	//NOTE: if this is set to ray_max_t (which it should be), we get errors, see for example dof_test.igs
	//REAL closest_dist = ray_max_t_f; // std::numeric_limits<float>::max();
	Real closest_dist = std::numeric_limits<Real>::max();
	DistType closest_dist_d = closest_dist;

	int stacktop = 0; // Index of node on top of stack

	while(stacktop >= 0)
	{
		// Pop node off stack
		unsigned int current = context.nodestack[stacktop].node;
		assert(current < nodes.size());
		__m128 tmin = _mm_load_ss(&context.nodestack[stacktop].tmin);
		__m128 tmax = _mm_load_ss(&context.nodestack[stacktop].tmax);

		stacktop--;

		tmax = _mm_min_ss(tmax, _mm_load_ss(&closest_dist));

		while(nodes[current].getNodeType() != KDTreeNode::NODE_TYPE_LEAF)
		{
			_mm_prefetch((const char*)(&nodes[0] + nodes[current].getPosChildIndex()), _MM_HINT_T0);

			const unsigned int splitting_axis = nodes[current].getSplittingAxis();
			const __m128 t_split =
				_mm_mul_ss(
					_mm_sub_ss(
						_mm_load_ss(&nodes[current].data2.dividing_val),
						_mm_load_ss(ray.startPosF().x + splitting_axis)
					),
					_mm_load_ss(ray.getRecipRayDirF().x + splitting_axis)
				);

			const unsigned int child_nodes[2] = {current + 1, nodes[current].getPosChildIndex()};

			if(_mm_comigt_ss(t_split, tmax) != 0)  // if(t_split > tmax) // Whole interval is on near cell
			{
				current = child_nodes[ray_child_indices[splitting_axis]];
			}
			else
			{
				if(_mm_comigt_ss(tmin, t_split) != 0) // if(tmin > t_split) // whole interval is on far cell.
					current = child_nodes[ray_child_indices[splitting_axis + 4]]; // farnode;
				else
				{
					// Ray hits plane - double recursion, into both near and far cells.
					// Push far node onto stack to process later.
					stacktop++;
					assert(stacktop < context.nodestack_size);
					context.nodestack[stacktop].node = child_nodes[ray_child_indices[splitting_axis + 4]]; // far node
					_mm_store_ss(&context.nodestack[stacktop].tmin, t_split);
					_mm_store_ss(&context.nodestack[stacktop].tmax, tmax);

					#ifdef DO_PREFETCHING
					_mm_prefetch((const char *)(&nodes[child_nodes[ray_child_indices[splitting_axis + 4]]]), _MM_HINT_T0);	// Prefetch pushed child
					#endif

					// Process near child next
					current = child_nodes[ray_child_indices[splitting_axis]]; // near node
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

		unsigned int leaf_geom_index = nodes[current].getLeafGeomIndex();
		const unsigned int num_leaf_tris = nodes[current].getNumLeafGeom();

		for(unsigned int i=0; i<num_leaf_tris; ++i)
		{
			#ifdef RECORD_TRACE_STATS
			total_num_tris_considered++;
			#endif
			assert(leaf_geom_index < leafgeom.size());
			const unsigned int triangle_index = leafgeom[leaf_geom_index];

			#ifdef USE_LETTERBOX
			if(!context.tri_hash->containsTriIndex(triangle_index)) //If this tri has not already been intersected against
			{
			#endif
				#ifdef RECORD_TRACE_STATS
				total_num_tris_intersected++;
				#endif
				// Try prefetching next tri.
				//_mm_prefetch((const char *)((const char *)leaftri + sizeof(js::Triangle)*(triindex + 1)), _MM_HINT_T0);

				float u, v;//, raydist;
				double raydist;
				if(intersect_tris[triangle_index].rayIntersectReal(
					ray,
					//use_min_t, // ray t min
					closest_dist, //max t NOTE: because we're using the tri hash, this can't just be the current leaf volume interval.
					epsilon, // Epsilon NEW
					raydist, //distance out
					u, v)//A != 0
					//&& (ignore_tri != triangle_index)
					) //hit uvs out
				{
					//TEMP assert(raydist < closest_dist);

					if(!object || object->isNonNullAtHit(thread_context, ray, (double)raydist, triangle_index, u, v)) // Do visiblity check for null materials etc..
					{
						closest_dist = raydist;
						closest_dist_d = raydist;
						hitinfo_out.sub_elem_index = triangle_index;
						hitinfo_out.sub_elem_coords.set(u, v);
					}
				}

			#ifdef USE_LETTERBOX
				// Add tri index to hash of tris already intersected against
				context.tri_hash->addTriIndex(triangle_index);
			}
			#endif
			leaf_geom_index++;
		}

		if(_mm_comile_ss(_mm_load_ss(&closest_dist), tmax) != 0) // if(closest_dist <= tmax)
			return closest_dist_d; // If intersection point lies before ray exit from this leaf volume, then finished.
	} // End while stacktop >= 0

	//return closest_dist < std::numeric_limits<Real>::max()/*ray_max_t_f*/ ? closest_dist : (Real)-1.0;
	return closest_dist_d < std::numeric_limits<Real>::max()/*ray_max_t_f*/ ? closest_dist_d : -1;
}


bool KDTree::doesFiniteRayHit(const ::Ray& ray, Real ray_max_t, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri) const
{
	assertSSEAligned(&ray);
	assert(ray.unitDir().isUnitLength());
	assert(ray_max_t >= 0.0);

	const float epsilon = ray.startPos().length() * TREE_EPSILON_FACTOR;

	js::TriTreePerThreadData& context = thread_context.getTreeContext();

	#ifdef RECORD_TRACE_STATS
	this->num_traces++;
	#endif

	assert(!nodes.empty());

	#ifdef RECORD_TRACE_STATS
	this->num_root_aabb_hits++;
	#endif

	#ifdef USE_LETTERBOX
	context.tri_hash->clear();
	#endif

	const float use_min_t = 0.0f; // myMax(ray.minT(), this->tree_specific_min_t);

	__m128 near_t, far_t;
	root_aabb.rayAABBTrace(ray.startPosF().v, ray.getRecipRayDirF().v, near_t, far_t);
	near_t = _mm_max_ss(near_t, _mm_load_ss(&use_min_t)); // near_t = max(near_t, 0)

	const float ray_max_t_f = (float)ray_max_t;
	far_t = _mm_min_ss(far_t, _mm_load_ss(&ray_max_t_f)); // far_t = min(far_t, ray_max_t)

	if(_mm_comile_ss(near_t, far_t) == 0) // if(!(near_t <= far_t) == if near_t > far_t
		return false;

	context.nodestack[0].node = ROOT_NODE_INDEX;
	_mm_store_ss(&context.nodestack[0].tmin, near_t);
	_mm_store_ss(&context.nodestack[0].tmax, far_t);

	SSE_ALIGN unsigned int ray_child_indices[8];
	TreeUtils::buildFlatRayChildIndices(ray, ray_child_indices);

	int stacktop = 0; // index of node on top of stack

	while(stacktop >= 0)
	{
		//pop node off stack
		unsigned int current = context.nodestack[stacktop].node;
		assert(current < nodes.size());
		__m128 tmin = _mm_load_ss(&context.nodestack[stacktop].tmin);
		__m128 tmax = _mm_load_ss(&context.nodestack[stacktop].tmax);

		stacktop--;

		while(nodes[current].getNodeType() != KDTreeNode::NODE_TYPE_LEAF)
		{
			_mm_prefetch((const char*)(&nodes[0] + nodes[current].getPosChildIndex()), _MM_HINT_T0);

			const unsigned int splitting_axis = nodes[current].getSplittingAxis();
			const __m128 t_split =
				_mm_mul_ss(
					_mm_sub_ss(
						_mm_load_ss(&nodes[current].data2.dividing_val),
						_mm_load_ss(ray.startPosF().x + splitting_axis)
					),
					_mm_load_ss(ray.getRecipRayDirF().x + splitting_axis)
				);

			const unsigned int child_nodes[2] = {current + 1, nodes[current].getPosChildIndex()};

			if(_mm_comigt_ss(t_split, tmax) != 0)  // if(t_split > tmax) // Whole interval is on near cell
			{
				current = child_nodes[ray_child_indices[splitting_axis]];
			}
			else
			{
				if(_mm_comigt_ss(tmin, t_split) != 0) // if(tmin > t_split) // whole interval is on far cell.
					current = child_nodes[ray_child_indices[splitting_axis + 4]]; // farnode;
				else
				{
					// Ray hits plane - double recursion, into both near and far cells.
					// Push far node onto stack to process later.
					stacktop++;
					assert(stacktop < context.nodestack_size);
					context.nodestack[stacktop].node = child_nodes[ray_child_indices[splitting_axis + 4]]; // far node
					_mm_store_ss(&context.nodestack[stacktop].tmin, t_split);
					_mm_store_ss(&context.nodestack[stacktop].tmax, tmax);

					#ifdef DO_PREFETCHING
					_mm_prefetch((const char *)(&nodes[child_nodes[ray_child_indices[splitting_axis + 4]]]), _MM_HINT_T0);	// Prefetch pushed child
					#endif

					// Process near child next
					current = child_nodes[ray_child_indices[splitting_axis]]; // near node
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

		unsigned int leaf_geom_index = nodes[current].getLeafGeomIndex();
		const unsigned int num_leaf_tris = nodes[current].getNumLeafGeom();

		for(unsigned int i=0; i<num_leaf_tris; ++i)
		{
			#ifdef RECORD_TRACE_STATS
			total_num_tris_considered++;
			#endif
			assert(leaf_geom_index < leafgeom.size());
			const unsigned int triangle_index = leafgeom[leaf_geom_index];

			#ifdef USE_LETTERBOX
			if(!context.tri_hash->containsTriIndex(triangle_index)) //If this tri has not already been intersected against
			{
			#endif
				#ifdef RECORD_TRACE_STATS
				total_num_tris_intersected++;
				#endif
				// Try prefetching next tri.
				//_mm_prefetch((const char *)((const char *)leaftri + sizeof(js::Triangle)*(triindex + 1)), _MM_HINT_T0);

				float u, v;//, raydist;
				double raydist;
				if(intersect_tris[triangle_index].rayIntersectReal(
					ray,
					//use_min_t, // ray_t_min
					ray_max_t_f, // ray_max_t_f is better than tmax, because we don't mind if we hit a tri outside of this leaf volume, we can still return now.
					epsilon, // Epsilon
					raydist, //distance out
					u, v)
					&& triangle_index != ignore_tri // NEW: ignore intersection if we are supposed to ignore this triangle.
					) //hit uvs out
				{
					if(!object || object->isNonNullAtHit(thread_context, ray, (double)raydist, triangle_index, u, v)) // Do visiblity check for null materials etc..
						return true;
				}

			#ifdef USE_LETTERBOX
				// Add triangle index to hash of triangles already intersected against
				context.tri_hash->addTriIndex(triangle_index);
			}
			#endif
			leaf_geom_index++;
		}

	} // End while stacktop >= 0

	return false;
}


void KDTree::getAllHits(const Ray& ray, ThreadContext& thread_context/*, js::TriTreePerThreadData& context*/, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	assertSSEAligned(&ray);
	assertSSEAligned(object);
	assert(!nodes.empty());

	const float epsilon = ray.startPos().length() * TREE_EPSILON_FACTOR;

	js::TriTreePerThreadData& context = thread_context.getTreeContext();

	hitinfos_out.resize(0);

	#ifdef USE_LETTERBOX
	context.tri_hash->clear();
	#endif

	const float use_min_t = 0.0f; // myMax(ray.minT(), this->tree_specific_min_t);

	__m128 near_t, far_t;
	root_aabb.rayAABBTrace(ray.startPosF().v, ray.getRecipRayDirF().v, near_t, far_t);
	near_t = _mm_max_ss(near_t, _mm_load_ss(&use_min_t)); // near_t = max(near_t, 0)

	if(_mm_comile_ss(near_t, far_t) == 0) // if(!(near_t <= far_t) == if near_t > far_t
		return;

	context.nodestack[0].node = ROOT_NODE_INDEX;
	_mm_store_ss(&context.nodestack[0].tmin, near_t);
	_mm_store_ss(&context.nodestack[0].tmax, far_t);

	SSE_ALIGN unsigned int ray_child_indices[8];
	TreeUtils::buildFlatRayChildIndices(ray, ray_child_indices);


	const double closest_dist = std::numeric_limits<double>::max();

	int stacktop = 0;//index of node on top of stack

	while(stacktop >= 0)//!nodestack.empty())
	{
		//pop node off stack
		unsigned int current = context.nodestack[stacktop].node;
		assert(current < nodes.size());
		__m128 tmin = _mm_load_ss(&context.nodestack[stacktop].tmin);
		__m128 tmax = _mm_load_ss(&context.nodestack[stacktop].tmax);

		stacktop--;

		while(nodes[current].getNodeType() != KDTreeNode::NODE_TYPE_LEAF)
		{
			_mm_prefetch((const char*)(&nodes[0] + nodes[current].getPosChildIndex()), _MM_HINT_T0);
			#ifdef DO_PREFETCHING
			_mm_prefetch((const char *)(&nodes[nodes[current].getPosChildIndex()]), _MM_HINT_T0);
			#endif

			const unsigned int splitting_axis = nodes[current].getSplittingAxis();
			const __m128 t_split =
				_mm_mul_ss(
					_mm_sub_ss(
						_mm_load_ss(&nodes[current].data2.dividing_val),
						_mm_load_ss(ray.startPosF().x + splitting_axis)
					),
					_mm_load_ss(ray.getRecipRayDirF().x + splitting_axis)
				);


			const unsigned int child_nodes[2] = {current + 1, nodes[current].getPosChildIndex()};
			/*unsigned int child_nodes[2];
			if(nodes[current].getNodeType() == TreeNode::NODE_TYPE_LEFT_CHILD_ONLY)
			{
				child_nodes[0] = current + 1;
				child_nodes[1] = DEFAULT_EMPTY_LEAF_NODE_INDEX;
			}
			else if(nodes[current].getNodeType() == TreeNode::NODE_TYPE_RIGHT_CHILD_ONLY)
			{
				child_nodes[0] = DEFAULT_EMPTY_LEAF_NODE_INDEX;
				child_nodes[1] = current + 1;
			}
			else
			{
				child_nodes[0] = current + 1;
				child_nodes[1] = nodes[current].getPosChildIndex();
			}*/

			if(_mm_comigt_ss(t_split, tmax) != 0) // whole interval is on near cell
			{
				current = child_nodes[ray_child_indices[splitting_axis]];
			}
			else
			{
				if(_mm_comigt_ss(tmin, t_split) != 0)  // whole interval is on far cell.
					current = child_nodes[ray_child_indices[splitting_axis + 4]];//farnode;
				else // ray hits plane - double recursion, into both near and far cells.
				{
					stacktop++;
					assert(stacktop < context.nodestack_size);
					context.nodestack[stacktop].node = child_nodes[ray_child_indices[splitting_axis + 4]]; // far node
					_mm_store_ss(&context.nodestack[stacktop].tmin, t_split);
					_mm_store_ss(&context.nodestack[stacktop].tmax, tmax);

					#ifdef DO_PREFETCHING
					_mm_prefetch((const char *)(&nodes[child_nodes[ray_child_indices[splitting_axis + 4]]]), _MM_HINT_T0);	// Prefetch pushed child
					#endif

					// Process near child next
					current = child_nodes[ray_child_indices[splitting_axis]]; // near node
					tmax = t_split;
				}
			}
		}//end while current node is not a leaf..

		//'current' is a leaf node..

		//------------------------------------------------------------------------
		//intersect with leaf tris
		//------------------------------------------------------------------------
		unsigned int triindex = nodes[current].getLeafGeomIndex();//positive_child;
		const unsigned int num_leaf_tris = nodes[current].getNumLeafGeom();

		for(unsigned int i=0; i<num_leaf_tris; ++i)
		{
			assert(triindex >= 0 && triindex < leafgeom.size());

			#ifdef USE_LETTERBOX
			//If this tri has not already been intersected against
			if(!context.tri_hash->containsTriIndex(leafgeom[triindex]))
			{
			#endif
				float u, v;//, raydist;
				double raydist;
				if(intersect_tris[leafgeom[triindex]].rayIntersectReal(
					ray, 
					//use_min_t, // ray_t_min
					closest_dist, 
					epsilon,
					raydist, 
					u, v))
				{
					assert(raydist < closest_dist);

					// Check to see if we have already recorded the hit against this triangle (while traversing another leaf volume)
					// NOTE that this is a slow linear time check, but N should be small, hopefully :)
					bool already_got_hit = false;
					for(unsigned int z=0; z<hitinfos_out.size(); ++z)
						if(hitinfos_out[z].sub_elem_index == leafgeom[triindex])//if tri index is the same
							already_got_hit = true;

					if(!already_got_hit)
					{
						if(!object || object->isNonNullAtHit(thread_context, ray, (double)raydist, leafgeom[triindex], u, v)) // Do visiblity check for null materials etc..
						{
							hitinfos_out.push_back(DistanceHitInfo(
								leafgeom[triindex],
								HitInfo::SubElemCoordsType(u, v),
								raydist
								));
						}
					}
				}
			#ifdef USE_LETTERBOX
				//Add tri index to hash of tris already intersected against
				context.tri_hash->addTriIndex(leafgeom[triindex]);
			}
			#endif
			triindex++;
		}
	}//end while !bundlenodestack.empty()
}


const Vec3f& KDTree::triVertPos(unsigned int tri_index, unsigned int vert_index_in_tri) const
{
	return raymesh->triVertPos(tri_index, vert_index_in_tri);
}


unsigned int KDTree::numTris() const
{
	return raymesh->getNumTris();
}


/*void KDTree::AABBoxForTri(unsigned int tri_index, AABBox& aabbox_out)
{
	aabbox_out.min_ = aabbox_out.max_ = triVertPos(tri_index, 0);//(*tris)[i].v0();
	aabbox_out.enlargeToHoldPoint(triVertPos(tri_index, 1));//(*tris)[i].v1());
	aabbox_out.enlargeToHoldPoint(triVertPos(tri_index, 2));//(*tris)[i].v2());

	assert(aabbox_out.invariant());
}*/


bool KDTree::diskCachable()
{
	const int NUM_TRIS_CACHING_THRESHOLD = 1000;
	return this->raymesh->getNumTris() >= NUM_TRIS_CACHING_THRESHOLD;
	//return true;
}


const js::AABBox& KDTree::getAABBoxWS() const
{
	return root_aabb;
}


unsigned int KDTree::calcMaxDepth() const
{
	return MAX_KDTREE_DEPTH - 1;
}


void KDTree::build(PrintOutput& print_output, bool verbose)
{
	if(verbose) print_output.print("\tBuilding kd-tree...");

	if(numTris() == 0)
		throw TreeExcep("Error, tried to build tree with zero triangles.");

	try
	{
		//------------------------------------------------------------------------
		//calc root node's aabbox
		//------------------------------------------------------------------------
		TreeUtils::buildRootAABB(*raymesh, root_aabb, print_output, verbose);
		assert(root_aabb.invariant());

		const unsigned int max_depth = calcMaxDepth();
		if(verbose) print_output.print("\tmax tree depth: " + ::toString(max_depth));

		const int expected_numnodes = (int)((float)numTris() * 1.0f);
		const int nodemem = expected_numnodes * sizeof(js::KDTreeNode);

		//print_output.print("reserving N nodes: " + ::toString(expected_numnodes) + "("
		//	+ ::getNiceByteSize(nodemem) + ")");
		//nodes.reserve(expected_numnodes);

		//------------------------------------------------------------------------
		//reserve space for leaf geom array
		//------------------------------------------------------------------------
		leafgeom.reserve(numTris() * 4);

		//const int leafgeommem = leafgeom.capacity() * sizeof(TRI_INDEX);
		//print_output.print("leafgeom reserved: " + ::toString(leafgeom.capacity()) + "("
		//	+ ::getNiceByteSize(leafgeommem) + ")");

		{
		//OldKDTreeBuilder tree_builder;
		NLogNKDTreeBuilder tree_builder;
		tree_builder.build(print_output, verbose, *this, root_aabb, nodes, leafgeom);
		}

		if(!nodes.empty())
		{
			assert(SSE::isAlignedTo(&nodes[0], sizeof(js::KDTreeNode)));
		}

		const unsigned int numnodes = (unsigned int)nodes.size();
		const unsigned int leafgeomsize = (unsigned int)leafgeom.size();

		if(verbose) print_output.print("\ttotal nodes used: " + ::toString(numnodes) + " (" + ::getNiceByteSize(numnodes * sizeof(js::KDTreeNode)) + ")");
		if(verbose) print_output.print("\ttotal leafgeom size: " + ::toString(leafgeomsize) + " (" + ::getNiceByteSize(leafgeomsize * sizeof(TRI_INDEX)) + ")");

		//------------------------------------------------------------------------
		//alloc intersect tri array
		//------------------------------------------------------------------------
		num_intersect_tris = numTris();
		if(verbose) print_output.print("\tAllocing intersect triangles...");
		if(verbose) print_output.print("\tintersect_tris mem usage: " + ::getNiceByteSize(num_intersect_tris * sizeof(INTERSECT_TRI_TYPE)));

		SSE::alignedSSEArrayMalloc(num_intersect_tris, intersect_tris);
		if(!intersect_tris)
			throw TreeExcep("Memory allocation failed while building kd-tree");

		for(unsigned int i=0; i<num_intersect_tris; ++i)
			intersect_tris[i].set(triVertPos(i, 0), triVertPos(i, 1), triVertPos(i, 2));

		const unsigned int total_tree_mem_usage = numnodes * sizeof(js::KDTreeNode) + leafgeomsize * sizeof(TRI_INDEX) + num_intersect_tris * sizeof(INTERSECT_TRI_TYPE);
		if(verbose) print_output.print("\tTotal tree mem usage: " + ::getNiceByteSize(total_tree_mem_usage));

		// TEMP:
		//printStats();

		postBuild();
	}
	catch(std::bad_alloc& )
	{
		throw TreeExcep("Memory allocation failed while building kd-tree");
	}
}


void KDTree::buildFromStream(std::istream& stream, PrintOutput& print_output, bool verbose)
{
	if(numTris() == 0)
		throw TreeExcep("Error, tried to build tree with zero triangles.");

	try
	{
		if(verbose) print_output.print("\tLoading kd-tree from cache...");

		// Read magic number
		uint32 magic_number;
		stream.read((char*)&magic_number, sizeof(magic_number));
		if(magic_number != TREE_CACHE_MAGIC_NUMBER)
			throw TreeExcep("Invalid file magic number.");

		// Read file version
		uint32 version;
		stream.read((char*)&version, sizeof(version));
		if(version != TREE_CACHE_VERSION)
			throw TreeExcep("Cached tree file is wrong version.");

		// Read checksum
		uint32 checksum_from_file;
		stream.read((char*)&checksum_from_file, sizeof(checksum_from_file));
		if(checksum_from_file != this->checksum())
			throw TreeExcep("Checksum mismatch");


		//------------------------------------------------------------------------
		//alloc intersect tri array
		//------------------------------------------------------------------------
		num_intersect_tris = numTris();
		SSE::alignedSSEArrayMalloc(num_intersect_tris, intersect_tris);
		if(!intersect_tris)
			throw TreeExcep("Memory allocation failed while building kd-tree");

		// Build intersect tris
		for(unsigned int i=0; i<num_intersect_tris; ++i)
			intersect_tris[i].set(triVertPos(i, 0), triVertPos(i, 1), triVertPos(i, 2));

		if(verbose)
			print_output.print("\tintersect_tris mem usage: " + ::getNiceByteSize(num_intersect_tris * sizeof(INTERSECT_TRI_TYPE)));

		//------------------------------------------------------------------------
		//calc root node's aabbox
		//------------------------------------------------------------------------
		if(verbose) print_output.print("\tcalcing root AABB.");
		{
		root_aabb.min_ = Vec4f(triVertPos(0, 0).x, triVertPos(0, 0).y, triVertPos(0, 0).z, 1.0f);
		root_aabb.max_ = root_aabb.min_;
		for(unsigned int i=0; i<num_intersect_tris; ++i)
			for(unsigned int v=0; v<3; ++v)
			{
				const Vec3f& vertpos = triVertPos(i, v);
				const Vec4f vert(vertpos.x, vertpos.y, vertpos.z, 1.0f);
				root_aabb.enlargeToHoldPoint(vert);
			}

		}

		if(verbose) print_output.print("\tAABB: (" + ::toString(root_aabb.min_.x[0]) + ", " + ::toString(root_aabb.min_.x[1]) + ", " + ::toString(root_aabb.min_.x[2]) + "), " +
							"(" + ::toString(root_aabb.max_.x[0]) + ", " + ::toString(root_aabb.max_.x[1]) + ", " + ::toString(root_aabb.max_.x[2]) + ")");

		assert(root_aabb.invariant());

		// Compute max allowable depth
		const unsigned int max_depth = calcMaxDepth();

		if(verbose) print_output.print("\tmax tree depth: " + ::toString(max_depth));

		//------------------------------------------------------------------------
		// Read nodes
		//------------------------------------------------------------------------
		// Read num nodes
		unsigned int num_nodes;
		stream.read((char*)&num_nodes, sizeof(unsigned int));

		// Read actual node data
		nodes.resize(num_nodes);
		stream.read((char*)&nodes[0], sizeof(js::KDTreeNode) * num_nodes);

		//------------------------------------------------------------------------
		// Read leafgeom
		//------------------------------------------------------------------------
		// Read num leaf geom
		unsigned int leaf_geom_count;
		stream.read((char*)&leaf_geom_count, sizeof(unsigned int));

		// Read actual leaf geom data
		leafgeom.resize(leaf_geom_count);
		stream.read((char*)&leafgeom[0], sizeof(TRI_INDEX) * leaf_geom_count);


		if(verbose) print_output.print("\ttotal nodes used: " + ::toString(num_nodes) + " (" +
			::getNiceByteSize(num_nodes * sizeof(js::KDTreeNode)) + ")");

		if(verbose) print_output.print("\ttotal leafgeom size: " + ::toString(leaf_geom_count) + " (" +
			::getNiceByteSize(leaf_geom_count * sizeof(TRI_INDEX)) + ")");

		const unsigned int total_tree_mem_usage = num_nodes * sizeof(js::KDTreeNode) + leaf_geom_count * sizeof(TRI_INDEX) + num_intersect_tris * sizeof(INTERSECT_TRI_TYPE);
		if(verbose) print_output.print("\tTotal tree mem usage: " + ::getNiceByteSize(total_tree_mem_usage));

		postBuild();
	}
	catch(std::bad_alloc& )
	{
		throw TreeExcep("Memory allocation failed while loading kd-tree from disk");
	}
}


void KDTree::postBuild()
{
	this->tree_specific_min_t = TreeUtils::getTreeSpecificMinT(this->root_aabb);

	//conPrint(this->raymesh->getName() + " tree_specific_min_t: " + toString(tree_specific_min_t));

	//------------------------------------------------------------------------
	//TEMP: check things are aligned
	//------------------------------------------------------------------------
	if((uint64)(&nodes[0]) % 8 != 0)
		conPrint("Warning: nodes are not aligned properly.");

	/*printVar((uint64)&nodes[0]);
	printVar((uint64)(&nodes[0]) % 8);
	printVar((uint64)&intersect_tris[0]);
	printVar((uint64)&intersect_tris[0] % 16);
	printVar((uint64)&leafgeom[0]);
	printVar((uint64)&leafgeom[0] % 4);
	printVar((uint64)&root_aabb[0]);
	printVar((uint64)&root_aabb[0] % 32);*/
}


void KDTree::printTree(unsigned int cur, unsigned int depth, std::ostream& out)
{
	if(nodes[cur].getNodeType() == KDTreeNode::NODE_TYPE_LEAF)//nodes[cur].isLeafNode())
	{
		for(unsigned int i=0; i<depth; ++i)
			out << "  ";
		out << cur << " LEAF (num tris: " << nodes[cur].getNumLeafGeom() << ")\n";
	}
	else
	{

		for(unsigned int i=0; i<depth; ++i)
			out << "  ";
		//out << cur << " INTERIOR (split axis: "  << nodes[cur].getSplittingAxis() << ", split val: " << nodes[cur].data2.dividing_val << ")\n";
		out << cur << "\n";

		//process neg child
		this->printTree(cur + 1, depth + 1, out);

		//process pos child
		this->printTree(nodes[cur].getPosChildIndex(), depth + 1, out);
		out << "\n";
	}
}


void KDTree::debugPrintTree(unsigned int cur, unsigned int depth)
{
	if(nodes[cur].getNodeType() == KDTreeNode::NODE_TYPE_LEAF)//nodes[cur].isLeafNode())
	{
		std::string lineprefix;
		for(unsigned int i=0; i<depth; ++i)
			lineprefix += " ";

		conPrint(lineprefix + "leaf node");// (num leaf tris: " + nodes[cur].num_leaf_tris + ")");
	}
	else
	{
		//process neg child
		this->debugPrintTree(cur + 1, depth + 1);

		std::string lineprefix;
		for(unsigned int i=0; i<depth; ++i)
			lineprefix += " ";

		conPrint(lineprefix + "interior node (split axis: " + ::toString(nodes[cur].getSplittingAxis()) + ", split val: "
				+ ::toString(nodes[cur].data2.dividing_val) + ")");

		//process pos child
		this->debugPrintTree(nodes[cur].getPosChildIndex(), depth + 1);
	}
}


void KDTree::getTreeStats(TreeStats& stats_out, unsigned int cur, unsigned int depth) const
{
	if(nodes.empty())
		return;


	if(nodes[cur].getNodeType() == KDTreeNode::NODE_TYPE_LEAF)//nodes[cur].isLeafNode())
	{
		stats_out.num_leaf_nodes++;
		stats_out.num_leaf_geom_tris += (int)nodes[cur].getNumLeafGeom();
		stats_out.average_leafnode_depth += (double)depth;
		stats_out.max_num_leaf_tris = myMax(stats_out.max_num_leaf_tris, (int)nodes[cur].getNumLeafGeom());

		stats_out.leaf_geom_counts[nodes[cur].getNumLeafGeom()]++;
	}
	else
	{
		stats_out.num_interior_nodes++;

		//------------------------------------------------------------------------
		//recurse to children
		//------------------------------------------------------------------------
		this->getTreeStats(stats_out, cur + 1, depth + 1);
		//if(nodes[cur].getNodeType() == TreeNode::NODE_TYPE_TWO_CHILDREN)

		this->getTreeStats(stats_out, nodes[cur].getPosChildIndex(), depth + 1);
	}

	if(cur == ROOT_NODE_INDEX)
	{
		//if this is the root node
		assert(depth == 0);

		stats_out.total_num_nodes = (int)nodes.size();
		stats_out.num_tris = num_intersect_tris;

		stats_out.average_leafnode_depth /= (double)stats_out.num_leaf_nodes;
		stats_out.average_numgeom_per_leafnode = (double)stats_out.num_leaf_geom_tris / (double)stats_out.num_leaf_nodes;

		stats_out.max_depth = calcMaxDepth();//max_depth;

		stats_out.total_node_mem = (int)nodes.size() * sizeof(KDTreeNode);
		stats_out.leafgeom_indices_mem = stats_out.num_leaf_geom_tris * sizeof(TRI_INDEX);
		stats_out.tri_mem = num_intersect_tris * sizeof(INTERSECT_TRI_TYPE);

		stats_out.num_cheaper_no_split_leafs = this->num_cheaper_no_split_leafs;
		stats_out.num_inseparable_tri_leafs = this->num_inseparable_tri_leafs;
		stats_out.num_maxdepth_leafs = this->num_maxdepth_leafs;
		stats_out.num_under_thresh_leafs = this->num_under_thresh_leafs;
		stats_out.num_empty_space_cutoffs = this->num_empty_space_cutoffs;

		//stats_out.leaf_geom_counts = this->leaf_geom_counts;
	}
}


// Returns dist till hit tri, neg number if missed.
KDTree::Real KDTree::traceRayAgainstAllTris(const ::Ray& ray, Real t_max, HitInfo& hitinfo_out) const
{
	hitinfo_out.sub_elem_index = 0;
	hitinfo_out.sub_elem_coords.set(0.f, 0.f);

	const double epsilon = ray.startPos().length() * TREE_EPSILON_FACTOR;

	Real closest_dist = std::numeric_limits<Real>::max(); //2e9f;//closest hit so far, also upper bound on hit dist

	for(unsigned int i=0; i<num_intersect_tris; ++i)
	{
		float u, v;//, raydist;
		double raydist;
		//raydist is distance until hit if hit occurred

		if(intersect_tris[i].rayIntersectReal(ray, 
			//ray.minT(), // ray_t_min
			(float)closest_dist, epsilon, raydist, u, v))
		{
			assert(raydist < closest_dist);

			closest_dist = raydist;
			hitinfo_out.sub_elem_index = i;
			hitinfo_out.sub_elem_coords.set(u, v);
		}
	}

	if(closest_dist > t_max)
		closest_dist = (Real)-1.0;

	return closest_dist;
}


void KDTree::getAllHitsAllTris(const Ray& ray, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	const double epsilon = ray.startPos().length() * TREE_EPSILON_FACTOR;

	for(unsigned int i=0; i<num_intersect_tris; ++i)
	{
		float u, v;//, raydist;
		double raydist;
		//raydist is distance until hit if hit occurred

		if(intersect_tris[i].rayIntersectReal(ray, 
			//ray.minT(), // ray_t_min
			std::numeric_limits<double>::max(), epsilon, raydist, u, v))
		{
			hitinfos_out.push_back(DistanceHitInfo(
				i,
				HitInfo::SubElemCoordsType(u, v),
				raydist
				));
		}
	}
}


js::TriTreePerThreadData* allocPerThreadData()
{
	return new js::TriTreePerThreadData();
}


void KDTree::saveTree(std::ostream& stream)
{
	// Write file format magic number
	stream.write((const char*)&TREE_CACHE_MAGIC_NUMBER, sizeof(TREE_CACHE_MAGIC_NUMBER));

	// Write file format version
	stream.write((const char*)&TREE_CACHE_VERSION, sizeof(TREE_CACHE_VERSION));

	// Write checksum
	const unsigned int the_checksum = checksum();
	stream.write((const char*)&the_checksum, sizeof(unsigned int));

	//------------------------------------------------------------------------
	//write nodes
	//------------------------------------------------------------------------
	// Write number of nodes
	unsigned int temp = (unsigned int)nodes.size();
	stream.write((const char*)&temp, sizeof(unsigned int));

	// Write actual node data
	stream.write((const char*)&nodes[0], sizeof(js::KDTreeNode)*(unsigned int)nodes.size());

	//------------------------------------------------------------------------
	//write leafgeom
	//------------------------------------------------------------------------
	// Write number of leafgeom
	temp = leafgeom.size();
	stream.write((const char*)&temp, sizeof(unsigned int));

	// Write actual leafgeom data
	stream.write((const char*)&leafgeom[0], sizeof(TRI_INDEX)*leafgeom.size());

}


// Checksum over triangle vertex positions
unsigned int KDTree::checksum()
{
	if(calced_checksum)
		return checksum_;

	std::vector<Vec3f> buf(numTris() * 3);
	for(unsigned int t=0; t<numTris(); ++t)
		for(unsigned int v=0; v<3; ++v)
		{
			buf[t * 3 + v] = triVertPos(t, v);
		}

	checksum_ = crc32(0, (const Bytef*)(&buf[0]), sizeof(Vec3f) * (unsigned int)buf.size());
	calced_checksum = true;
	return checksum_;
}


void KDTree::printStats() const
{
	TreeStats stats;
	getTreeStats(stats);
	stats.print();
}


void KDTree::test()
{
	{
	KDTreeNode n(
		//TreeNode::NODE_TYPE_INTERIOR, //TreeNode::NODE_TYPE_TWO_CHILDREN,
		2, //axis
		666.0, //split
		43 // right child index
		);

	testAssert(n.getNodeType() == KDTreeNode::NODE_TYPE_INTERIOR); // TreeNode::NODE_TYPE_TWO_CHILDREN);
	//testAssert(n.getNodeType() != TreeNode::NODE_TYPE_LEAF);
	testAssert(n.getSplittingAxis() == 2);
	testAssert(n.data2.dividing_val == 666.0);
	testAssert(n.getPosChildIndex() == 43);
	}
	{
	KDTreeNode n(
		67, // leaf geom index
		777 // num leaf geom
		);

	testAssert(n.getNodeType() == KDTreeNode::NODE_TYPE_LEAF);
	testAssert(n.getLeafGeomIndex() == 67);
	testAssert(n.getNumLeafGeom() == 777);
	}
}


TreeStats::TreeStats()
{
	total_num_nodes = 0;//total number of nodes, both interior and leaf
	num_interior_nodes = 0;
	num_leaf_nodes = 0;
	num_tris = 0;//number of triangles stored
	num_leaf_geom_tris = 0;//number of references to tris held in leaf nodes
	average_leafnode_depth = 0;//average depth in tree of leaf nodes, where depth of 0 is root level.
	average_numgeom_per_leafnode = 0;//average num tri refs per leaf node
	max_depth = 0;
	max_num_leaf_tris = 0;

	total_node_mem = 0;
	leafgeom_indices_mem = 0;
	tri_mem = 0;
}


TreeStats::~TreeStats()
{
}


void TreeStats::print()
{
	conPrint("------------------------ KDTree Stats-----------------------------------");
	conPrint("Nodes");
	conPrint("\tTotal num nodes: " + toString(total_num_nodes));
	conPrint("\tTotal num interior nodes: " + toString(num_interior_nodes));
	conPrint("\tTotal num leaf nodes: " + toString(num_leaf_nodes));

	conPrint("Tri References");
	conPrint("\tnum_tris: " + toString(num_tris));
	conPrint("\tnum_leaf_geom_tris: " + toString(num_leaf_geom_tris));
	conPrint("\tmax_num_leaf_tris: " + toString(max_num_leaf_tris));

	//printVar(total_node_mem);
	//printVar(leafgeom_indices_mem);
	//printVar(tri_mem);



	conPrint("\tnum_empty_space_cutoffs: " + toString(num_empty_space_cutoffs));

	conPrint("Build termination reasons:");
	conPrint("\tCheaper to not split: " + toString(num_cheaper_no_split_leafs));
	conPrint("\tInseperable: " + toString(num_inseparable_tri_leafs));
	conPrint("\tReached max depth: " + toString(num_maxdepth_leafs));
	conPrint("\t<=tris per leaf threshold: " + toString(num_under_thresh_leafs));

	conPrint("Depth Statistics:");
	conPrint("\tmax depth limit: " + toString(max_depth));
	conPrint("\tAverage leaf depth: " + toString(average_leafnode_depth));


	conPrint("Number of leaves with N triangles:");
	/*int sum = 0;
	for(int i=0; i<(int)leaf_geom_counts.size(); ++i)
	{
		sum += leaf_geom_counts[i];
		conPrint("\tN=" + toString(i) + ": " + toString(leaf_geom_counts[i]));
	}
	conPrint("\tN>=" + toString((unsigned int)leaf_geom_counts.size()) + ": " + toString(num_leaf_nodes - sum));
	conPrint("\tAverage N: " + toString(average_numgeom_per_leafnode));*/
	for(std::map<int, int>::iterator i=leaf_geom_counts.begin(); i!=leaf_geom_counts.end(); ++i)
	{
		conPrint("\tN=" + toString((*i).first) + ": " + toString((*i).second));
	}
	conPrint("-----------------------------------------------------------------------");
}


} //end namespace jscol
