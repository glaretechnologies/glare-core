/*=====================================================================
tritree.cpp
-----------
File created by ClassTemplate on Fri Nov 05 01:09:27 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "jscol_tritree.h"

#include "jscol_treenode.h"
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
#include <zlib.h>
#include <limits>
#include <algorithm>
#include "../graphics/TriBoxIntersection.h"
#include "../indigo/TestUtils.h"
#include "../utils/timer.h" // TEMP
#include <iostream> // TEMP

#ifdef USE_SSE
#define DO_PREFETCHING 1
#endif


const uint32 TREE_CACHE_MAGIC_NUMBER = 0xE727B363;
const uint32 TREE_CACHE_VERSION = 1;

namespace js
{

static void triTreeDebugPrint(const std::string& s)
{
	conPrint("\t" + s);
}

//#define RECORD_TRACE_STATS 1
//#define USE_LETTERBOX 1



TriTree::TriTree(RayMesh* raymesh_)
:	root_aabb(NULL)
{
//	nodes.setAlignment(8);

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
	raymesh = raymesh_;//NULL;

	assert(sizeof(AABBox) == 32);
	root_aabb = (js::AABBox*)alignedMalloc(sizeof(AABBox), sizeof(AABBox));
	new(root_aabb) AABBox(Vec3f(0,0,0), Vec3f(0,0,0));

	assert(isAlignedTo(root_aabb, sizeof(AABBox)));

	// Insert DEFAULT_EMPTY_LEAF_NODE_INDEX node
	nodes.push_back(TreeNode(
		0, // leaf tri index
		0 // num leaf tris
		));

	// Insert root node
	nodes.push_back(TreeNode());

	assert((uint64)&nodes[0] % 8 == 0);//assert aligned

	numnodesbuilt = 0;

	num_traces = 0;
	num_root_aabb_hits = 0;
	total_num_nodes_touched = 0;
	total_num_leafs_touched = 0;
	total_num_tris_intersected = 0;
	total_num_tris_considered = 0;
	num_empty_space_cutoffs = 0;
}


TriTree::~TriTree()
{
	alignedSSEFree(root_aabb);
	alignedSSEFree(intersect_tris);
	intersect_tris = NULL;
}

void TriTree::printTraceStats() const
{
	printVar(num_traces);
	conPrint("AABB hit fraction: " + toString((double)num_root_aabb_hits / (double)num_traces));
	conPrint("av num nodes touched: " + toString((double)total_num_nodes_touched / (double)num_traces));
	conPrint("av num leaves touched: " + toString((double)total_num_leafs_touched / (double)num_traces));
	conPrint("av num tris considered: " + toString((double)total_num_tris_considered / (double)num_traces));
	conPrint("av num tris tested: " + toString((double)total_num_tris_intersected / (double)num_traces));
}


// Returns dist till hit triangle, neg number if missed.
double TriTree::traceRay(const Ray& ray, double ray_max_t, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const
{
	assertSSEAligned(&ray);
	assert(ray.unitDir().isUnitLength());
	assert(ray_max_t >= 0.0);

#ifdef RECORD_TRACE_STATS
	this->num_traces++;
#endif

	hitinfo_out.sub_elem_index = 0;
	hitinfo_out.sub_elem_coords.set(0.0, 0.0);

	assert(!nodes.empty());
	assert(root_aabb);

#ifdef DO_PREFETCHING
	// Prefetch first node
	// _mm_prefetch((const char *)(&nodes[0]), _MM_HINT_T0);	
#endif

	float aabb_enterdist, aabb_exitdist;
	if(root_aabb->rayAABBTrace(ray.startPosF(), ray.getRecipRayDirF(), aabb_enterdist, aabb_exitdist) == 0)
		return -1.0; // Ray missed aabbox
	assert(aabb_enterdist <= aabb_exitdist);

#ifdef RECORD_TRACE_STATS
	this->num_root_aabb_hits++;
#endif

	const float root_t_min = myMax(0.0f, aabb_enterdist);
	const float root_t_max = myMin((float)ray_max_t, aabb_exitdist);
	if(root_t_min > root_t_max)
		return -1.0; // Ray interval does not intersect AABB

#ifdef USE_LETTERBOX
	context.tri_hash->clear();
#endif

	SSE_ALIGN unsigned int ray_child_indices[8];
	TreeUtils::buildFlatRayChildIndices(ray, ray_child_indices);

	REAL closest_dist = std::numeric_limits<float>::max();

	context.nodestack[0] = StackFrame(ROOT_NODE_INDEX, root_t_min, root_t_max);

	int stacktop = 0;//index of node on top of stack
	
	while(stacktop >= 0)
	{
		//pop node off stack
		unsigned int current = context.nodestack[stacktop].node;
		assert(current < nodes.size());
		float tmin = context.nodestack[stacktop].tmin;
		float tmax = context.nodestack[stacktop].tmax;
		assert(tmax >= tmin);

		stacktop--;

		//tmax = myMin(tmax, closest_dist);
		
		while(nodes[current].getNodeType() != TreeNode::NODE_TYPE_LEAF)
		{
#ifdef DO_PREFETCHING
//			_mm_prefetch((const char *)(&nodes[nodes[current].getPosChildIndex()]), _MM_HINT_T0);	
#endif			

			const unsigned int splitting_axis = nodes[current].getSplittingAxis();
			const SSE4Vec t = mult4Vec(
				sub4Vec(
					loadScalarCopy(&nodes[current].data2.dividing_val), 
					load4Vec(&ray.startPosF().x)
					), 
				load4Vec(&ray.getRecipRayDirF().x)
				);
			const float t_split = t.m128_f32[splitting_axis];
	
			const unsigned int child_nodes[2] = {current + 1, nodes[current].getPosChildIndex()};

			if(t_split > tmax) // Whole interval is on near cell	
			{
				current = child_nodes[ray_child_indices[splitting_axis]];
			}
			else 
			{
				if(tmin > t_split) // whole interval is on far cell.
					current = child_nodes[ray_child_indices[splitting_axis + 4]]; //farnode;
				else 
				{
					// Ray hits plane - double recursion, into both near and far cells.
					const unsigned int nearnode = child_nodes[ray_child_indices[splitting_axis]];
					const unsigned int farnode = child_nodes[ray_child_indices[splitting_axis + 4]];

					// Push far node onto stack to process later
					stacktop++;
					assert(stacktop < context.nodestack_size);
					context.nodestack[stacktop] = StackFrame(farnode, t_split, tmax);

#ifdef DO_PREFETCHING
					// Prefetch pushed child
					_mm_prefetch((const char *)(&nodes[farnode]), _MM_HINT_T0);	
#endif					
					// Process near child next
					current = nearnode;
					tmax = t_split;
				}
			}
			/*if(t_split > tmax) // whole interval is on near cell	
			{
				current = child_nodes[ray_child_indices[splitting_axis]];
			}
			else if(tmin > t_split) // whole interval is on far cell.
			{
				current = child_nodes[ray_child_indices[splitting_axis + 4]];//farnode;
			}
			else // ray hits plane - double recursion, into both near and far cells.
			{
				const unsigned int nearnode = child_nodes[ray_child_indices[splitting_axis]];
				const unsigned int farnode = child_nodes[ray_child_indices[splitting_axis + 4]];
					
				//push far node onto stack to process later
				stacktop++;
				assert(stacktop < context.nodestack_size);
				context.nodestack[stacktop] = StackFrame(farnode, t_split, tmax);
	
#ifdef DO_PREFETCHING
				// Prefetch pushed child
				_mm_prefetch((const char *)(&nodes[farnode]), _MM_HINT_T0);	
#endif					
				//process near child next
				current = nearnode;
				tmax = t_split;
			}*/
		}//end while current node is not a leaf..

		//'current' is a leaf node..
	
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

				float u, v, raydist;
				if(intersect_tris[triangle_index].rayIntersect(
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

		if(closest_dist <= tmax)
		{
			// If intersection point lies before ray exit from this leaf volume, then finished.
			return closest_dist;
		}

	}//end while stacktop >= 0

	//assert(closest_dist == std::numeric_limits<float>::max());
	//return -1.0; // Missed all tris
	//assert(closest_dist == std::numeric_limits<float>::max() || Maths::inRange(closest_dist, aabb_exitdist, aabb_exitdist + (float)NICKMATHS_EPSILON));
	return closest_dist < std::numeric_limits<float>::max() ? closest_dist : -1.0;
		
}




