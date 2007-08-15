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
#include "../maths/SSE.h"
#include "../utils/PlatformUtils.h"
#include "../simpleraytracer/raymesh.h"
#include "TreeUtils.h"
#include <zlib.h>
#include <limits>
#include <algorithm>

#ifdef USE_SSE
#define DO_PREFETCHING 1
#endif

static void triTreeDebugPrint(const std::string& s)
{
	conPrint("\t" + s);
}

namespace js
{

//#define RECORD_TRACE_STATS 1

TriTree::TriTree(RayMesh* raymesh_)
:	root_aabb(NULL)
{
//	nodes.setAlignment(8);

	checksum_ = 0;
	calced_checksum = false;
	nodestack_size = 0;
#ifdef SSE_TRAVERSAL
	bundlenodestack = NULL;
	bundlenodestack_size = 0;
#endif
	intersect_tris = NULL;
	num_intersect_tris = 0;
	max_depth = 0;
	num_inseparable_tri_leafs = 0;
	num_maxdepth_leafs = 0;
	num_under_thresh_leafs = 0;
//	build_checksum = 0;
	raymesh = raymesh_;//NULL;

	assert(sizeof(AABBox) == 32);
	root_aabb = (js::AABBox*)alignedMalloc(sizeof(AABBox), sizeof(AABBox));
	new(root_aabb) AABBox(Vec3f(0,0,0), Vec3f(0,0,0));

	assert(isAlignedTo(root_aabb, sizeof(AABBox)));

	nodes.push_back(TreeNode());//root node
	assert((uint64)&nodes[0] % 8 == 0);//assert aligned

	numnodesbuilt = 0;

	//-----------------------------TEMP TEST:
	/*TreeNode n;
	n.setLeafGeomIndex(5000);
	assert(n.getLeafGeomIndex() == 5000);

	n.setLeafNode(true);
	assert(n.isLeafNode());
	n.setLeafNode(false);
	assert(!n.isLeafNode());

	n.setPosChildIndex(560);
	assert(n.getPosChildIndex() == 560);

	n.setSplittingAxis(2);
	assert(n.getSplittingAxis() == 2);*/
	//---------------------------------

	num_traces = 0;
	total_num_nodes_touched = 0;
	total_num_leafs_touched = 0;
	total_num_tris_intersected = 0;
}


TriTree::~TriTree()
{
	alignedSSEFree(root_aabb);

#ifdef SSE_TRAVERSAL
	alignedSSEFree(bundlenodestack);
	bundlenodestack = NULL;
#endif

	alignedSSEFree(intersect_tris);
	intersect_tris = NULL;
}

void TriTree::printTraceStats() const
{
	printVar(num_traces);
	conPrint("av num nodes touched: " + toString((double)total_num_nodes_touched / (double)num_traces));
	conPrint("av num leaves touched: " + toString((double)total_num_leafs_touched / (double)num_traces));
	conPrint("av num tris tested: " + toString((double)total_num_tris_intersected / (double)num_traces));
}

//returns dist till hit tri, neg number if missed.
double TriTree::traceRay(const Ray& ray, double ray_max_t, js::TriTreePerThreadData& context, HitInfo& hitinfo_out) const
{
	assertSSEAligned(&ray);
	assert(ray.unitDir().isUnitLength());
	assert(ray.builtRecipRayDir());
	assert(ray_max_t >= 0);

#ifdef RECORD_TRACE_STATS
	this->num_traces++;
#endif

	
	hitinfo_out.hittriindex = 0;
	hitinfo_out.hittricoords.set(0.0, 0.0);


	assert(nodestack_size > 0);
	assert(!nodes.empty());
	assert(root_aabb);
	if(nodes.empty())
		return -1.0;
/*#ifdef DO_PREFETCHING
	//Prefetch first node
	_mm_prefetch((const char *)(&nodes[0]), _MM_HINT_T0);	
#endif	*/

	float aabb_enterdist, aabb_exitdist;
	if(root_aabb->rayAABBTrace(ray.startPosF(), ray.getRecipRayDirF(), aabb_enterdist, aabb_exitdist) == 0)
	//if(!root_aabb.TBPIntersect(ray.startpos, recip_unitraydir, aabb_enterdist, aabb_exitdist))
		return -1.0; //missed aabbox

	const float MIN_TMIN = 0.0;//0.00000001f;//above zero to avoid denorms
	aabb_enterdist = myMax(aabb_enterdist, MIN_TMIN);

	//if this ray finishes before we even enter the aabb...
	if(ray_max_t <= aabb_enterdist)
		return -1.0;

	aabb_exitdist = myMin(aabb_exitdist, (float)ray_max_t);//reduce ray interval if possible

	context.tri_hash->clear();

	SSE_ALIGN unsigned int ray_child_indices[8];
	TreeUtils::buildFlatRayChildIndices(ray, ray_child_indices);

	assert(aabb_enterdist >= 0.0f);
//TEMP as failing too much	assert(aabb_exitdist >= aabb_enterdist);

	REAL closest_dist = std::numeric_limits<float>::max();//initial_closest_dist;

	context.nodestack[0] = StackFrame(0, aabb_enterdist, aabb_exitdist);

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

		//TEMP:
		//if(closest_dist < tmax)
		//	tmax = closest_dist;

		while(!nodes[current].isLeafNode())//while current node is not a leaf..
		{
#ifdef RECORD_TRACE_STATS
			this->total_num_nodes_touched++;
#endif
			//prefetch child node memory
#ifdef DO_PREFETCHING
			//_mm_prefetch((const char *)(&nodes[current+1]), _MM_HINT_T0);	
			//_mm_prefetch((const char *)(&nodes[current+2]), _MM_HINT_T0);	
			//_mm_prefetch((const char *)(&nodes[current+3]), _MM_HINT_T0);	
			_mm_prefetch((const char *)(&nodes[nodes[current].getPosChildIndex()]), _MM_HINT_T0);	
		//	_mm_prefetch((const char *)(node_zero_addr + nodes[current].getPosChildIndex()), _MM_HINT_T0);	
#endif			
	
			const unsigned int splitting_axis = nodes[current].getSplittingAxis();
			const REAL t_split = (nodes[current].data2.dividing_val - ray.startPosF()[splitting_axis]) * ray.getRecipRayDirF()[splitting_axis];	
			const unsigned int child_nodes[2] = {current + 1, nodes[current].getPosChildIndex()};

			if(t_split > tmax)//or ray segment ends b4 split //whole interval is on near cell	
			{
				current = child_nodes[ray_child_indices[splitting_axis]];
			}
			else if(tmin > t_split)//else if ray seg starts after split //whole interval is on far cell.
			{
				current = child_nodes[ray_child_indices[splitting_axis + 4]];//farnode;
			}
			else //ray hits plane - double recursion, into both near and far cells.
			{
				const unsigned int nearnode = child_nodes[ray_child_indices[splitting_axis]];
				const unsigned int farnode = child_nodes[ray_child_indices[splitting_axis + 4]];
					
				//push far node onto stack to process later
				stacktop++;
				assert(stacktop < nodestack_size);
				context.nodestack[stacktop] = StackFrame(farnode, t_split, tmax);
					
				//process near child next
				current = nearnode;
				tmax = t_split;
			}
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
			total_num_tris_intersected++;
#endif
			assert(leaf_geom_index < leafgeom.size());
			const unsigned int triangle_index = leafgeom[leaf_geom_index];

			if(!context.tri_hash->containsTriIndex(triangle_index)) //If this tri has not already been intersected against
			{
				//try prefetching next tri.
				//_mm_prefetch((const char *)((const char *)leaftri + sizeof(js::Triangle)*(triindex + 1)), _MM_HINT_T0);		

				float u, v, raydist;
				if(intersect_tris[triangle_index].rayIntersect(
					ray, 
					closest_dist, //max t NOTE: because we're using the tri hash, this can't just be the current leaf volume interval.
					raydist, //distance out
					u, v)) //hit uvs out
				{
					assert(raydist < closest_dist);

					closest_dist = raydist;
					hitinfo_out.hittriindex = triangle_index;
					hitinfo_out.hittricoords.set(u, v);
				}

				//Add tri index to hash of tris already intersected against
				context.tri_hash->addTriIndex(triangle_index);
			}
			leaf_geom_index++;
		}

		if(closest_dist <= tmax)
		{
			//If intersection point lies before ray exit from this leaf volume, then finished.
#ifdef RECORD_TRACE_STATS
			printTraceStats();
#endif
			return closest_dist;
		}

	}//end while stacktop >= 0

#ifdef RECORD_TRACE_STATS
	printTraceStats();
#endif

