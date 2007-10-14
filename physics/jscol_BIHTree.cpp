/*=====================================================================
BIHTree.cpp
-----------
File created by ClassTemplate on Sun Nov 26 21:59:32 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "jscol_BIHTree.h"


#include "jscol_aabbox.h"
#include "jscol_TriTreePerThreadData.h"
#include "TreeUtils.h"
#include "../simpleraytracer/raymesh.h"
#include "../raytracing/hitinfo.h"
#include "../indigo/FullHitInfo.h"
#include "../utils/timer.h"
#include "../utils/random.h"
#include <limits>

namespace js
{


//#define RECORD_TRACE_STATS 1
//#define VERBOSE_TRACE 1
//#define VERBOSE_BUILD 1

BIHTree::BIHTree(RayMesh* raymesh_)
:	raymesh(raymesh_)
{
	root_aabb = (js::AABBox*)alignedSSEMalloc(sizeof(AABBox));
	new(root_aabb) AABBox(Vec3f(0,0,0), Vec3f(0,0,0));

	num_intersect_tris = 0;
	intersect_tris = NULL;

	num_inseparable_tri_leafs = 0;
	num_maxdepth_leafs = 0;
	num_under_thresh_leafs = 0;

	num_traces = 0.;
	total_num_nodes_touched = 0.;
	total_num_leafs_touched = 0.;
	total_num_tris_intersected = 0.;
}


BIHTree::~BIHTree()
{
}


const Vec3f& BIHTree::triVertPos(unsigned int tri_index, unsigned int vert_index_in_tri) const
{
	return raymesh->triVertPos(tri_index, vert_index_in_tri);
}

unsigned int BIHTree::numTris() const
{
	return raymesh->getNumTris();
}


/*void BIHTree::AABBoxForTri(unsigned int tri_index, AABBox& aabbox_out)
{
	aabbox_out.min = aabbox_out.max = triVertPos(tri_index, 0);
	aabbox_out.enlargeToHoldPoint(triVertPos(tri_index, 1));
	aabbox_out.enlargeToHoldPoint(triVertPos(tri_index, 2));

	assert(aabbox_out.invariant());
}*/

float BIHTree::triMaxPos(unsigned int tri_index, unsigned int axis) const
{
	return myMax(triVertPos(tri_index, 0)[axis], myMax(triVertPos(tri_index, 1)[axis], triVertPos(tri_index, 2)[axis]));
}

float BIHTree::triMinPos(unsigned int tri_index, unsigned int axis) const
{
	return myMin(triVertPos(tri_index, 0)[axis], myMin(triVertPos(tri_index, 1)[axis], triVertPos(tri_index, 2)[axis]));
}