void TriTree::getAllHits(const Ray& ray, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	assertSSEAligned(&ray);

	assert(!nodes.empty());
	assert(root_aabb);	
	
	hitinfos_out.resize(0);

	float aabb_enterdist, aabb_exitdist;
	if(root_aabb->rayAABBTrace(ray.startPosF(), ray.getRecipRayDirF(), aabb_enterdist, aabb_exitdist) == 0)
		return; //missed aabbox

#ifdef USE_LETTERBOX
	context.tri_hash->clear();
#endif

	SSE_ALIGN unsigned int ray_child_indices[8];
	TreeUtils::buildFlatRayChildIndices(ray, ray_child_indices);


	//const float MIN_TMIN = 0.00000001f;//above zero to avoid denorms
	//TEMP:
	//aabb_enterdist = myMax(aabb_enterdist, MIN_TMIN);

	//assert(aabb_enterdist >= 0.0f);
	//assert(aabb_exitdist >= aabb_enterdist);

	const float root_t_min = myMax(0.0f, aabb_enterdist);
	const float root_t_max = aabb_exitdist; //myMin((float)ray_max_t, aabb_exitdist);// * (1.1f + (float)NICKMATHS_EPSILON));
	if(root_t_min > root_t_max)
		return; // Ray interval does not intersect AABB

	REAL closest_dist = std::numeric_limits<float>::max();

	//const REAL initial_closest_dist = aabb_exitdist + 0.01f;
	//REAL closest_dist = initial_closest_dist;//(REAL)2.0e9;//closest hit on a tri so far.

	context.nodestack[0] = StackFrame(ROOT_NODE_INDEX, root_t_min, root_t_max);

	int stacktop = 0;//index of node on top of stack
	
	while(stacktop >= 0)//!nodestack.empty())
	{
		//pop node off stack
		unsigned int current = context.nodestack[stacktop].node;
		float tmin = context.nodestack[stacktop].tmin;
		float tmax = context.nodestack[stacktop].tmax;

		stacktop--;

		while(nodes[current].getNodeType() != TreeNode::NODE_TYPE_LEAF)
		{
#ifdef DO_PREFETCHING
			_mm_prefetch((const char *)(&nodes[nodes[current].getPosChildIndex()]), _MM_HINT_T0);	
#endif			

			const unsigned int splitting_axis = nodes[current].getSplittingAxis();
			const SSE4Vec t = mult4Vec(
				sub4Vec(
					loadScalarCopy(&nodes[current].data2.dividing_val), 
					load4Vec(&ray.startPosF().x)
					), 
				load4Vec(&ray.getRecipRayDirF().x)
				);
			const float t_split = t.m128_f32[splitting_axis];
	

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

			if(t_split > tmax) // whole interval is on near cell	
			{
				current = child_nodes[ray_child_indices[splitting_axis]];
			}
			else if(tmin > t_split) // whole interval is on far cell.
			{
				current = child_nodes[ray_child_indices[splitting_axis + 4]];//farnode;
			}
			else // ray hits plane - double recursion, into both near and far cells.
			{
				const unsigned int nearnode = child_nodes[ray_child_indices[splitting_axis]];
				const unsigned int farnode = child_nodes[ray_child_indices[splitting_axis + 4]];
					
				//push far node onto stack to process later
				stacktop++;
				assert(stacktop < context.nodestack_size);
				context.nodestack[stacktop] = StackFrame(farnode, t_split, tmax);

#ifdef DO_PREFETCHING
				// Prefetch pushed child
				_mm_prefetch((const char *)(&nodes[farnode]), _MM_HINT_T0);	
#endif
					
				//process near child next
				current = nearnode;
				tmax = t_split;
			}
		}//end while current node is not a leaf..

		//'current' is a leaf node..

		//------------------------------------------------------------------------
		//intersect with leaf tris
		//------------------------------------------------------------------------
		unsigned int triindex = nodes[current].getLeafGeomIndex();//positive_child;

		//lookup the number of leaf tris
		const unsigned int num_leaf_tris = nodes[current].getNumLeafGeom();

		for(unsigned int i=0; i<num_leaf_tris; ++i)
		{
			assert(triindex >= 0 && triindex < leafgeom.size());

#ifdef USE_LETTERBOX
			//If this tri has not already been intersected against
			if(!context.tri_hash->containsTriIndex(leafgeom[triindex]))
			{
#endif
				float u, v, raydist;
				if(intersect_tris[leafgeom[triindex]].rayIntersect(ray, closest_dist, raydist, u, v))
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
								Vec2d(u, v),
								raydist
								));
							//hitinfos_out.back().hitpos = ray.startPos();
							//hitinfos_out.back().hitpos.addMult(ray.unitDir(), raydist);
						}
					}

					//NOTE: this looks fully bogus :(
					/*const int last_hit_index = hitinfos_out.size() - 1;
					if(last_hit_index >= 0 && hitinfos_out[last_hit_index].dist == raydist)
					{
						//then we already have this hit
					}
					else
					{
						hitinfos_out.push_back(FullHitInfo());
						hitinfos_out.back().hittri_index = leafgeom[triindex];
						hitinfos_out.back().tri_coords.set(u, v);
						hitinfos_out.back().dist = raydist;
						hitinfos_out.back().hitpos = ray.startpos;
						hitinfos_out.back().hitpos.addMult(ray.unitdir, raydist);
					}*/
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