	if(closest_dist < std::numeric_limits<float>::max())
		return closest_dist;
	else
		return -1.0;//missed all tris
}






void TriTree::getAllHits(const Ray& ray, js::TriTreePerThreadData& context, std::vector<FullHitInfo>& hitinfos_out) const
{
	assertSSEAligned(&ray);

	hitinfos_out.resize(0);

	SSE_ALIGN PaddedVec3 raydir = ray.unitDirF();
	//raydir.padding = 1.0f;

	////////////// load recip vector ///////////////
	assert(ray.builtRecipRayDir());
	const SSE_ALIGN PaddedVec3& recip_unitraydir = ray.getRecipRayDirF();

	assert(nodestack_size > 0);
	assert(!nodes.empty());
	assert(root_aabb);
	//TEMP DODGY NO TESTS
	if(nodes.empty())// || nodestack.empty())
		return;

	float aabb_enterdist, aabb_exitdist;
	if(root_aabb->rayAABBTrace(ray.startPosF(), recip_unitraydir, aabb_enterdist, aabb_exitdist) == 0)
		return; //missed aabbox

	context.tri_hash->clear();

	const float MIN_TMIN = 0.00000001f;//above zero to avoid denorms
	//TEMP:
	aabb_enterdist = myMax(aabb_enterdist, MIN_TMIN);

	//assert(aabb_enterdist >= 0.0f);
	//assert(aabb_exitdist >= aabb_enterdist);

	const REAL initial_closest_dist = aabb_exitdist + 0.01f;
	REAL closest_dist = initial_closest_dist;//(REAL)2.0e9;//closest hit on a tri so far.
	context.nodestack[0] = StackFrame(0, aabb_enterdist, aabb_exitdist);

	int stacktop = 0;//index of node on top of stack
	
	while(stacktop >= 0)//!nodestack.empty())
	{
		//pop node off stack
		unsigned int current = context.nodestack[stacktop].node;
		float tmin = context.nodestack[stacktop].tmin;
		float tmax = context.nodestack[stacktop].tmax;

		stacktop--;

		while(!nodes[current].isLeafNode())
		{
			//------------------------------------------------------------------------
			//prefetch child node memory
			//------------------------------------------------------------------------
#ifdef DO_PREFETCHING
			_mm_prefetch((const char *)(&nodes[nodes[current].getPosChildIndex()]), _MM_HINT_T0);	
#endif			
			//while current node is not a leaf..
	
			const unsigned int splitting_axis = nodes[current].getSplittingAxis();

			//signed dist to split plane along ray direction from ray origin
			const REAL axis_sgnd_dist = nodes[current].data2.dividing_val - ray.startPosF()[splitting_axis];
			const REAL sgnd_dist_to_split = axis_sgnd_dist * recip_unitraydir[splitting_axis];			

			//nearnode is the halfspace that the ray origin is in
			unsigned int nearnode;
			unsigned int farnode;
			if(axis_sgnd_dist > (REAL)0.0)
			{
				nearnode = current + 1;//= neg child index
				farnode = nodes[current].getPosChildIndex();
			}
			else
			{
				nearnode = nodes[current].getPosChildIndex();
				farnode = current + 1;//= neg child index
			}

//TEMP WAS FAILING TOO MUCH			assert(tmin >= 0.0f);

			if(sgnd_dist_to_split < (REAL)0.0 || //if heading away from split
				sgnd_dist_to_split > tmax)//or ray segment ends b4 split
			{
				//whole interval is on near cell	
				
				//process near child next
				current = nearnode;
			}
			else
			{
				if(tmin > sgnd_dist_to_split)//else if ray seg starts after split
				{
					//whole interval is on far cell.
					current = farnode;
				}
				else
				{
					//ray hits plane - double recursion, into both near and far cells.
					
					//push far node onto stack to process later
					stacktop++;
					assert(stacktop < nodestack_size);
					context.nodestack[stacktop] = StackFrame(farnode, sgnd_dist_to_split, tmax);
					
					//process near child next
					current = nearnode;			

					tmax = sgnd_dist_to_split;
				}
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

			//If this tri has not already been intersected against
			if(!context.tri_hash->containsTriIndex(leafgeom[triindex]))
			{
				float u, v, raydist;
				if(intersect_tris[leafgeom[triindex]].rayIntersect(ray, closest_dist, raydist, u, v))
				{
					assert(raydist < closest_dist);

					// Check to see if we have already recorded the hit against this triangle (while traversing another leaf volume)
					// NOTE that this is a slow linear time check, but N should be small, hopefully :)
					bool already_got_hit = false;
					for(unsigned int z=0; z<hitinfos_out.size(); ++z)
						if(hitinfos_out[z].hittri_index == leafgeom[triindex])//if tri index is the same
							already_got_hit = true;

					if(!already_got_hit)
					{
						hitinfos_out.push_back(FullHitInfo());
						hitinfos_out.back().hittri_index = leafgeom[triindex];
						hitinfos_out.back().tri_coords.set(u, v);
						hitinfos_out.back().dist = raydist;
						hitinfos_out.back().hitpos = ray.startPos();
						hitinfos_out.back().hitpos.addMult(ray.unitDir(), raydist);
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

				//Add tri index to hash of tris already intersected against
				context.tri_hash->addTriIndex(leafgeom[triindex]);
			}
			triindex++;
		}
	}//end while !bundlenodestack.empty()
}







bool TriTree::doesFiniteRayHit(const ::Ray& ray, double raylength, js::TriTreePerThreadData& context) const
{
	assertSSEAligned(&ray);
	assert(ray.unitDir().isUnitLength());
	assert(ray.builtRecipRayDir());
	assert(raylength > 0.0);

	if(nodes.empty())
		return false;

	// Intersect ray with AABBox
	float aabb_enterdist, aabb_exitdist;
	if(!root_aabb->rayAABBTrace(ray.startPosF(), ray.getRecipRayDirF(), aabb_enterdist, aabb_exitdist))
		return false; //missed bounding box

	if(aabb_enterdist > raylength) //if finite ray terminates before entry into AABB
		return false;
	aabb_exitdist = myMin(aabb_exitdist, (float)raylength);

	SSE_ALIGN unsigned int ray_child_indices[8];
	TreeUtils::buildFlatRayChildIndices(ray, ray_child_indices);

	context.tri_hash->clear();

	context.nodestack[0] = StackFrame(0, aabb_enterdist, aabb_exitdist);
	int stacktop = 0;//index of node on top of stack

	while(stacktop >= 0)
	{
		unsigned int current = context.nodestack[stacktop].node;
		assert(current < nodes.size());
		float tmin = context.nodestack[stacktop].tmin;
		float tmax = context.nodestack[stacktop].tmax;

		stacktop--; //pop current node off stack

		while(!nodes[current].isLeafNode()) //while current node is not a leaf..
		{
#ifdef DO_PREFETCHING
			_mm_prefetch((const char *)(&nodes[current+1]), _MM_HINT_T0);	
			_mm_prefetch((const char *)(&nodes[nodes[current].getPosChildIndex()]), _MM_HINT_T0);	
#endif			
			const unsigned int splitting_axis = nodes[current].getSplittingAxis();
			const REAL t_split = (nodes[current].data2.dividing_val - ray.startPosF()[splitting_axis]) * ray.getRecipRayDirF()[splitting_axis];	
			const unsigned int child_nodes[2] = {current + 1, nodes[current].getPosChildIndex()};

			if(t_split > tmax)//or ray segment ends b4 split //whole interval is on near cell	
			{
				current = child_nodes[ray_child_indices[splitting_axis]];
			}
			else if(tmin > t_split)//else if ray seg starts after split //whole interval is on far cell.
			{
				current = child_nodes[ray_child_indices[splitting_axis + 4]];//farnode;
			}
			else //ray hits plane - double recursion, into both near and far cells.
			{
				const unsigned int nearnode = child_nodes[ray_child_indices[splitting_axis]];
				const unsigned int farnode = child_nodes[ray_child_indices[splitting_axis + 4]];
					
				//push far node onto stack to process later
				stacktop++;
				assert(stacktop < nodestack_size);
				context.nodestack[stacktop] = StackFrame(farnode, t_split, tmax);
					
				//process near child next
				current = nearnode;
				tmax = t_split;
			}
		}//end while current node is not a leaf..

		//'current' is a leaf node..

		unsigned int leaf_geom_index = nodes[current].getLeafGeomIndex(); //get index into leaf geometry array
		const unsigned int num_leaf_tris = nodes[current].getNumLeafGeom();

		for(unsigned int i=0; i<num_leaf_tris; ++i)
		{
			assert(leaf_geom_index < leafgeom.size());
			const unsigned int triangle_index = leafgeom[leaf_geom_index]; //get the actual intersection triangle index

			//If this tri has not already been intersected against
			if(!context.tri_hash->containsTriIndex(triangle_index))
			{
				//assert(tmax <= raylength);
				float u, v, dummy_hitdist;
				if(intersect_tris[triangle_index].rayIntersect(ray, 
					(float)raylength,//raylength is better than tmax, because we don't mind if we hit a tri outside of this leaf volume, we can still return now.
					dummy_hitdist, u, v))
				{
					return true;
				}
				
				//Add tri index to hash of tris already intersected against
				context.tri_hash->addTriIndex(triangle_index);
			}

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


void TriTree::build(/*RayMesh* raymesh_*/)
{
	//raymesh = raymesh_;

	triTreeDebugPrint("TriTree::build()");
	//triTreeDebugPrint(::toString(tris->size()) + " tris.");

	/*if(tris->empty())
	{
		assert(0);
		return;
	}*/

	//build_checksum = checksum();//save checksum
//	const unsigned int the_checksum = checksum();//save checksum

	//if(::atDebugLevel(DEBUG_LEVEL_VERBOSE))
	//	conPrint("temp tris mem usage: " + ::getNiceByteSize(tris->size() * sizeof(js::Triangle)));//TEMP

	

	//free tris array
	//tris = std::auto_ptr<TRIANGLE_VECTOR_TYPE>(NULL);


	//------------------------------------------------------------------------
	//calc root node's aabbox
	//------------------------------------------------------------------------
	triTreeDebugPrint("calcing root AABB.");
	{
	root_aabb->min_ = triVertPos(0, 0);//(*tris)[0].getVertex(0);
	root_aabb->max_ = triVertPos(0, 0);//(*tris)[0].getVertex(0);
	for(unsigned int i=0; i<numTris(); ++i)
		for(int v=0; v<3; ++v)
			root_aabb->enlargeToHoldPoint(triVertPos(i, v));//(*tris)[i].getVertex(v));
	}

	triTreeDebugPrint("AABB: (" + ::toString(root_aabb->min_.x) + ", " + ::toString(root_aabb->min_.y) + ", " + ::toString(root_aabb->min_.z) + "), " + 
						"(" + ::toString(root_aabb->max_.x) + ", " + ::toString(root_aabb->max_.y) + ", " + ::toString(root_aabb->max_.z) + ")"); 
							
	assert(root_aabb->invariant());

	//TreeNode::calcAABB(tripointers, min, max);
	
	//TEMP: HACK:
	//const float EPS = 0.0f;
	//aabb.min -= Vec3(EPS, EPS, EPS);
	//aabb.max += Vec3(EPS, EPS, EPS);
	//rootnode->calcAABB(tripointers);
	//nodes[0].calcAABB(tripointers);

	//------------------------------------------------------------------------
	//compute max allowable depth
	//------------------------------------------------------------------------
	//const int numtris = tris->size();
	//max_depth = (int)(2.0f + logBase2((float)numtris) * 1.2f);
	max_depth = myMin(
		(int)(2.0f + logBase2((float)numTris()) * 2.0f),
		(int)MAX_KDTREE_DEPTH-1
		);

	triTreeDebugPrint("max tree depth: " + ::toString(max_depth));

	// Aalloc stack vector
	nodestack_size = max_depth + 1;
	//alignedSSEArrayMalloc(nodestack_size, nodestack);

#ifdef SSE_TRAVERSAL
	bundlenodestack_size = max_depth + 1;
	alignedSSEArrayMalloc(bundlenodestack_size, bundlenodestack);
#endif
	
	const int expected_numnodes = (int)((float)numTris() * 1.0f);
	const int nodemem = expected_numnodes * sizeof(js::TreeNode);

	//triTreeDebugPrint("reserving N nodes: " + ::toString(expected_numnodes) + "(" 
	//	+ ::getNiceByteSize(nodemem) + ")");

	//TEMP NO RESERVE 
	//nodes.reserve(expected_numnodes);

	//------------------------------------------------------------------------
	//reserve space for leaf geom array
	//------------------------------------------------------------------------
	leafgeom.reserve(numTris() * 4);

	const int leafgeommem = leafgeom.capacity() * sizeof(TRI_INDEX);
	triTreeDebugPrint("leafgeom reserved: " + ::toString(leafgeom.capacity()) + "("
		+ ::getNiceByteSize(leafgeommem) + ")");

	//------------------------------------------------------------------------
	//get array of tri indices for root node
	//------------------------------------------------------------------------
	//std::vector<TRI_INDEX> roottris(numTris());
	//{
	//for(unsigned int i=0; i<numTris(); ++i)
	//	roottris[i] = i;
	//}

	//const int tris_size = tris->size();

	//------------------------------------------------------------------------
	//Create array of triangle AABBoxes
	//------------------------------------------------------------------------
	/*alignedSSEArrayMalloc(numTris(), tri_boxes);

	for(unsigned int i=0; i<numTris(); ++i)
	{
		tri_boxes[i].min = tri_boxes[i].max = triVertPos(i, 0);//(*tris)[i].v0();
		tri_boxes[i].enlargeToHoldPoint(triVertPos(i, 1));//(*tris)[i].v1());
		tri_boxes[i].enlargeToHoldPoint(triVertPos(i, 2));//(*tris)[i].v2());

		assert(tri_boxes[i].invariant());
	}

	if(::atDebugLevel(DEBUG_LEVEL_VERBOSE))
		conPrint("tri_boxes mem usage: " + ::getNiceByteSize(numTris() * sizeof(js::AABBox)));//TEMP*/


	//triTreeDebugPrint("sizeof(js::EdgeTri): " + ::toString(sizeof(js::EdgeTri)));

	//triTreeDebugPrint("calling doBuild()...");
	{
	std::vector<float> lower(numTris());
	std::vector<float> upper(numTris());
	std::vector<std::vector<TRI_INDEX> > node_tri_layers(MAX_KDTREE_DEPTH);//1 list of tris for each depth

	//for(int t=0; t<MAX_KDTREE_DEPTH; ++t)//TEMP
	//	node_tri_layers[t].reserve(1000);
	
	node_tri_layers[0].resize(numTris());
	for(unsigned int i=0; i<numTris(); ++i)
		node_tri_layers[0][i] = i;

	doBuild(0, node_tri_layers, 0, max_depth, *root_aabb, lower, upper);
	}
	//triTreeDebugPrint("doBuild() done.");

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
	//free aabox array
	//------------------------------------------------------------------------
	//alignedSSEFree(tri_boxes);
	//tri_boxes = NULL;

	//------------------------------------------------------------------------
	//alloc intersect tri array
	//------------------------------------------------------------------------
	num_intersect_tris = numTris();
	triTreeDebugPrint("Allocing intersect triangles...");
	if(::atDebugLevel(DEBUG_LEVEL_VERBOSE))
		triTreeDebugPrint("intersect_tris mem usage: " + ::getNiceByteSize(num_intersect_tris * sizeof(INTERSECT_TRI_TYPE)));//TEMP

	::alignedSSEArrayMalloc(num_intersect_tris, intersect_tris);

	//copy tri data from tris array
	for(unsigned int i=0; i<num_intersect_tris; ++i)
		intersect_tris[i].set(triVertPos(i, 0), triVertPos(i, 1), triVertPos(i, 2));
		//intersect_tris[i].set((*tris)[i].v0(), (*tris)[i].v1(), (*tris)[i].v2());

	

	//------------------------------------------------------------------------
	//free temporary build mem
	//------------------------------------------------------------------------
	//tris = std::auto_ptr<TRIANGLE_VECTOR_TYPE>(NULL);

	postBuild();

	
}



void TriTree::buildFromStream(std::istream& stream)
{
	//triTreeDebugPrint("-----------TriTree::build()-------------");
	//triTreeDebugPrint(::toString(tris->size()) + " tris.");

	/*if(tris->empty())
	{
		assert(0);
		return;
	}*/

	//if(::atDebugLevel(DEBUG_LEVEL_VERBOSE))
	//	conPrint("temp tris mem usage: " + ::getNiceByteSize(tris->size() * sizeof(js::Triangle)));//TEMP

	//------------------------------------------------------------------------
	//alloc aligned edge tri array
	//------------------------------------------------------------------------
	num_intersect_tris = numTris();
	::alignedSSEArrayMalloc(num_intersect_tris, intersect_tris);

	//copy tri data from tris array
	for(unsigned int i=0; i<num_intersect_tris; ++i)
		intersect_tris[i].set(triVertPos(i, 0), triVertPos(i, 1), triVertPos(i, 2));  //(*tris)[i].v0(), (*tris)[i].v1(), (*tris)[i].v2());

	if(::atDebugLevel(DEBUG_LEVEL_VERBOSE))
		triTreeDebugPrint("intersect_tris mem usage: " + ::getNiceByteSize(num_intersect_tris * sizeof(js::BadouelTri)));//TEMP

	//free tris array
	//tris = std::auto_ptr<TRIANGLE_VECTOR_TYPE>(NULL);


	//------------------------------------------------------------------------
	//calc root node's aabbox
	//------------------------------------------------------------------------
	triTreeDebugPrint("calcing root AABB.");
	{
	root_aabb->min_ = triVertPos(0, 0);//(*tris)[0].getVertex(0);
	root_aabb->max_ = triVertPos(0, 0);//(*tris)[0].getVertex(0);
	for(unsigned int i=0; i<num_intersect_tris; ++i)
		for(unsigned int v=0; v<3; ++v)
			root_aabb->enlargeToHoldPoint(triVertPos(i, v));//(*tris)[i].getVertex(v));
	}

	//triTreeDebugPrint("AABB: (" + ::toString(root_aabb.min.x) + ", " + ::toString(root_aabb.min.y) + ", " + ::toString(root_aabb.min.z) + "), " + 
	//					"(" + ::toString(root_aabb.max.x) + ", " + ::toString(root_aabb.max.y) + ", " + ::toString(root_aabb.max.z) + ")"); 
							
	assert(root_aabb->invariant());


	//------------------------------------------------------------------------
	//compute max allowable depth
	//------------------------------------------------------------------------
	//const int numtris = tris->size();
	//max_depth = (int)(2.0f + logBase2((float)numtris) * 1.2f);
	max_depth = (int)(2.0f + logBase2((float)numTris()) * 2.0f);

	triTreeDebugPrint("max tree depth: " + ::toString(max_depth));

	// Aalloc stack vector
	nodestack_size = max_depth + 1;
	//alignedSSEArrayMalloc(nodestack_size, nodestack);

#ifdef SSE_TRAVERSAL
	bundlenodestack_size = max_depth + 1;
	alignedSSEArrayMalloc(bundlenodestack_size, bundlenodestack);
#endif
	
	//------------------------------------------------------------------------
	//read nodes
	//------------------------------------------------------------------------
	unsigned int num_nodes;
	stream.read((char*)&num_nodes, sizeof(unsigned int));
	//stream >> num_nodes;

	nodes.resize(num_nodes);
	stream.read((char*)&nodes[0], sizeof(js::TreeNode) * num_nodes);

	//------------------------------------------------------------------------
	//read leafgeom
	//------------------------------------------------------------------------
	unsigned int leaf_geom_count;
	stream.read((char*)&leaf_geom_count, sizeof(unsigned int));
	//stream >> leaf_geom_count;

	leafgeom.resize(leaf_geom_count);
	stream.read((char*)&leafgeom[0], sizeof(TRI_INDEX) * leaf_geom_count);


	triTreeDebugPrint("total nodes used: " + ::toString(num_nodes) + " (" + 
		::getNiceByteSize(num_nodes * sizeof(js::TreeNode)) + ")");

	triTreeDebugPrint("total leafgeom size: " + ::toString(leaf_geom_count) + " (" + 
		::getNiceByteSize(leaf_geom_count * sizeof(TRI_INDEX)) + ")");

	postBuild();
}

void TriTree::postBuild() const
{
	triTreeDebugPrint("finished building tree.");

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


static inline void getMinAndMax(float a, float b, float c, float& min_out, float& max_out)
{
	if(a <= b)
	{
		if(a <= c)
		{
			min_out = a;
			max_out = b >= c ? b : c;
		}
		else //else c < a
		{
			min_out = c;
			max_out = b;
		}
	}
	else // else a > b == b < a
	{
		if(b <= c)
		{
			min_out = b;
			max_out = a >= c ? a : c;
		}
		else //else c < b
		{
			min_out = c;
			max_out = a;
		}
	}
	assert(min_out == myMin(a, myMin(b, c)));
	assert(max_out == myMax(a, myMax(b, c)));
}


//precondition: node_tri_layers[depth] is valid and contains all tris for this volume.

void TriTree::doBuild(unsigned int cur, //index of current node getting built
						//const std::vector<TRI_INDEX>& nodetris,//tris for current node
						std::vector<std::vector<TRI_INDEX> >& node_tri_layers,
						unsigned int depth, //depth of current node
						unsigned int maxdepth, //max permissible depth
						const AABBox& cur_aabb, //AABB of current node
						std::vector<float>& upper, std::vector<float>& lower)
{
	//TEMP:
#ifdef DEBUG
	{
	float minval, maxval;
	getMinAndMax(1., 2., 3., minval, maxval);
	assert(minval == 1. && maxval == 3.);
	getMinAndMax(2., 1., 3., minval, maxval);
	assert(minval == 1. && maxval == 3.);
	getMinAndMax(2., 3., 1., minval, maxval);
	assert(minval == 1. && maxval == 3.);
	}
#endif

	// Get the list of tris intersecting this volume
	assert(depth < (unsigned int)node_tri_layers.size());
	const std::vector<TRI_INDEX>& nodetris = node_tri_layers[depth];

	assert(cur < (int)nodes.size());
	if(depth == 6)
	{
		triTreeDebugPrint(::toString(numnodesbuilt) + "/" + ::toString(1 << 6) + " nodes at depth 6 built.");
		numnodesbuilt++;
	}
	
	if(nodetris.size() == 0)
	{
		//no tris were allocated to this node, so make it a leaf node with 0 tris
		nodes[cur].setLeafNode(true);
		nodes[cur].setLeafGeomIndex(0);
		nodes[cur].setNumLeafGeom(0);
		return;
	}
	//------------------------------------------------------------------------
	//test for termination of splitting
	//------------------------------------------------------------------------
	const int SPLIT_THRESHOLD = 2;//TEMP

	if(nodetris.size() <= SPLIT_THRESHOLD || depth >= maxdepth)
	{
		//make this node a leaf node.
		nodes[cur].setLeafNode(true);
		nodes[cur].setLeafGeomIndex((unsigned int)leafgeom.size());
		nodes[cur].setNumLeafGeom((unsigned int)nodetris.size());

		for(unsigned int i=0; i<nodetris.size(); ++i)
			leafgeom.push_back(nodetris[i]);

		if(depth >= maxdepth)
			num_maxdepth_leafs++;
		else
			num_under_thresh_leafs++;

		return;
	}
	
	const float traversal_cost = 1.0f;
	const float intersection_cost = 4.0f;
	const float aabb_surface_area = cur_aabb.getSurfaceArea();
	const float recip_aabb_surf_area = (1.0f / aabb_surface_area);

	float smallest_cost = std::numeric_limits<float>::max();
	int best_axis = -1;
	float best_div_val = std::numeric_limits<float>::min();
	int best_num_in_neg = 0;
	int best_num_in_pos = 0;

	//------------------------------------------------------------------------
	//compute non-split cost
	//------------------------------------------------------------------------
	smallest_cost = (float)nodetris.size() * intersection_cost;//NEW
	const unsigned int numtris = (unsigned int)nodetris.size();

	const unsigned int nonsplit_axes[3][2] = {{1, 2}, {0, 2}, {0, 1}};

	//std::vector<float> lower(numtris);
	//std::vector<float> upper(numtris);

	for(int axis=0; axis<3; ++axis)
	{
		const unsigned int axis1 = nonsplit_axes[axis][0];
		const unsigned int axis2 = nonsplit_axes[axis][1];
		const float two_cap_area = (cur_aabb.axisLength(axis1) * cur_aabb.axisLength(axis2)) * 2.0f;
		const float circum = (cur_aabb.axisLength(axis1) + cur_aabb.axisLength(axis2)) * 2.0f;
		
		//------------------------------------------------------------------------
		//Sort lower and upper tri AABB bounds into ascending order
		//------------------------------------------------------------------------
		for(unsigned int i=0; i<numtris; ++i)
		{
			/*const float v0 = triVertPos(nodetris[i], 0)[axis];
			const float v1 = triVertPos(nodetris[i], 1)[axis];
			const float v2 = triVertPos(nodetris[i], 2)[axis];
			lower[i] = myMin(v0, myMin(v1, v2));
			upper[i] = myMax(v0, myMax(v1, v2));*/
			getMinAndMax(
				triVertPos(nodetris[i], 0)[axis],
				triVertPos(nodetris[i], 1)[axis],
				triVertPos(nodetris[i], 2)[axis], 
				lower[i], upper[i]);

			/*if(upper[i] == lower[i])
			{
				//TEMP HACK!!!!!!!
				//tri lies totally in the plane perpendicular to the current axis.
				//So nudge up its upper bound a bit to force it into the positive volume.
				float upperval = upper[i];
				upper[i] += 0.000001f;
				upperval = upper[i];
				//TEMP fails on large values assert(upper[i] > lower[i]);
			}*/
		}

		//Note that we don't want to sort the whole buffers here, as we're only using the first numtris elements.
		std::sort(lower.begin(), lower.begin() + numtris); //lower.end());
		std::sort(upper.begin(), upper.begin() + numtris); //upper.end());

		//Tris in contact with splitting plane will not imply tri is in either volme, except
		//for tris lying totally on splitting plane which is considered to be in positive volume.

		unsigned int upper_index = 0;
		//Index of first triangle in upper volume
		//Index of first tri with upper bound >= then current split val.
		//Also the number of tris with upper bound <= current split val.
		//which is the number of tris *not* in pos volume

		float last_splitval = std::numeric_limits<float>::min(); //-1e20f;
		for(unsigned int i=0; i<numtris; ++i)
		{
			const float splitval = lower[i];

			if(splitval != last_splitval)//only consider first tri seen with a given lower bound.
			{
				if(splitval > cur_aabb.min_[axis] && splitval < cur_aabb.max_[axis])
				{
					//advance upper index to maintain invariant above
					//while(upper_index < numtris && upper[upper_index] < splitval) //<= splitval)
					//	upper_index++;

					//advance upper_index while it points to triangles with upper < split (definitely in negative volume)
					while(upper_index < numtris && upper[upper_index] < splitval)
						upper_index++;

					unsigned int num_coplanar_tris = 0;
					//advance upper_index while it points to triangles with upper == split, which may be coplanar and hence in positive volume, if lower == split as well.
					while(upper_index < numtris && upper[upper_index] == splitval)
					{
						if(lower[upper_index] == splitval)
							num_coplanar_tris++;
						upper_index++;
					}

					assert(upper_index == numtris || upper[upper_index] > splitval);

					const int num_in_neg = i;//upper_index;
					const int num_in_pos = num_coplanar_tris + numtris - upper_index;//numtris - i;
					assert(num_in_neg >= 0 && num_in_neg <= (int)numtris);
					assert(num_in_pos >= 0 && num_in_pos <= (int)numtris);
					assert(num_in_neg + num_in_pos >= (int)numtris);
					const float negchild_surface_area = two_cap_area + (splitval - cur_aabb.min_[axis]) * circum;
					const float poschild_surface_area = two_cap_area + (cur_aabb.max_[axis] - splitval) * circum;
					assert(negchild_surface_area >= 0.f && negchild_surface_area <= aabb_surface_area);
					assert(poschild_surface_area >= 0.f && negchild_surface_area <= aabb_surface_area);
					assert(::epsEqual(negchild_surface_area + poschild_surface_area - two_cap_area, aabb_surface_area, 0.001f));


					const float cost = traversal_cost + 
						((float)num_in_neg * negchild_surface_area + (float)num_in_pos * poschild_surface_area) * recip_aabb_surf_area * intersection_cost;

					assert(cost >= 0.0);

					/*const float neg_prob = negchild_surface_area * recip_aabb_surf_area;
					assert(neg_prob >= 0.0 && neg_prob <= 1.0);
					const float pos_prob = poschild_surface_area * recip_aabb_surf_area;
					assert(pos_prob >= 0.0 && pos_prob <= 1.0);

					const float neg_cost = traversal_cost * logBase2((float)(myMax(1, num_in_neg))) + 2.0 * intersection_cost;
					const float pos_cost = traversal_cost * logBase2((float)(myMax(1, num_in_pos))) + 2.0 * intersection_cost;

					const float cost = traversal_cost + neg_prob * neg_cost + pos_prob * pos_cost;*/

					if(cost < smallest_cost)
					{
						smallest_cost = cost;
						best_axis = axis;
						best_div_val = splitval;
						best_num_in_neg = num_in_neg;
						best_num_in_pos = num_in_pos;
					}
				}
				last_splitval = splitval;
			}
		}
		unsigned int lower_index = 0;//index of first tri with lower bound >= current split val
		//Also the number of tris that are in the negative volume

		for(unsigned int i=0; i<numtris; ++i)
		{
			unsigned int num_coplanar_tris = lower[i] == upper[i] ? 1 : 0;
			//if tri i and tri i+1 share upper bounds, advance to largest index of tris sharing same upper bound
			while(i<numtris-1 && upper[i] == upper[i+1])
			{
				++i;
				if(lower[i] == upper[i])
					num_coplanar_tris++;
			}


			assert(i < upper.size());
			const float splitval = upper[i];
			
			//if(splitval != last_splitval)
			//{
				if(splitval > cur_aabb.min_[axis] && splitval < cur_aabb.max_[axis])
				{
					//advance lower index to maintain invariant above
					while(lower_index < numtris && lower[lower_index] < splitval)
						lower_index++;

					assert(lower_index == numtris || lower[lower_index] >= splitval);

					const int num_in_neg = lower_index;//i;
					const int num_in_pos = num_coplanar_tris + numtris - (i + 1);//numtris - lower_index;
					assert(num_in_neg >= 0 && num_in_neg <= (int)numtris);
					assert(num_in_pos >= 0 && num_in_pos <= (int)numtris);
					assert(num_in_neg + num_in_pos >= (int)numtris);
					const float negchild_surface_area = two_cap_area + (splitval - cur_aabb.min_[axis]) * circum;
					const float poschild_surface_area = two_cap_area + (cur_aabb.max_[axis] - splitval) * circum;
					assert(negchild_surface_area >= 0.f && negchild_surface_area <= aabb_surface_area);
					assert(poschild_surface_area >= 0.f && negchild_surface_area <= aabb_surface_area);
					assert(::epsEqual(negchild_surface_area + poschild_surface_area - two_cap_area, aabb_surface_area, 0.001f));

					const float cost = traversal_cost + 
						((float)num_in_neg * negchild_surface_area + (float)num_in_pos * poschild_surface_area) * recip_aabb_surf_area * intersection_cost;

					assert(cost >= 0.0);

					/*const float neg_prob = negchild_surface_area * recip_aabb_surf_area;
					assert(neg_prob >= 0.0 && neg_prob <= 1.0);
					const float pos_prob = poschild_surface_area * recip_aabb_surf_area;
					assert(pos_prob >= 0.0 && pos_prob <= 1.0);

					const float neg_cost = traversal_cost * logBase2((float)(myMax(1, num_in_neg))) + 2.0 * intersection_cost;
					const float pos_cost = traversal_cost * logBase2((float)(myMax(1, num_in_pos))) + 2.0 * intersection_cost;

					const float cost = traversal_cost + neg_prob * neg_cost + pos_prob * pos_cost;*/

					if(cost < smallest_cost)
					{
						smallest_cost = cost;
						best_axis = axis;
						best_div_val = splitval;
						best_num_in_neg = num_in_neg;
						best_num_in_pos = num_in_pos;
					}
				}
				//last_splitval = splitval;
			//}
		}
	}

	if(best_axis == -1)
	{
		//if the least cost is to not split the node...
		nodes[cur].setLeafNode(true);
		nodes[cur].setLeafGeomIndex((unsigned int)leafgeom.size());
		nodes[cur].setNumLeafGeom((unsigned int)nodetris.size());

		for(unsigned int i=0; i<nodetris.size(); ++i)
			leafgeom.push_back(nodetris[i]);

		num_inseparable_tri_leafs++;
		return;
	}

	assert(best_axis >= 0 && best_axis <= 2);
	assert(best_div_val != -666.0f);

	assert(best_div_val >= cur_aabb.min_[best_axis]);
	assert(best_div_val <= cur_aabb.max_[best_axis]);
	
	nodes[cur].setSplittingAxis(best_axis);
	nodes[cur].data2.dividing_val = best_div_val;

	//------------------------------------------------------------------------
	//compute AABBs of child nodes
	//------------------------------------------------------------------------
	assert(nodes[cur].getSplittingAxis() >= 0 && nodes[cur].getSplittingAxis() <= 2);

	AABBox negbox(cur_aabb.min_, cur_aabb.max_);
	negbox.max_[nodes[cur].getSplittingAxis()] = nodes[cur].data2.dividing_val;

	AABBox posbox(cur_aabb.min_, cur_aabb.max_);
	posbox.min_[nodes[cur].getSplittingAxis()] = nodes[cur].data2.dividing_val;

	//NEW: moved this up here
	//if(neg_tris.size() == nodetris.size() && pos_tris.size() == nodetris.size())
	if(best_num_in_neg == numtris && best_num_in_pos == numtris)
	{
		//If we were unable to get a reduction in the number of tris in either of the children,
		//then splitting is pointless.  So make this a leaf node.
		nodes[cur].setLeafNode(true);
		nodes[cur].setLeafGeomIndex((unsigned int)leafgeom.size());
		nodes[cur].setNumLeafGeom((unsigned int)nodetris.size());

		for(unsigned int i=0; i<(unsigned int)nodetris.size(); ++i)
			leafgeom.push_back(nodetris[i]);
		return;
	}


	//------------------------------------------------------------------------
	//assign tris to neg and pos child node bins
	//------------------------------------------------------------------------
	/*std::vector<TRI_INDEX> neg_tris;
	neg_tris.reserve(best_num_in_neg);
	std::vector<TRI_INDEX> pos_tris;
	pos_tris.reserve(best_num_in_pos);

	assert(numtris == nodetris.size());
	const float split = best_div_val;
	assert(best_axis == nodes[cur].getSplittingAxis());
	for(unsigned int i=0; i<numtris; ++i)//for each tri
	{
		// Get lower and upper bound of tri projection along axis
		//const float v0 = triVertPos(nodetris[i], 0)[best_axis];
		//const float v1 = triVertPos(nodetris[i], 1)[best_axis];
		//const float v2 = triVertPos(nodetris[i], 2)[best_axis];
		//const float tri_upper = myMax(v0, myMax(v1, v2));
		//const float tri_lower = myMin(v0, myMin(v1, v2));
		float tri_lower, tri_upper;
		getMinAndMax(
				triVertPos(nodetris[i], 0)[best_axis],
				triVertPos(nodetris[i], 1)[best_axis],
				triVertPos(nodetris[i], 2)[best_axis], 
				tri_lower, tri_upper);

		if(tri_upper <= split)//does tri lie entirely in min volume (including splitting plane)?
		{
			//tri is either in neg box, or on splitting plane
			if(tri_lower >= split)
				pos_tris.push_back(nodetris[i]);//tri lies entirely on splitting plane.  so assign it to positive half.
			else
				neg_tris.push_back(nodetris[i]);
		}
		else //else upper[i] > split
		{
			if(tri_lower < split)
			{
				//tri lies in both boxes
				neg_tris.push_back(nodetris[i]);
				pos_tris.push_back(nodetris[i]);
			}
			else //else lower >= split[i]
				pos_tris.push_back(nodetris[i]);//tri lies entirely in pos volume.
		}
	}*/

	
#if 0
	/*	const int num_in_neg = (int)neg_tris.size();
	const int num_in_pos = (int)pos_tris.size();
	assert(num_in_neg == best_num_in_neg);
	assert(num_in_pos == best_num_in_pos);
	const int num_tris_in_both = neg_tris.size() + pos_tris.size() - numtris;
*/
	//::triTreeDebugPrint("split " + ::toString(nodetris.size()) + " tris: left: " + ::toString(neg_tris.size())
	//	+ ", right: " + ::toString(pos_tris.size()) + ", both: " + ::toString(num_tris_in_both) );

	if(neg_tris.size() == nodetris.size() && pos_tris.size() == nodetris.size())
	{
		//If we were unable to get a reduction in the number of tris in either of the children,
		//then splitting is pointless.  So make this a leaf node.
		nodes[cur].setLeafNode(true);
		nodes[cur].setLeafGeomIndex(leafgeom.size());
		nodes[cur].setNumLeafGeom(nodetris.size());

		for(unsigned int i=0; i<nodetris.size(); ++i)
			leafgeom.push_back(nodetris[i]);
		return;
	}
#endif

	assert(depth + 1 < (int)node_tri_layers.size());
	std::vector<TRI_INDEX>& child_tris = node_tri_layers[depth + 1];

	/// Build list of neg tris ///
	child_tris.resize(0);
	child_tris.reserve(myMax(best_num_in_neg, best_num_in_pos));

	assert(numtris == nodetris.size());
	const float split = best_div_val;
	assert(best_axis == nodes[cur].getSplittingAxis());
	for(unsigned int i=0; i<numtris; ++i)//for each tri
	{
		float tri_lower, tri_upper;
		getMinAndMax(
				triVertPos(nodetris[i], 0)[best_axis],
				triVertPos(nodetris[i], 1)[best_axis],
				triVertPos(nodetris[i], 2)[best_axis], 
				tri_lower, tri_upper);

		if(tri_upper <= split)//does tri lie entirely in min volume (including splitting plane)?
		{
			//tri is either in neg box, or on splitting plane
			if(tri_lower < split)
				child_tris.push_back(nodetris[i]);
		}
		else //else upper[i] > split
		{
			if(tri_lower < split)
				child_tris.push_back(nodetris[i]);//tri lies in both boxes
		}
	}

	//conPrint("Finished binning neg tris, depth=" + toString(depth) + ", size=" + toString(child_tris.size()) + ", capacity=" + toString(child_tris.capacity()));

	//------------------------------------------------------------------------
	//create negative child node, next in the array.
	//------------------------------------------------------------------------
	nodes.push_back(TreeNode());
	//const int negchild_index = nodes.size() - 1;

	//build left subtree, recursive call.
	doBuild(
		(unsigned int)nodes.size() - 1, //negchild_index, 
		node_tri_layers, depth + 1, maxdepth, negbox, lower, upper);
	//doBuild(negchild_index, neg_tris, depth + 1, maxdepth, negbox, lower, upper);

	//Build list of positive tris
	//std::vector<TRI_INDEX> pos_tris;
	//pos_tris.reserve(best_num_in_pos);
	child_tris.resize(0);
	//child_tris.reserve(best_num_in_pos);

	assert(numtris == nodetris.size());
	assert(best_axis == nodes[cur].getSplittingAxis());
	for(unsigned int i=0; i<numtris; ++i)//for each tri
	{
		float tri_lower, tri_upper;
		getMinAndMax(
				triVertPos(nodetris[i], 0)[best_axis],
				triVertPos(nodetris[i], 1)[best_axis],
				triVertPos(nodetris[i], 2)[best_axis], 
				tri_lower, tri_upper);

		if(tri_upper <= split)//does tri lie entirely in min volume (including splitting plane)?
		{
			//tri is either in neg box, or on splitting plane
			if(tri_lower >= split)
				child_tris.push_back(nodetris[i]);//tri lies entirely on splitting plane.  so assign it to positive half.
		}
		else //else upper[i] > split
			child_tris.push_back(nodetris[i]);
	}

	//conPrint("Finished binning pos tris, depth=" + toString(depth) + ", size=" + toString(child_tris.size()) + ", capacity=" + toString(child_tris.capacity()));


	//------------------------------------------------------------------------
	//create positive child
	//------------------------------------------------------------------------
	nodes.push_back(TreeNode());
	nodes[cur].setPosChildIndex((unsigned int)nodes.size() - 1);

	//build right subtree
	doBuild(nodes[cur].getPosChildIndex(), node_tri_layers, depth + 1, maxdepth, posbox, lower, upper);
	//doBuild(nodes[cur].getPosChildIndex(), pos_tris, depth + 1, maxdepth, posbox, lower, upper);
}


void TriTree::printTree(unsigned int cur, unsigned int depth, std::ostream& out)
{
	if(nodes[cur].isLeafNode())
	{
		for(unsigned int i=0; i<depth; ++i)
			out << "  ";
//		out << "leaf node (num leaf tris: " << nodes[cur].num_leaf_tris << ")\n";
	}
	else
	{	
		//process neg child
		this->printTree(cur + 1, depth + 1, out);

		for(unsigned int i=0; i<depth; ++i)
			out << "  ";
//		out << "interior node (split axis: "  << nodes[cur].dividing_axis << ", split val: "
//				<< nodes[cur].dividing_val << ")\n";
		//process pos child
		this->printTree(nodes[cur].getPosChildIndex(), depth + 1, out);
	}
}

void TriTree::debugPrintTree(unsigned int cur, unsigned int depth)
{
	if(nodes[cur].isLeafNode())
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


TreeStats::TreeStats()
{
	memset(this, 0, sizeof(TreeStats));
}

TreeStats::~TreeStats()
{
}

void TriTree::getTreeStats(TreeStats& stats_out, unsigned int cur, unsigned int depth)
{
	if(nodes.empty())
		return;

	if(nodes[cur].isLeafNode())
	{
		stats_out.num_leaf_nodes++;
		stats_out.num_leaf_geom_tris += (int)nodes[cur].getNumLeafGeom();
		stats_out.average_leafnode_depth += (double)depth;
	}
	else
	{
		stats_out.num_interior_nodes++;

		//------------------------------------------------------------------------
		//recurse to children
		//------------------------------------------------------------------------
		this->getTreeStats(stats_out, cur + 1, depth + 1);
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
	
		stats_out.max_depth = max_depth;

		stats_out.total_node_mem = (int)nodes.size() * sizeof(TreeNode);
		stats_out.leafgeom_indices_mem = stats_out.num_leaf_geom_tris * sizeof(TRI_INDEX);
		stats_out.tri_mem = num_intersect_tris * sizeof(INTERSECT_TRI_TYPE);

		stats_out.num_inseparable_tri_leafs = this->num_inseparable_tri_leafs;
		stats_out.num_maxdepth_leafs = this->num_maxdepth_leafs;
		stats_out.num_under_thresh_leafs = this->num_under_thresh_leafs;
	}
}



	//returns dist till hit tri, neg number if missed.
double TriTree::traceRayAgainstAllTris(const ::Ray& ray, double t_max, HitInfo& hitinfo_out) const
{
	hitinfo_out.hittriindex = 0;
	hitinfo_out.hittricoords.set(0.f, 0.f);

	double closest_dist = std::numeric_limits<double>::max(); //2e9f;//closest hit so far, also upper bound on hit dist

	for(unsigned int i=0; i<num_intersect_tris; ++i)
	{
		float u, v, raydist;
		//raydist is distance until hit if hit occurred
				
		if(intersect_tris[i].rayIntersect(ray, (float)closest_dist, raydist, u, v))
		{			
			assert(raydist < closest_dist);

			closest_dist = raydist;
			hitinfo_out.hittriindex = i;
			hitinfo_out.hittricoords.set(u, v);
		}
	}

	if(closest_dist > t_max)
		closest_dist = -1.0;

	return closest_dist;
}

void TriTree::getAllHitsAllTris(const Ray& ray, std::vector<FullHitInfo>& hitinfos_out) const
{
	for(unsigned int i=0; i<num_intersect_tris; ++i)
	{
		float u, v, raydist;
		//raydist is distance until hit if hit occurred
				
		if(intersect_tris[i].rayIntersect(ray, std::numeric_limits<float>::max(), raydist, u, v))
		{			
			hitinfos_out.push_back(FullHitInfo());
			hitinfos_out.back().dist = raydist;
			hitinfos_out.back().hittri_index = i;
			hitinfos_out.back().tri_coords.set(u, v);
		}
	}
}


js::TriTreePerThreadData* allocPerThreadData()
{
	return new js::TriTreePerThreadData();
}

void TriTree::saveTree(std::ostream& stream)
{
	//write checksum
	const unsigned int the_checksum = checksum();
	stream.write((const char*)&the_checksum, sizeof(unsigned int));

	//------------------------------------------------------------------------
	//write nodes
	//------------------------------------------------------------------------
	unsigned int temp = (unsigned int)nodes.size();
	stream.write((const char*)&temp, sizeof(unsigned int));
	//stream << nodes.size();
	
	stream.write((const char*)&nodes[0], sizeof(js::TreeNode)*(unsigned int)nodes.size());

	//------------------------------------------------------------------------
	//write leafgeom
	//------------------------------------------------------------------------
	temp = leafgeom.size();
	stream.write((const char*)&temp, sizeof(unsigned int));
	//stream << leafgeom.size();

	stream.write((const char*)&leafgeom[0], sizeof(TRI_INDEX)*leafgeom.size());

}

//checksum over tris
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



} //end namespace jscol