void BIHTree::build()
{
	//raymesh = raymesh_;

	conPrint("\tBIHTree::build()");
	Timer buildtimer;
	//conPrint("\t" + ::toString(numTris()) + " tris.");

	//------------------------------------------------------------------------
	//calc root node's aabbox
	//NOTE: could do this faster by looping over vertices instead.
	//------------------------------------------------------------------------
	conPrint("\tcalcing root AABB.");
	root_aabb->min_ = triVertPos(0, 0);
	root_aabb->max_ = triVertPos(0, 0);
	for(unsigned int i=0; i<numTris(); ++i)
	{
		root_aabb->enlargeToHoldPoint(triVertPos(i, 0));
		root_aabb->enlargeToHoldPoint(triVertPos(i, 1));
		root_aabb->enlargeToHoldPoint(triVertPos(i, 2));
	}
	conPrint("\tdone.");
	conPrint("\tRoot AABB min: " + root_aabb->min_.toString());
	conPrint("\tRoot AABB max: " + root_aabb->max_.toString());

	{
	std::vector<TRI_INDEX> tris(numTris());
	for(unsigned int i=0; i<numTris(); ++i)
		tris[i] = i;

	nodes.resize(1);
	AABBox tri_aabb = *root_aabb;
	doBuild(*root_aabb, tri_aabb, tris, 0, numTris(), 0, 0);
	}

	//------------------------------------------------------------------------
	//alloc intersect tri array
	//------------------------------------------------------------------------
	conPrint("\tAllocing and copying intersect triangles...");
	this->num_intersect_tris = numTris();
	if(::atDebugLevel(DEBUG_LEVEL_VERBOSE))
		conPrint("\tintersect_tris mem usage: " + ::getNiceByteSize(num_intersect_tris * sizeof(INTERSECT_TRI_TYPE)));

	::alignedSSEArrayMalloc(num_intersect_tris, intersect_tris);

	//copy tri data
	for(unsigned int i=0; i<num_intersect_tris; ++i)
		intersect_tris[i].set(triVertPos(i, 0), triVertPos(i, 1), triVertPos(i, 2));
	conPrint("\tdone.");


	conPrint("\tBuild Stats:");
	conPrint("\t\ttotal leafgeom size: " + ::toString((unsigned int)leafgeom.size()) + " (" + ::getNiceByteSize(leafgeom.size() * sizeof(TRI_INDEX)) + ")");
	conPrint("\t\ttotal nodes used: " + ::toString((unsigned int)nodes.size()) + " (" + ::getNiceByteSize(nodes.size() * sizeof(js::BIHTreeNode)) + ")");
	conPrint("\t\tnumInternalNodes: " + toString(numInternalNodes()));
	conPrint("\t\tnumLeafNodes: " + toString(numLeafNodes()));
	conPrint("\t\tnum_under_thresh_leafs: " + toString(num_under_thresh_leafs));
	conPrint("\t\tnum_maxdepth_leafs: " + toString(num_maxdepth_leafs));
	conPrint("\t\tnum_inseparable_tri_leafs: " + toString(num_inseparable_tri_leafs));
	conPrint("\t\tmeanNumTrisPerLeaf: " + toString(meanNumTrisPerLeaf()));
	conPrint("\t\tmaxNumTrisPerLeaf: " + toString(maxNumTrisPerLeaf()));


	conPrint("\tFinished building tree. (Build time: " + toString(buildtimer.getSecondsElapsed()) + "s)");	
}

unsigned int BIHTree::numInternalNodes() const
{
	uint32 c = 0;
	for(uint32 i=0; i<nodes.size(); ++i)
		if(!nodes[i].isLeafNode())
			c++;
	return c;
}
unsigned int BIHTree::numLeafNodes() const
{
	uint32 c = 0;
	for(uint32 i=0; i<nodes.size(); ++i)
		if(nodes[i].isLeafNode())
			c++;
	return c;
}
double BIHTree::meanNumTrisPerLeaf() const
{
	double num_tris_per_leaf = 0.;
	for(uint32 i=0; i<nodes.size(); ++i)
		if(nodes[i].isLeafNode())
			num_tris_per_leaf += (double)nodes[i].getNumLeafGeom();
	return num_tris_per_leaf / (double)numLeafNodes();
}
unsigned int BIHTree::maxNumTrisPerLeaf() const
{
	unsigned int num = 0;
	for(uint32 i=0; i<nodes.size(); ++i)
		if(nodes[i].isLeafNode())
			num = myMax(num, nodes[i].getNumLeafGeom());
	return num;
}