bool TriTree::doesFiniteRayHit(const ::Ray& ray, double raylength, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object) const
{
	assertSSEAligned(&ray);
	assert(ray.unitDir().isUnitLength());
	assert(raylength > 0.0);

	// Intersect ray with AABBox
	float aabb_enterdist, aabb_exitdist;
	if(root_aabb->rayAABBTrace(ray.startPosF(), ray.getRecipRayDirF(), aabb_enterdist, aabb_exitdist) == 0)
		return false; // missed bounding box

	const float root_t_min = myMax(0.0f, aabb_enterdist);
	const float root_t_max = myMin((float)raylength, aabb_exitdist);
	if(root_t_min > root_t_max)
		return false; // Ray interval does not intersect AABB

	SSE_ALIGN unsigned int ray_child_indices[8];
	TreeUtils::buildFlatRayChildIndices(ray, ray_child_indices);

#ifdef USE_LETTERBOX
	context.tri_hash->clear();
#endif

	context.nodestack[0] = StackFrame(ROOT_NODE_INDEX, root_t_min, root_t_max);
	int stacktop = 0; // index of node on top of stack

	while(stacktop >= 0)
	{
		unsigned int current = context.nodestack[stacktop].node;
		assert(current < nodes.size());
		float tmin = context.nodestack[stacktop].tmin;
		float tmax = context.nodestack[stacktop].tmax;

		stacktop--; //pop current node off stack

		while(nodes[current].getNodeType() != TreeNode::NODE_TYPE_LEAF) //while current node is not a leaf..
		{
#ifdef DO_PREFETCHING
			//_mm_prefetch((const char *)(&nodes[current+1]), _MM_HINT_T0);	
//			_mm_prefetch((const char *)(&nodes[nodes[current].getPosChildIndex()]), _MM_HINT_T0);	
#endif			
			const unsigned int splitting_axis = nodes[current].getSplittingAxis();
			//const REAL t_split = (nodes[current].data2.dividing_val - ray.startPosF()[splitting_axis]) * ray.getRecipRayDirF()[splitting_axis];	
			const SSE4Vec t = mult4Vec(
				sub4Vec(
					loadScalarCopy(&nodes[current].data2.dividing_val), 
					load4Vec(&ray.startPosF().x)
					), 
				load4Vec(&ray.getRecipRayDirF().x)
				);

			const float t_split = t.m128_f32[splitting_axis];
			const unsigned int child_nodes[2] = {current + 1, nodes[current].getPosChildIndex()};

			if(t_split > tmax) // whole interval is on near cell	
			{
				current = child_nodes[ray_child_indices[splitting_axis]];
			}
			else if(tmin > t_split) // whole interval is on far cell.
			{
				current = child_nodes[ray_child_indices[splitting_axis + 4]];//farnode;
			}
			else // ray hits plane - double recursion, into both near and far cells.
			{
				const unsigned int nearnode = child_nodes[ray_child_indices[splitting_axis]];
				const unsigned int farnode = child_nodes[ray_child_indices[splitting_axis + 4]];
					
				//push far node onto stack to process later
				stacktop++;
				assert(stacktop < context.nodestack_size);
				context.nodestack[stacktop] = StackFrame(farnode, t_split, tmax);

#ifdef DO_PREFETCHING
				// Prefetch pushed child
				_mm_prefetch((const char *)(&nodes[farnode]), _MM_HINT_T0);	
#endif
					
				// process near child next
				current = nearnode;
				tmax = t_split;
			}
		} // end while current node is not a leaf..

		// 'current' is a leaf node..

		unsigned int leaf_geom_index = nodes[current].getLeafGeomIndex(); //get index into leaf geometry array
		const unsigned int num_leaf_tris = nodes[current].getNumLeafGeom();

		for(unsigned int i=0; i<num_leaf_tris; ++i)
		{
			assert(leaf_geom_index < leafgeom.size());
			const unsigned int triangle_index = leafgeom[leaf_geom_index]; //get the actual intersection triangle index

#ifdef USE_LETTERBOX
			//If this tri has not already been intersected against
			if(!context.tri_hash->containsTriIndex(triangle_index))
			{
#endif
				//assert(tmax <= raylength);
				float u, v, dummy_hitdist;
				if(intersect_tris[triangle_index].rayIntersect(ray, 
					(float)raylength, // raylength is better than tmax, because we don't mind if we hit a tri outside of this leaf volume, we can still return now.
					dummy_hitdist, u, v))
				{
					if(!object || object->isNonNullAtHit(thread_context, ray, (double)dummy_hitdist, triangle_index, u, v)) // Do visiblity check for null materials etc..
					{
						return true;
					}
				}
				
#ifdef USE_LETTERBOX
				//Add tri index to hash of tris already intersected against
				context.tri_hash->addTriIndex(triangle_index);
			}
#endif
			leaf_geom_index++;
		}
	}
	return false;
}


const Vec3f& TriTree::triVertPos(unsigned int tri_index, unsigned int vert_index_in_tri) const
{
	return raymesh->triVertPos(tri_index, vert_index_in_tri);
}

unsigned int TriTree::numTris() const
{
	return raymesh->getNumTris();
}

/*
void TriTree::AABBoxForTri(unsigned int tri_index, AABBox& aabbox_out)
{
	aabbox_out.min_ = aabbox_out.max_ = triVertPos(tri_index, 0);//(*tris)[i].v0();
	aabbox_out.enlargeToHoldPoint(triVertPos(tri_index, 1));//(*tris)[i].v1());
	aabbox_out.enlargeToHoldPoint(triVertPos(tri_index, 2));//(*tris)[i].v2());

	assert(aabbox_out.invariant());
}*/


bool TriTree::diskCachable()
{
	return true;
}

const js::AABBox& TriTree::getAABBoxWS() const
{
	assert(this->root_aabb);
	return *root_aabb;
}

unsigned int TriTree::calcMaxDepth() const
{
	return MAX_KDTREE_DEPTH - 1;
}

void TriTree::build()
{
	triTreeDebugPrint("Building kd-tree...");

	if(numTris() == 0)
		throw TreeExcep("Error, tried to build tree with zero triangles.");

	try
	{
		//------------------------------------------------------------------------
		//calc root node's aabbox
		//------------------------------------------------------------------------
		triTreeDebugPrint("calculating root AABB.");
		{
		root_aabb->min_ = triVertPos(0, 0);
		root_aabb->max_ = triVertPos(0, 0);
		for(unsigned int i=0; i<numTris(); ++i)
			for(int v=0; v<3; ++v)
			{
				const SSE_ALIGN PaddedVec3 vert = triVertPos(i, v);
				root_aabb->enlargeToHoldAlignedPoint(vert);
			}
		}

		triTreeDebugPrint("AABB: (" + ::toString(root_aabb->min_.x) + ", " + ::toString(root_aabb->min_.y) + ", " + ::toString(root_aabb->min_.z) + "), " + 
							"(" + ::toString(root_aabb->max_.x) + ", " + ::toString(root_aabb->max_.y) + ", " + ::toString(root_aabb->max_.z) + ")"); 
								
		assert(root_aabb->invariant());

		//------------------------------------------------------------------------
		//compute max allowable depth
		//------------------------------------------------------------------------
		//const int numtris = tris->size();
		//max_depth = (int)(2.0f + logBase2((float)numtris) * 1.2f);
	/*TEMP	max_depth = myMin(
			(int)(2.0f + logBase2((float)numTris()) * 2.0f),
			(int)MAX_KDTREE_DEPTH-1
			);*/
		//max_depth = (int)MAX_KDTREE_DEPTH-1;


		const unsigned int max_depth = calcMaxDepth();
		triTreeDebugPrint("max tree depth: " + ::toString(max_depth));

		const int expected_numnodes = (int)((float)numTris() * 1.0f);
		const int nodemem = expected_numnodes * sizeof(js::TreeNode);

		//triTreeDebugPrint("reserving N nodes: " + ::toString(expected_numnodes) + "(" 
		//	+ ::getNiceByteSize(nodemem) + ")");
		//nodes.reserve(expected_numnodes);

		//------------------------------------------------------------------------
		//reserve space for leaf geom array
		//------------------------------------------------------------------------
		leafgeom.reserve(numTris() * 4);

		//const int leafgeommem = leafgeom.capacity() * sizeof(TRI_INDEX);
		//triTreeDebugPrint("leafgeom reserved: " + ::toString(leafgeom.capacity()) + "("
		//	+ ::getNiceByteSize(leafgeommem) + ")");

		{
		OldKDTreeBuilder tree_builder;
		tree_builder.build(*this, *root_aabb, nodes, leafgeom);
		}

		if(!nodes.empty())
		{
			assert(isAlignedTo(&nodes[0], sizeof(js::TreeNode)));
		}

		const unsigned int numnodes = (unsigned int)nodes.size();
		const unsigned int leafgeomsize = (unsigned int)leafgeom.size();

		triTreeDebugPrint("total nodes used: " + ::toString(numnodes) + " (" + 
			::getNiceByteSize(numnodes * sizeof(js::TreeNode)) + ")");

		triTreeDebugPrint("total leafgeom size: " + ::toString(leafgeomsize) + " (" + 
			::getNiceByteSize(leafgeomsize * sizeof(TRI_INDEX)) + ")");

		//------------------------------------------------------------------------
		//alloc intersect tri array
		//------------------------------------------------------------------------
		num_intersect_tris = numTris();
		triTreeDebugPrint("Allocing intersect triangles...");
		if(::atDebugLevel(DEBUG_LEVEL_VERBOSE))
			triTreeDebugPrint("intersect_tris mem usage: " + ::getNiceByteSize(num_intersect_tris * sizeof(INTERSECT_TRI_TYPE)));

		::alignedSSEArrayMalloc(num_intersect_tris, intersect_tris);

		for(unsigned int i=0; i<num_intersect_tris; ++i)
			intersect_tris[i].set(triVertPos(i, 0), triVertPos(i, 1), triVertPos(i, 2));

		const unsigned int total_tree_mem_usage = numnodes * sizeof(js::TreeNode) + leafgeomsize * sizeof(TRI_INDEX) + num_intersect_tris * sizeof(INTERSECT_TRI_TYPE);
		triTreeDebugPrint("Total tree mem usage: " + ::getNiceByteSize(total_tree_mem_usage));

		postBuild();
	}
	catch(std::bad_alloc& )
	{
		throw TreeExcep("Memory allocation failed while building kd-tree");
	}

	//TEMP:
//	printTree(ROOT_NODE_INDEX, 0, std::cout);
}