/*
The triangles [left, right) are considered to belong to this node.

*/
void BIHTree::doBuild(const AABBox& aabb_, const AABBox& tri_aabb, std::vector<TRI_INDEX>& tris, int left, int right, unsigned int node_index, int depth)
{
	const int MAX_DEPTH = 100;

#ifdef VERBOSE_BUILD
	conPrint("doBuild()");
	printVar(left);
	printVar(right);
	printVar(depth);
	printVar(node_index);
#endif

	//nodes.push_back(BIHTreeNode());
	//if(nodes.size() <= node_index_to_use)
	//	nodes.resize(node_index_to_use+1);

	/*conPrint("-------------------------------------------------------------------");
	conPrint("node_index: " + toString(node_index));
	conPrint("nodes.size(): " + toString(nodes.size()));
	conPrint("left: " + toString(left) + ", right: " + toString(right));
	conPrint("depth: " + toString(depth));*/

	assert(node_index < nodes.size());
	//BIHTreeNode& node = nodes[node_index];

	const int LEAF_NUM_TRI_THRESHOLD = 3;

	if((right - left <= LEAF_NUM_TRI_THRESHOLD) || depth > MAX_DEPTH)
	{
		//------------------------------------------------------------------------
		//Leaf node
		//------------------------------------------------------------------------
		if(right - left <= 1)
			this->num_under_thresh_leafs++;
		else
			this->num_maxdepth_leafs++;

		nodes[node_index].setIsLeafNode(true);

		//add tris
		nodes[node_index].index = leafgeom.size();

		for(int i=left; i<right; ++i)
		{
			assert(i >= 0 && i < (int)numTris());
			leafgeom.push_back(tris[i]);
		}

		nodes[node_index].num_intersect_items = myMax(right - left, 0);
		return;
	}

	//------------------------------------------------------------------------
	//make interior node
	//------------------------------------------------------------------------
	nodes[node_index].setIsLeafNode(false);

	/*const unsigned int split_axis = aabb.longestAxis();
	assert(split_axis >= 0 && split_axis <= 2);
	nodes[node_index].setSplittingAxis(split_axis);
	assert(nodes[node_index].getSplittingAxis() >= 0 && nodes[node_index].getSplittingAxis() <= 2);

	//const float splitval = 0.5f*(aabb.min[split_axis] + aabb.max[split_axis]);
	*/

	AABBox aabb = aabb_;
	float splitval;
	unsigned int split_axis;
	for(int z=0; z<4; ++z)
	{
		split_axis = aabb.longestAxis();
		splitval = 0.5f*(aabb.min_[split_axis] + aabb.max_[split_axis]);
		if(splitval < tri_aabb.min_[split_axis])
			aabb.min_[split_axis] = splitval;
		else if(splitval > tri_aabb.max_[split_axis])
			aabb.max_[split_axis] = splitval;
		else
			break;
	}

	nodes[node_index].setSplittingAxis(split_axis);

	assert(splitval >= aabb.min_[split_axis] && splitval <= aabb.max_[split_axis]);

	
	float negnode_upper = std::numeric_limits<float>::infinity()*-1.f;//aabb.min[split_axis];//upper bound of min child node
	float posnode_lower = std::numeric_limits<float>::infinity();//aabb.max[split_axis];//upper bound of min child node
	//float negnode_upper = aabb.min[split_axis];//upper bound of min child node
	//float posnode_lower = aabb.max[split_axis];//upper bound of min child node

	int d = right-1;
	int i = left;
	while(i <= d)
	{
		//catergorise triangle at i

		assert(i >= 0 && i < (int)numTris());
		assert(d >= 0 && d < (int)numTris());
		assert(tris[i] < numTris());

		const float tri_min = triMinPos(tris[i], split_axis);
		const float tri_max = triMaxPos(tris[i], split_axis);
		//if(::epsEqual(tri_max, 2.7f))
		//	int b = 9;
		if(tri_max - splitval >= splitval - tri_min)
		{
			//if mostly on positive side of split - categorise as R
			posnode_lower = myMin(tri_min, posnode_lower);//update lower bound on positive node

			//swap element at i with element at d
			mySwap(tris[i], tris[d]);

			assert(d >= 0);
			--d;
		}
		else
		{
			//tri mostly on negative side of split - categorise as L
			negnode_upper = myMax(tri_max, negnode_upper);//update upper bound on negative node
			++i;
		}
	}
	assert(i == d+1);

	int num_in_left = (d - left) + 1;
	int num_in_right = right - i;//+1;
	assert(num_in_left >= 0);
	assert(num_in_right >= 0);
	assert(num_in_left + num_in_right == right - left);
	//printVar(num_in_left);
	//printVar(num_in_right);
	//if(num_in_left == 6250)
	//	int a = 9;
	/*int numiters = 0;
	while(((num_in_left == 0 && num_in_right > 2) || (num_in_right == 0 && num_in_left > 2)) && numiters < 100)
	{
		numiters++;
		//pick a random split val
		splitval = aabb.min[split_axis] + Random::unit() * (aabb.max[split_axis] - aabb.min[split_axis]);

		negnode_upper = std::numeric_limits<float>::infinity()*-1.f;//aabb.min[split_axis];//upper bound of min child node
		posnode_lower = std::numeric_limits<float>::infinity();//aabb.max[split_axis];//upper bound of min child node
		//float negnode_upper = aabb.min[split_axis];//upper bound of min child node
		//float posnode_lower = aabb.max[split_axis];//upper bound of min child node

		d = right;
		i = left;
		while(i <= d)
		{
			//catergorise triangle at i

			assert(i >= 0 && i < (int)numTris());
			assert(d >= 0 && d < (int)numTris());
			assert(tris[i] < numTris());

			const float tri_min = triMinPos(tris[i], split_axis);
			const float tri_max = triMaxPos(tris[i], split_axis);
			//if(::epsEqual(tri_max, 2.7f))
			//	int b = 9;
			if(tri_max - splitval >= splitval - tri_min)
			{
				//if mostly on positive side of split - categorise as R
				posnode_lower = myMin(tri_min, posnode_lower);//update lower bound on positive node

				//swap element at i with element at d
				mySwap(tris[i], tris[d]);

				assert(d >= 0);
				--d;
			}
			else
			{
				//tri mostly on negative side of split - categorise as L
				negnode_upper = myMax(tri_max, negnode_upper);//update upper bound on negative node
				++i;
			}
		}
		num_in_left = d-left+1;
		num_in_right = right-i+1;
	}*/

	
	//assert(negnode_upper <= aabb.max[split_axis]);
	//assert(posnode_lower >= aabb.min[split_axis]);

	nodes[node_index].clip[0] = negnode_upper;
	const unsigned int child_index = nodes.size();
	nodes[node_index].setChildIndex(child_index);//make index of left child node 1 past the end of the current node array
	
	//nodes.push_back(BIHTreeNode());
	//nodes.push_back(BIHTreeNode());
	nodes.resize(nodes.size() + 2);//reserve space for children

	//conPrint("min child index: " + toString(child_index));

	/// Build left child AABB ///
	AABBox neg_aabb = aabb;
	AABBox neg_tri_aabb = tri_aabb;
	//neg_tri_aabb.max[split_axis] = neg_aabb.max[split_axis] = negnode_upper;
	neg_tri_aabb.max_[split_axis] = negnode_upper;
	neg_aabb.max_[split_axis] = splitval;

	//recurse to build left subtree
	doBuild(neg_aabb, neg_tri_aabb, tris, left, d + 1, child_index, depth+1);

	//conPrint("max child index: " + toString(child_index + 1));
	nodes[node_index].clip[1] = posnode_lower;
	
	/// Build right child AABB ///
	AABBox pos_aabb = aabb;
	AABBox pos_tri_aabb = tri_aabb;
	//pos_tri_aabb.min[split_axis] = pos_aabb.min[split_axis] = posnode_lower;
	pos_tri_aabb.min_[split_axis] = posnode_lower;
	pos_aabb.min_[split_axis] = splitval;

	//recurse to build right subtree
	doBuild(pos_aabb, pos_tri_aabb, tris, i, right, child_index+1, depth+1);
}