void TriTree::buildFromStream(std::istream& stream)
{
	if(numTris() == 0)
		throw TreeExcep("Error, tried to build tree with zero triangles.");

	try
	{
		triTreeDebugPrint("Loading kd-tree from cache...");

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
		::alignedSSEArrayMalloc(num_intersect_tris, intersect_tris);

		// Build intersect tris
		for(unsigned int i=0; i<num_intersect_tris; ++i)
			intersect_tris[i].set(triVertPos(i, 0), triVertPos(i, 1), triVertPos(i, 2));

		if(::atDebugLevel(DEBUG_LEVEL_VERBOSE))
			triTreeDebugPrint("intersect_tris mem usage: " + ::getNiceByteSize(num_intersect_tris * sizeof(js::BadouelTri)));

		//------------------------------------------------------------------------
		//calc root node's aabbox
		//------------------------------------------------------------------------
		triTreeDebugPrint("calcing root AABB.");
		{
		root_aabb->min_ = triVertPos(0, 0);
		root_aabb->max_ = triVertPos(0, 0);
		for(unsigned int i=0; i<num_intersect_tris; ++i)
			for(unsigned int v=0; v<3; ++v)
			{
				const SSE_ALIGN PaddedVec3 vert = triVertPos(i, v);
				root_aabb->enlargeToHoldAlignedPoint(vert);
			}
				
		}

		triTreeDebugPrint("AABB: (" + ::toString(root_aabb->min_.x) + ", " + ::toString(root_aabb->min_.y) + ", " + ::toString(root_aabb->min_.z) + "), " + 
							"(" + ::toString(root_aabb->max_.x) + ", " + ::toString(root_aabb->max_.y) + ", " + ::toString(root_aabb->max_.z) + ")"); 
								
		assert(root_aabb->invariant());

		// Compute max allowable depth
		const unsigned int max_depth = calcMaxDepth();

		triTreeDebugPrint("max tree depth: " + ::toString(max_depth));

		//------------------------------------------------------------------------
		// Read nodes
		//------------------------------------------------------------------------
		// Read num nodes
		unsigned int num_nodes;
		stream.read((char*)&num_nodes, sizeof(unsigned int));

		// Read actual node data
		nodes.resize(num_nodes);
		stream.read((char*)&nodes[0], sizeof(js::TreeNode) * num_nodes);

		//------------------------------------------------------------------------
		// Read leafgeom
		//------------------------------------------------------------------------
		// Read num leaf geom
		unsigned int leaf_geom_count;
		stream.read((char*)&leaf_geom_count, sizeof(unsigned int));

		// Read actual leaf geom data
		leafgeom.resize(leaf_geom_count);
		stream.read((char*)&leafgeom[0], sizeof(TRI_INDEX) * leaf_geom_count);


		triTreeDebugPrint("total nodes used: " + ::toString(num_nodes) + " (" + 
			::getNiceByteSize(num_nodes * sizeof(js::TreeNode)) + ")");

		triTreeDebugPrint("total leafgeom size: " + ::toString(leaf_geom_count) + " (" + 
			::getNiceByteSize(leaf_geom_count * sizeof(TRI_INDEX)) + ")");

		const unsigned int total_tree_mem_usage = num_nodes * sizeof(js::TreeNode) + leaf_geom_count * sizeof(TRI_INDEX) + num_intersect_tris * sizeof(INTERSECT_TRI_TYPE);
		triTreeDebugPrint("Total tree mem usage: " + ::getNiceByteSize(total_tree_mem_usage));

		postBuild();
	}
	catch(std::bad_alloc& )
	{
		throw TreeExcep("Memory allocation failed while loading kd-tree from disk");
	}

}