double BIHTree::traceRay(const Ray& ray, double ray_max_t, js::TriTreePerThreadData& context, HitInfo& hitinfo_out) const
{
	assertSSEAligned(&ray);
	assert(ray_max_t >= 0.0f);

#ifdef RECORD_TRACE_STATS
	this->num_traces += 1.;
#endif
#ifdef VERBOSE_TRACE
	conPrint("------------------------- traceRay() ------------------------------");
	conPrint("ray.unitdir: " + ray.unitdir.toStringFullPrecision());
	conPrint("ray.getRecipRayDir(): " + ray.getRecipRayDir().toStringFullPrecision());
#endif

	hitinfo_out.hittriindex = 0;
	hitinfo_out.hittricoords.set(0.f, 0.f);

	////////////// load recip vector ///////////////
	const SSE_ALIGN PaddedVec3& recip_unitraydir = ray.getRecipRayDirF();

	assert(!nodes.empty());
	assert(root_aabb);
	if(nodes.empty())
		return -1.0f;

	float aabb_enterdist, aabb_exitdist;
	if(root_aabb->rayAABBTrace(ray.startPosF(), recip_unitraydir, aabb_enterdist, aabb_exitdist) == 0)
		return -1.0f;//missed aabbox

	const float MIN_TMIN = 0.00000001f;//above zero to avoid denorms
	
	aabb_enterdist = myMax(aabb_enterdist, MIN_TMIN);//TEMP

	if(ray_max_t <= aabb_enterdist)//if this ray finishes before we even enter the aabb...
		return -1.0f;

	//unsigned int ray_child_indices[4][2];
	//TreeUtils::buildRayChildIndices(ray, ray_child_indices);
	SSE_ALIGN unsigned int ray_child_indices[8];
	TreeUtils::buildFlatRayChildIndices(ray, ray_child_indices);


	//assert(ray_child_indices[2][0] == 0 || ray_child_indices[2][0] == 1);
	//assert(ray_child_indices[2][1] == 0 || ray_child_indices[2][1] == 1);

	assert(aabb_enterdist >= 0.0f);
//TEMP as failing too much	assert(aabb_exitdist >= aabb_enterdist);

	const REAL initial_closest_dist = ray_max_t * 1.0000001f;//needs to be bigger for test below
	REAL closest_dist = initial_closest_dist;
	assert(closest_dist > ray_max_t);

#ifdef VERBOSE_TRACE
	printVar(closest_dist);
	printVar(aabb_enterdist);
	printVar(aabb_exitdist);
#endif

	context.nodestack[0] = StackFrame(0, aabb_enterdist, aabb_exitdist);

	int stacktop = 0;//index of node on top of stack

	//closest_dist is a t_max that applies to *all* nodes, including ones just popped off the stack.
	
	while(stacktop >= 0)//!nodestack.empty())
	{
		//pop node off stack
		unsigned int current = context.nodestack[stacktop].node;
		assert(current < nodes.size());
		float tmin = context.nodestack[stacktop].tmin;
		float tmax = myMin(closest_dist, context.nodestack[stacktop].tmax);
		//float tmax = context.nodestack[stacktop].tmax;

		stacktop--;

#ifdef VERBOSE_TRACE
		conPrint("Popped node " + toString(current) + " off stack.");
		printVar(stacktop);
		printVar(tmin);
		printVar(tmax);
		printVar(closest_dist);
#endif

		//if we have hit a tri before the min_t of this child, then skip this subtree
		if(closest_dist <= tmin)
			continue;

		//TEMP:
		//if(closest_dist < tmax)
		//	tmax = closest_dist;

		//assert(tmax >= tmin);
		//if(tmin < tmax
		//TreeNode* current = frame.node;

		bool do_intersections = true;

		while(!nodes[current].isLeafNode())//while current node is not a leaf..
		{
#ifdef VERBOSE_TRACE
			conPrint("---Processing interior node " + toString(current) + "...---");
#endif
			//prefetch child node memory
#ifdef DO_PREFETCHING
			_mm_prefetch((const char *)(&nodes[nodes[current].getPosChildIndex()]), _MM_HINT_T0);	
#endif			
	
			const unsigned int splitting_axis = nodes[current].getSplittingAxis();
			assert(splitting_axis >= 0 && splitting_axis <= 2);
			const REAL t_split_0 = (nodes[current].clip[ray_child_indices[splitting_axis]] - ray.startPosF()[splitting_axis]) * recip_unitraydir[splitting_axis];//clip plane for near child
			const REAL t_split_1 = (nodes[current].clip[ray_child_indices[splitting_axis + 4]] - ray.startPosF()[splitting_axis]) * recip_unitraydir[splitting_axis];//clip plane for far child
			const unsigned int child_nodes[2] = {nodes[current].childIndex(), nodes[current].childIndex() + 1};

			const bool intersect_near = tmin <= t_split_0;
			const bool intersect_far = tmax >= t_split_1;

#ifdef VERBOSE_TRACE
			printVar(ray_child_indices[splitting_axis][0]);
			printVar(ray_child_indices[splitting_axis][1]);
			printVar(nodes[current].clip[ray_child_indices[splitting_axis][0]]);
			printVar(nodes[current].clip[ray_child_indices[splitting_axis][1]]);
			printVar(splitting_axis);
			printVar(tmin);
			printVar(t_split_0);
			printVar(tmax);
			printVar(t_split_1);
			printVar(intersect_near);
			printVar(intersect_far);
#endif

			if(intersect_near && intersect_far)
			{
				//ray hits plane - double recursion, into both near and far cells.
				const unsigned int nearnode = child_nodes[ray_child_indices[splitting_axis]];
				const unsigned int farnode = child_nodes[ray_child_indices[splitting_axis + 4]];
					
				//push far node onto stack to process later
				stacktop++;
				assert(stacktop < context.nodestack_size);
				//assert(t_split_1 >= tmin);
				context.nodestack[stacktop] = StackFrame(farnode, myMax(t_split_1, tmin), tmax);
					
				//process near child next
				current = nearnode;
				//assert(t_split_0 <= tmax);
				//tmax = t_split_0;
				tmax = myMin(t_split_0, tmax);
			}
			else if(intersect_near)
			{
				current = child_nodes[ray_child_indices[splitting_axis]];//nearnode;
				tmax = myMin(tmax, t_split_0);
			}
			else if(intersect_far)
			{
				current = child_nodes[ray_child_indices[splitting_axis + 4]];//farnode;
				tmin = myMax(tmin, t_split_1);
			}
			else
			{
				assert(tmin >= t_split_0 && tmax <= t_split_1);
				//neither
				//want to pop this node off the stack and continue the traversal
				do_intersections = false;
				break;
			}

#ifdef RECORD_TRACE_STATS
			this->total_num_nodes_touched += 1.;
#endif
		}//end while current node is not a leaf..

		//'current' is a leaf node..
	

#ifdef VERBOSE_TRACE
		conPrint("Processing leaf node " + toString(current) + "...");
#endif

		if(do_intersections)
		{
			assert(nodes[current].isLeafNode());

#ifdef RECORD_TRACE_STATS
			this->total_num_leafs_touched += 1.;
#endif
			//------------------------------------------------------------------------
			//intersect ray against leaf triangles
			//------------------------------------------------------------------------
			unsigned int leaf_geom_index = nodes[current].getLeafGeomIndex();
			const unsigned int num_leaf_tris = nodes[current].getNumLeafGeom();

#ifdef VERBOSE_TRACE
			conPrint("Doing leaf intersections...");
			printVar(leaf_geom_index);
			printVar(num_leaf_tris);
#endif

			for(unsigned int i=0; i<num_leaf_tris; ++i)
			{
#ifdef RECORD_TRACE_STATS
				this->total_num_tris_intersected += 1;
#endif
				assert(leaf_geom_index >= 0 && leaf_geom_index < leafgeom.size());

				float u, v, raydist;
				assert(leafgeom[leaf_geom_index] >= 0 && leafgeom[leaf_geom_index] < num_intersect_tris);
				if(intersect_tris[leafgeom[leaf_geom_index]].rayIntersect(ray, closest_dist, raydist, u, v))
				{
					assert(raydist < closest_dist);
#ifdef VERBOSE_TRACE
					conPrint("Ray hit triangle");
					printVar(raydist);
					printVar(leafgeom[leaf_geom_index]);
#endif

					closest_dist = raydist;
					hitinfo_out.hittriindex = leafgeom[leaf_geom_index];
					hitinfo_out.hittricoords.set(u, v);
				}
				++leaf_geom_index;
			}
		}
		
		/*if(closest_dist <= tmax)
		{
			//If intersection point lies before ray exit from this leaf volume, then finished.
			//return closest_dist;
		}*/
	}//end while !bundlenodestack.empty()

#ifdef RECORD_TRACE_STATS
//	if(num_traces % 10000 == 0)
	{
		printVar(num_traces);
		conPrint("av num nodes touched: " + toString(total_num_nodes_touched / num_traces));
		conPrint("av num leaves touched: " + toString(total_num_leafs_touched / num_traces));
		conPrint("av num tris tested: " + toString(total_num_tris_intersected / num_traces));
	}
#endif

	if(closest_dist < initial_closest_dist)
		return closest_dist;
	else
		return -1.0f;//missed all tris
}