void TriTree::postBuild() const
{
	//triTreeDebugPrint("finished building tree.");

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
















void TriTree::printTree(unsigned int cur, unsigned int depth, std::ostream& out)
{
	if(nodes[cur].getNodeType() == TreeNode::NODE_TYPE_LEAF)//nodes[cur].isLeafNode())
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

void TriTree::debugPrintTree(unsigned int cur, unsigned int depth)
{
	if(nodes[cur].getNodeType() == TreeNode::NODE_TYPE_LEAF)//nodes[cur].isLeafNode())
	{
		std::string lineprefix;
		for(unsigned int i=0; i<depth; ++i)
			lineprefix += " ";

		triTreeDebugPrint(lineprefix + "leaf node");// (num leaf tris: " + nodes[cur].num_leaf_tris + ")");
	}
	else
	{	
		//process neg child
		this->debugPrintTree(cur + 1, depth + 1);

		std::string lineprefix;
		for(unsigned int i=0; i<depth; ++i)
			lineprefix += " ";

		triTreeDebugPrint(lineprefix + "interior node (split axis: " + ::toString(nodes[cur].getSplittingAxis()) + ", split val: "
				+ ::toString(nodes[cur].data2.dividing_val) + ")");

		//process pos child
		this->debugPrintTree(nodes[cur].getPosChildIndex(), depth + 1);
	}
}



void TriTree::getTreeStats(TreeStats& stats_out, unsigned int cur, unsigned int depth) const
{
	if(nodes.empty())
		return;


	if(nodes[cur].getNodeType() == TreeNode::NODE_TYPE_LEAF)//nodes[cur].isLeafNode())
	{
		stats_out.num_leaf_nodes++;
		stats_out.num_leaf_geom_tris += (int)nodes[cur].getNumLeafGeom();
		stats_out.average_leafnode_depth += (double)depth;

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

	if(cur == 0)
	{
		//if this is the root node
		assert(depth == 0);

		stats_out.total_num_nodes = (int)nodes.size();
		stats_out.num_tris = num_intersect_tris;

		stats_out.average_leafnode_depth /= (double)stats_out.num_leaf_nodes;
		stats_out.average_numgeom_per_leafnode = (double)stats_out.num_leaf_geom_tris / (double)stats_out.num_leaf_nodes;
	
		stats_out.max_depth = calcMaxDepth();//max_depth;

		stats_out.total_node_mem = (int)nodes.size() * sizeof(TreeNode);
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



	//returns dist till hit tri, neg number if missed.
double TriTree::traceRayAgainstAllTris(const ::Ray& ray, double t_max, HitInfo& hitinfo_out) const
{
	hitinfo_out.sub_elem_index = 0;
	hitinfo_out.sub_elem_coords.set(0.f, 0.f);

	double closest_dist = std::numeric_limits<double>::max(); //2e9f;//closest hit so far, also upper bound on hit dist

	for(unsigned int i=0; i<num_intersect_tris; ++i)
	{
		float u, v, raydist;
		//raydist is distance until hit if hit occurred
				
		if(intersect_tris[i].rayIntersect(ray, (float)closest_dist, raydist, u, v))
		{			
			assert(raydist < closest_dist);

			closest_dist = raydist;
			hitinfo_out.sub_elem_index = i;
			hitinfo_out.sub_elem_coords.set(u, v);
		}
	}

	if(closest_dist > t_max)
		closest_dist = -1.0;

	return closest_dist;
}

void TriTree::getAllHitsAllTris(const Ray& ray, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	for(unsigned int i=0; i<num_intersect_tris; ++i)
	{
		float u, v, raydist;
		//raydist is distance until hit if hit occurred
				
		if(intersect_tris[i].rayIntersect(ray, std::numeric_limits<float>::max(), raydist, u, v))
		{			
			hitinfos_out.push_back(DistanceHitInfo(
				i,
				Vec2d(u, v),
				raydist
				));
		}
	}
}


js::TriTreePerThreadData* allocPerThreadData()
{
	return new js::TriTreePerThreadData();
}

void TriTree::saveTree(std::ostream& stream)
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
	stream.write((const char*)&nodes[0], sizeof(js::TreeNode)*(unsigned int)nodes.size());

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
unsigned int TriTree::checksum()
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




void TriTree::printStats() const
{
	TreeStats stats;
	getTreeStats(stats);
	stats.print();
}

void TriTree::test()
{

	{
	TreeNode n(
		//TreeNode::NODE_TYPE_INTERIOR, //TreeNode::NODE_TYPE_TWO_CHILDREN,
		2, //axis
		666.0, //split
		43 // right child index
		);

	testAssert(n.getNodeType() == TreeNode::NODE_TYPE_INTERIOR); // TreeNode::NODE_TYPE_TWO_CHILDREN);
	//testAssert(n.getNodeType() != TreeNode::NODE_TYPE_LEAF);
	testAssert(n.getSplittingAxis() == 2);
	testAssert(n.data2.dividing_val == 666.0);
	testAssert(n.getPosChildIndex() == 43);
	}
	{
	TreeNode n(
		67, // leaf geom index
		777 // num leaf geom
		);

	testAssert(n.getNodeType() == TreeNode::NODE_TYPE_LEAF);
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

	total_node_mem = 0;
	leafgeom_indices_mem = 0;
	tri_mem = 0;
}

TreeStats::~TreeStats()
{
}


void TreeStats::print()
{
	conPrint("Nodes");
	conPrint("\tTotal num nodes: " + toString(total_num_nodes));
	conPrint("\tTotal num interior nodes: " + toString(num_interior_nodes));
	conPrint("\tTotal num leaf nodes: " + toString(num_leaf_nodes));

	printVar(num_tris);
	printVar(num_leaf_geom_tris);

	printVar(total_node_mem);
	printVar(leafgeom_indices_mem);
	printVar(tri_mem);

	conPrint("num_empty_space_cutoffs: " + toString(num_empty_space_cutoffs));

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

}




} //end namespace jscol