const js::AABBox& BIHTree::getAABBoxWS() const
{
	return *root_aabb;
}






void BIHTree::getAllHits(const Ray& ray, js::TriTreePerThreadData& context, std::vector<FullHitInfo>& hitinfos_out) const
{
	assertSSEAligned(&ray);
	//assert(ray_max_t >= 0.0f);

	hitinfos_out.resize(0);

#ifdef RECORD_TRACE_STATS
	this->num_traces += 1.;
#endif

	//TEMP HACK:
	//return traceRayAgainstAllTris(ray, ray_max_t, hitinfo_out);

	//hitinfo_out.hittriindex = 0;
	//hitinfo_out.hittricoords.set(0.f, 0.f);

	////////////// load recip vector ///////////////
	const SSE_ALIGN PaddedVec3& recip_unitraydir = ray.getRecipRayDirF();

	assert(!nodes.empty());
	assert(root_aabb);
	if(nodes.empty())
		return;// -1.0f;

	float aabb_enterdist, aabb_exitdist;
	if(root_aabb->rayAABBTrace(ray.startPosF(), recip_unitraydir, aabb_enterdist, aabb_exitdist) == 0)
		return;// -1.0f;//missed aabbox

	const float MIN_TMIN = 0.00000001f;//above zero to avoid denorms
	
	aabb_enterdist = myMax(aabb_enterdist, MIN_TMIN);//TEMP

	//if(ray_max_t <= aabb_enterdist)//if this ray finishes before we even enter the aabb...
	//	return -1.0f;

	//unsigned int ray_child_indices[4][2];//first dimension is axis.
	//TreeUtils::buildRayChildIndices(ray, ray_child_indices);
	SSE_ALIGN unsigned int ray_child_indices[8];
	TreeUtils::buildFlatRayChildIndices(ray, ray_child_indices);

	assert(aabb_enterdist >= 0.0f);
//TEMP as failing too much	assert(aabb_exitdist >= aabb_enterdist);

	//const REAL initial_closest_dist = ray_max_t * 1.0000001f;//needs to be bigger for test below
	//REAL closest_dist = initial_closest_dist;
	//assert(closest_dist > ray_max_t);

	context.nodestack[0] = StackFrame(0, aabb_enterdist, aabb_exitdist);

	int stacktop = 0;//index of node on top of stack

	//closest_dist is a t_max that applies to *all* nodes, including ones just popped off the stack.
	
	while(stacktop >= 0)//!nodestack.empty())
	{
		//pop node off stack
		unsigned int current = context.nodestack[stacktop].node;
		assert(current < nodes.size());
		float tmin = context.nodestack[stacktop].tmin;
		float tmax = context.nodestack[stacktop].tmax;//myMin(closest_dist, context.nodestack[stacktop].tmax);
		//float tmax = context.nodestack[stacktop].tmax;

		stacktop--;

		//if we have hit a tri before the min_t of this child, then skip this subtree
		
		//NOTE: removing this is the only change so far for getAllHits()
		//if(closest_dist <= tmin)
		//	continue;

		//TEMP:
		//if(closest_dist < tmax)
		//	tmax = closest_dist;

		//assert(tmax >= tmin);
		//if(tmin < tmax
		//TreeNode* current = frame.node;

		bool do_intersections = true;

		while(!nodes[current].isLeafNode())//while current node is not a leaf..
		{
			//prefetch child node memory
#ifdef DO_PREFETCHING
			_mm_prefetch((const char *)(&nodes[nodes[current].getPosChildIndex()]), _MM_HINT_T0);	
#endif			
	
			const unsigned int splitting_axis = nodes[current].getSplittingAxis();
			assert(splitting_axis >= 0 && splitting_axis <= 2);
			const REAL t_split_0 = (nodes[current].clip[ray_child_indices[splitting_axis]] - ray.startPosF()[splitting_axis]) * recip_unitraydir[splitting_axis];//clip plane for near child
			const REAL t_split_1 = (nodes[current].clip[ray_child_indices[splitting_axis + 4]] - ray.startPosF()[splitting_axis]) * recip_unitraydir[splitting_axis];//clip plane for far child
			const unsigned int child_nodes[2] = {nodes[current].childIndex(), nodes[current].childIndex() + 1};

			const bool intersect_near = tmin <= t_split_0;
			const bool intersect_far = tmax >= t_split_1;

			if(intersect_near && intersect_far)
			{
				//ray hits plane - double recursion, into both near and far cells.
				const unsigned int nearnode = child_nodes[ray_child_indices[splitting_axis]];
				const unsigned int farnode = child_nodes[ray_child_indices[splitting_axis + 4]];
					
				//push far node onto stack to process later
				stacktop++;
				assert(stacktop < context.nodestack_size);
				//assert(t_split_1 >= tmin);
				context.nodestack[stacktop] = StackFrame(farnode, myMax(t_split_1, tmin), tmax);
					
				//process near child next
				current = nearnode;
				//assert(t_split_0 <= tmax);
				//tmax = t_split_0;
				tmax = myMin(t_split_0, tmax);
			}
			else if(intersect_near)
			{
				current = child_nodes[ray_child_indices[splitting_axis]];//nearnode;
				tmax = myMin(tmax, t_split_0);
			}
			else if(intersect_far)
			{
				current = child_nodes[ray_child_indices[splitting_axis + 4]];//farnode;
				tmin = myMax(tmin, t_split_1);
			}
			else
			{
				assert(tmin >= t_split_0 && tmax <= t_split_1);
				//neither
				//want to pop this node off the stack and continue the traversal
				do_intersections = false;
				break;
			}

#ifdef RECORD_TRACE_STATS
			this->total_num_nodes_touched += 1.;
#endif
		}//end while current node is not a leaf..

		//'current' is a leaf node..

		if(do_intersections)
		{
#ifdef RECORD_TRACE_STATS
			this->total_num_leafs_touched += 1.;
#endif
			//------------------------------------------------------------------------
			//intersect ray against leaf triangles
			//------------------------------------------------------------------------
			unsigned int leaf_geom_index = nodes[current].getLeafGeomIndex();
			const unsigned int num_leaf_tris = nodes[current].getNumLeafGeom();

			for(unsigned int i=0; i<num_leaf_tris; ++i)
			{
#ifdef RECORD_TRACE_STATS
				this->total_num_tris_intersected += 1;
#endif
				assert(leaf_geom_index >= 0 && leaf_geom_index < leafgeom.size());

				float u, v, raydist;
				assert(leafgeom[leaf_geom_index] >= 0 && leafgeom[leaf_geom_index] < num_intersect_tris);
				if(intersect_tris[leafgeom[leaf_geom_index]].rayIntersect(ray, aabb_exitdist/*closest_dist*/, raydist, u, v))
				{
					/*assert(raydist < closest_dist);

					closest_dist = raydist;
					hitinfo_out.hittriindex = leafgeom[leaf_geom_index];
					hitinfo_out.hittricoords.set(u, v);*/
					bool already_added = false;
					for(unsigned int z=0; z<hitinfos_out.size(); ++z)
						if(hitinfos_out[z].hittri_index == leafgeom[leaf_geom_index])
							already_added = true;
					
					if(!already_added)
					{
						hitinfos_out.push_back(FullHitInfo());
						hitinfos_out.back().hittri_index = leafgeom[leaf_geom_index];
						hitinfos_out.back().tri_coords.set(u, v);
						hitinfos_out.back().dist = raydist;
						hitinfos_out.back().hitpos = ray.startPos();
						hitinfos_out.back().hitpos.addMult(ray.unitDir(), raydist);
					}
				}
				++leaf_geom_index;
			}
		}
		
		/*if(closest_dist <= tmax)
		{
			//If intersection point lies before ray exit from this leaf volume, then finished.
			//return closest_dist;
		}*/
	}//end while !bundlenodestack.empty()

#ifdef RECORD_TRACE_STATS
	printVar(num_traces);
	conPrint("av num nodes touched: " + toString(total_num_nodes_touched / num_traces));
	conPrint("av num leaves touched: " + toString(total_num_leafs_touched / num_traces));
	conPrint("av num tris tested: " + toString(total_num_tris_intersected / num_traces));
#endif

	/*if(closest_dist < initial_closest_dist)
		return closest_dist;
	else
		return -1.0f;//missed all tris*/
}
bool BIHTree::doesFiniteRayHit(const ::Ray& ray, double raylength, js::TriTreePerThreadData& context) const
{
	//NOTE: can speed this up
	HitInfo hitinfo;
	const float dist = traceRay(ray, raylength, context, hitinfo);
	return dist >= 0.0f && dist < raylength;
}

	//returns dist till hit tri, neg number if missed.
double BIHTree::traceRayAgainstAllTris(const ::Ray& ray, double tmax, HitInfo& hitinfo_out) const
{
	hitinfo_out.hittriindex = 0;
	hitinfo_out.hittricoords.set(0.f, 0.f);
	double closest_dist = std::numeric_limits<double>::max();//2e9f;//closest hit so far, also upper bound on hit dist

	for(uint32 i=0; i<num_intersect_tris; ++i)
	{
		float u, v, raydist;
		//raydist is distance until hit if hit occurred
		if(intersect_tris[i].rayIntersect(ray, closest_dist, raydist, u, v))
		{			
			assert(raydist < closest_dist);

			closest_dist = raydist;
			hitinfo_out.hittriindex = i;
			hitinfo_out.hittricoords.set(u, v);
		}
	}

	if(closest_dist > tmax)
		closest_dist = -1.0f;

	return closest_dist;
}


bool BIHTree::diskCachable()
{
	return false;
}
void BIHTree::buildFromStream(std::istream& stream)
{
	assert(false);
	throw TreeExcep("BIH can't save to disk.");
}
void BIHTree::saveTree(std::ostream& stream)
{
	assert(false);
}

unsigned int BIHTree::checksum()
{
	assert(false);
	return 0;
}

const Vec3f& BIHTree::triGeometricNormal(unsigned int tri_index) const //slow
{
	return intersect_tris[tri_index].getNormal();
}



} //end namespace js






