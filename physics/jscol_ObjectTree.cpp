/*=====================================================================
ObjectTree.cpp
--------------
File created by ClassTemplate on Sat Apr 15 18:59:20 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "jscol_ObjectTree.h"

#include "../indigo/FullHitInfo.h"
#include "../simpleraytracer/ray.h"
#include "../raytracing/hitinfo.h"
#include <algorithm>
#include "jscol_ObjectTreePerThreadData.h"
#include <limits>
#include "TreeUtils.h"

namespace js
{

//#define OBJECTTREE_VERBOSE 1


ObjectTree::ObjectTree()
{
	root_aabb = NULL;
	nodestack_size = 0;

	root_aabb = (js::AABBox*)alignedMalloc(sizeof(AABBox), sizeof(AABBox));
	new(root_aabb) AABBox(Vec3f(0,0,0), Vec3f(0,0,0));
	assert(::isAlignedTo(root_aabb, sizeof(AABBox)));

	nodes.push_back(ObjectTreeNode());//root node
	assert((uint64)&nodes[0] % 8 == 0);//assert aligned

	num_inseparable_tri_leafs = 0;
	num_maxdepth_leafs = 0;
	num_under_thresh_leafs = 0;
}


ObjectTree::~ObjectTree()
{
	alignedSSEFree(root_aabb);

//	delete rootnode;

	//alignedSSEFree(nodestack);
	//nodestack = NULL;
}

void ObjectTree::insertObject(INTERSECTABLE_TYPE* intersectable)
{
	intersectable->setObjectIndex((int)objects.size());
	objects.push_back(intersectable);
	//intersectable->object_index = (int)objects.size() - 1;
}



//returns dist till hit tri, neg number if missed.
double ObjectTree::traceRay(const Ray& ray, 
						   ThreadContext& thread_context, 
						   js::ObjectTreePerThreadData& object_context, 
						   const INTERSECTABLE_TYPE*& hitob_out, HitInfo& hitinfo_out) const
{
	if(object_context.last_test_time.size() < objects.size())
		object_context.last_test_time.resize(objects.size());

#ifdef OBJECTTREE_VERBOSE
	conPrint("-------------------------ObjectTree::traceRay()-----------------------------");
#endif
	assertSSEAligned(&ray);
	assert(ray.unitDir().isUnitLength());

	object_context.time++;

#ifdef OBJECTTREE_VERBOSE
	printVar(object_context.time);
#endif

	hitob_out = NULL;
	hitinfo_out.hittriindex = 0;
	hitinfo_out.hittricoords.set(0.0f, 0.0f);

	HitInfo ob_hit_info;

	if(leafgeom.empty())
		return -1.0;

	assert(nodestack_size > 0);
	assert(!nodes.empty());
	assert(root_aabb);

	float aabb_enterdist, aabb_exitdist;
	if(root_aabb->rayAABBTrace(ray.startPosF(), ray.getRecipRayDirF(), aabb_enterdist, aabb_exitdist) == 0)
		return -1.0;//missed aabbox


	SSE_ALIGN unsigned int ray_child_indices[8];
	TreeUtils::buildFlatRayChildIndices(ray, ray_child_indices);

	//const float MIN_TMIN = 0.00000001f;//above zero to avoid denorms
	//TEMP:
	//aabb_enterdist = myMax(aabb_enterdist, MIN_TMIN);

	aabb_enterdist = myMax(aabb_enterdist, 0.f);

	assert(aabb_enterdist >= 0.0f);
	assert(aabb_exitdist >= aabb_enterdist);

#ifdef OBJECTTREE_VERBOSE
	printVar(aabb_enterdist);
	printVar(aabb_exitdist);
#endif

	double closest_dist = std::numeric_limits<double>::max(); //closest hit on a tri so far.
	object_context.nodestack[0] = StackFrame(0, aabb_enterdist, aabb_exitdist);

	int stacktop = 0;//index of node on top of stack
	
	while(stacktop >= 0)//!nodestack.empty())
	{
		//pop node off stack
		unsigned int current = object_context.nodestack[stacktop].node;
		float tmin = object_context.nodestack[stacktop].tmin;
		float tmax = object_context.nodestack[stacktop].tmax;

		stacktop--;

		while(nodes[current].getNodeType() != ObjectTreeNode::NODE_TYPE_LEAF)//!nodes[current].isLeafNode())
		{
			//------------------------------------------------------------------------
			//prefetch child node memory
			//------------------------------------------------------------------------
#ifdef DO_PREFETCHING
			_mm_prefetch((const char *)(&nodes[nodes[current].getPosChildIndex()]), _MM_HINT_T0);	
#endif			
			//while current node is not a leaf..
	
			const unsigned int splitting_axis = nodes[current].getSplittingAxis();
			const REAL t_split = (nodes[current].data2.dividing_val - ray.startPosF()[splitting_axis]) * ray.getRecipRayDirF()[splitting_axis];	
			const unsigned int child_nodes[2] = {current + 1, nodes[current].getPosChildIndex()};

			if(t_split > tmax)//or ray segment ends b4 split
			{
				//whole interval is on near cell	
				current = child_nodes[ray_child_indices[splitting_axis]];
			}
			else if(tmin > t_split)//else if ray seg starts after split
			{
				//whole interval is on far cell.
				current = child_nodes[ray_child_indices[splitting_axis + 4]];//farnode;
			}
			else
			{
				//ray hits plane - double recursion, into both near and far cells.
				const unsigned int nearnode = child_nodes[ray_child_indices[splitting_axis]];
				const unsigned int farnode = child_nodes[ray_child_indices[splitting_axis + 4]];
					
				//push far node onto stack to process later
				stacktop++;
				assert(stacktop < nodestack_size);
				object_context.nodestack[stacktop] = StackFrame(farnode, t_split, tmax);
					
				//process near child next
				current = nearnode;
				tmax = t_split;
			}

		}//end while current node is not a leaf..

		//'current' is a leaf node..

		unsigned int leaf_geom_index = nodes[current].getLeafGeomIndex();
		const unsigned int num_leaf_geom = nodes[current].getNumLeafGeom();
#ifdef OBJECTTREE_VERBOSE
		printVar(leaf_geom_index);
		printVar(num_leaf_geom);
#endif
		for(unsigned int i=0; i<num_leaf_geom; ++i)
		{
			assert(leaf_geom_index >= 0 && leaf_geom_index < leafgeom.size());
	
			const INTERSECTABLE_TYPE* ob = leafgeom[leaf_geom_index];//get pointer to intersectable

			assert(ob->getObjectIndex() >= 0 && ob->getObjectIndex() < (int)object_context.last_test_time.size());
#ifdef OBJECTTREE_VERBOSE
			conPrint("considering intersection with object '" + ob->debugName() + "', object_index=" + toString(ob->object_index));
#endif

			if(object_context.last_test_time[ob->getObjectIndex()] != object_context.time) //If this object has not already been intersected against during this traversal
			{
				//assert(object_context.object_context != NULL);
				const double dist = ob->traceRay(
					ray, 
					closest_dist,
					thread_context, 
					object_context.object_context ? *object_context.object_context : object_context, 
					ob_hit_info
					);
#ifdef OBJECTTREE_VERBOSE
				conPrint("Ran intersection, dist=" + toString(dist));
#endif
				if(dist >= 0.0 && dist < closest_dist)
				{
					closest_dist = dist;
					hitob_out = ob;
					hitinfo_out = ob_hit_info;
				}

				object_context.last_test_time[ob->getObjectIndex()] = object_context.time;
			}
			leaf_geom_index++;
		}

		if(closest_dist <= tmax)
		{
			//If intersection point lies before ray exit from this leaf volume, then finished.
			return closest_dist;
		}

	}

	return hitob_out ? closest_dist : -1.0;
}


//for debugging
bool ObjectTree::allObjectsDoesFiniteRayHitAnything(const Ray& ray, double length, 
													ThreadContext& thread_context,
													js::ObjectTreePerThreadData& object_context) const
{
	const INTERSECTABLE_TYPE* hitob = NULL;
	HitInfo hitinfo;
	const double dist = this->traceRayAgainstAllObjects(ray, thread_context, object_context, hitob, hitinfo);
	return dist >= 0.0 && dist < length;
}

bool ObjectTree::doesFiniteRayHit(const Ray& ray, double raylength, 
								  ThreadContext& thread_context, 
								  js::ObjectTreePerThreadData& object_context) const
{
#ifdef OBJECTTREE_VERBOSE
	conPrint("-------------------------ObjectTree::doesFiniteRayHitAnything()-----------------------------");
#endif
	if(object_context.last_test_time.size() < objects.size())
		object_context.last_test_time.resize(objects.size());

	assertSSEAligned(&ray);
	assert(ray.unitDir().isUnitLength());
	assert(raylength > 0.0);
	
	object_context.time++;

#ifdef OBJECTTREE_VERBOSE
	printVar(object_context.time);
#endif

	if(leafgeom.empty())
		return false;

	assert(nodestack_size > 0);
	assert(!nodes.empty());
	assert(root_aabb);

	float aabb_enterdist, aabb_exitdist;
	if(root_aabb->rayAABBTrace(ray.startPosF(), ray.getRecipRayDirF(), aabb_enterdist, aabb_exitdist) == 0)
		return false;//missed aabbox

	/*const float MIN_TMIN = 0.00000001f;//above zero to avoid denorms
	//TEMP:
	aabb_enterdist = myMax(aabb_enterdist, MIN_TMIN);

	assert(aabb_enterdist >= 0.0f);
	assert(aabb_exitdist >= aabb_enterdist);

	if(aabb_enterdist > raylength)
		return false;
	aabb_exitdist = myMin(aabb_exitdist, (float)raylength);*/

	const float root_t_min = myMax(0.0f, aabb_enterdist);
	const float root_t_max = myMin((float)raylength, aabb_exitdist);
	if(root_t_min > root_t_max)
		return false; // Ray interval does not intersect AABB

	SSE_ALIGN unsigned int ray_child_indices[8];
	TreeUtils::buildFlatRayChildIndices(ray, ray_child_indices);

	object_context.nodestack[0] = StackFrame(0, root_t_min, root_t_max);

	int stacktop = 0;//index of node on top of stack
	
	while(stacktop >= 0)//!nodestack.empty())
	{
		//pop node off stack
		unsigned int current = object_context.nodestack[stacktop].node;
		float tmin = object_context.nodestack[stacktop].tmin;
		float tmax = object_context.nodestack[stacktop].tmax;

		stacktop--;

		while(nodes[current].getNodeType() != ObjectTreeNode::NODE_TYPE_LEAF)//!nodes[current].isLeafNode())
		{
			//------------------------------------------------------------------------
			//prefetch child node memory
			//------------------------------------------------------------------------
#ifdef DO_PREFETCHING
			_mm_prefetch((const char *)(&nodes[nodes[current].getPosChildIndex()]), _MM_HINT_T0);	
#endif			
			//while current node is not a leaf..
	

			const unsigned int splitting_axis = nodes[current].getSplittingAxis();
			const REAL t_split = (nodes[current].data2.dividing_val - ray.startPosF()[splitting_axis]) * ray.getRecipRayDirF()[splitting_axis];	
			const unsigned int child_nodes[2] = {current + 1, nodes[current].getPosChildIndex()};

			if(t_split > tmax)//or ray segment ends b4 split
			{
				//whole interval is on near cell	
				current = child_nodes[ray_child_indices[splitting_axis]];
			}
			else if(tmin > t_split)//else if ray seg starts after split
			{
				//whole interval is on far cell.
				current = child_nodes[ray_child_indices[splitting_axis + 4]];//farnode;
			}
			else
			{
				//ray hits plane - double recursion, into both near and far cells.
				const unsigned int nearnode = child_nodes[ray_child_indices[splitting_axis]];
				const unsigned int farnode = child_nodes[ray_child_indices[splitting_axis + 4]];
					
				//push far node onto stack to process later
				stacktop++;
				assert(stacktop < nodestack_size);
				object_context.nodestack[stacktop] = StackFrame(farnode, t_split, tmax);
					
				//process near child next
				current = nearnode;
				tmax = t_split;
			}

		}//end while current node is not a leaf..

		//'current' is a leaf node..

		unsigned int leaf_geom_index = nodes[current].getLeafGeomIndex();
		const unsigned int num_leaf_geom = nodes[current].getNumLeafGeom();
#ifdef OBJECTTREE_VERBOSE
		printVar(leaf_geom_index);
		printVar(num_leaf_geom);
#endif
		for(unsigned int i=0; i<num_leaf_geom; ++i)
		{
			assert(leaf_geom_index >= 0 && leaf_geom_index < leafgeom.size());
			INTERSECTABLE_TYPE* ob = leafgeom[leaf_geom_index];
#ifdef OBJECTTREE_VERBOSE
			conPrint("considering intersection with object '" + ob->debugName() + "', object_index=" + toString(ob->object_index));
#endif
			assert(ob->getObjectIndex() >= 0 && ob->getObjectIndex() < (int)object_context.last_test_time.size());
			if(object_context.last_test_time[ob->getObjectIndex()] != object_context.time)
			{
#ifdef OBJECTTREE_VERBOSE
				conPrint("Intersecting with object...");
				conPrint(ob->doesFiniteRayHit(ray, raylength, tritree_context) ? "\tHIT" : "\tMISSED");
#endif
				//assert(object_context.object_context != NULL);
				if(ob->doesFiniteRayHit(ray, raylength, thread_context, *object_context.object_context))
					return true;
				object_context.last_test_time[ob->getObjectIndex()] = object_context.time;
			}
			leaf_geom_index++;
		}
	}//end while !bundlenodestack.empty()
	return false;
}



void obTreeDebugPrint(const std::string& s)
{
	conPrint("\t" + s);
}





void ObjectTree::build()
{
	conPrint("Building Object Tree...");
	obTreeDebugPrint(::toString((int)objects.size()) + " objects.");

	if(objects.size() == 0)
	{
		//assert(0);
		return;
	}

	//------------------------------------------------------------------------
	//calc root node's aabbox
	//------------------------------------------------------------------------
	obTreeDebugPrint("calcing root AABB.");
	{
	
	*root_aabb = objects[0]->getAABBoxWS();

	for(unsigned int i=0; i<objects.size(); ++i)
		root_aabb->enlargeToHoldAABBox(objects[i]->getAABBoxWS());
	}

	obTreeDebugPrint("AABB: (" + ::toString(root_aabb->min_.x) + ", " + ::toString(root_aabb->min_.y) + ", " + ::toString(root_aabb->min_.z) + "), " + 
						"(" + ::toString(root_aabb->max_.x) + ", " + ::toString(root_aabb->max_.y) + ", " + ::toString(root_aabb->max_.z) + ")"); 
							
	assert(root_aabb->invariant());

	//------------------------------------------------------------------------
	//compute max allowable depth
	//------------------------------------------------------------------------
	const int numtris = (int)objects.size();
	max_depth = (int)(2.0 + logBase2((double)numtris) * 2.0);

	obTreeDebugPrint("max tree depth: " + ::toString(max_depth));

	// Alloc stack vector
	nodestack_size = max_depth + 1;
	//alignedSSEArrayMalloc(nodestack_size, nodestack);
	
	const int expected_numnodes = (int)((float)numtris * 1.0);
	const int nodemem = expected_numnodes * sizeof(js::ObjectTreeNode);

	obTreeDebugPrint("reserving N nodes: " + ::toString(expected_numnodes) + "(" 
		+ ::getNiceByteSize(nodemem) + ")");

	//------------------------------------------------------------------------
	//get array of tri indices for root node
	//------------------------------------------------------------------------
	std::vector<INTERSECTABLE_TYPE*> rootobs = objects;

	//const int tris_size = tris->size();

	//triTreeDebugPrint("calling doBuild()...");
	doBuild(0, rootobs, 0, max_depth, *root_aabb);
	//triTreeDebugPrint("doBuild() done.");

	if(!nodes.empty())
	{
		assert(isAlignedTo(&nodes[0], sizeof(js::ObjectTreeNode)));
	}

	const uint64 numnodes = nodes.size();
	const uint64 leafgeomsize = leafgeom.size();

	obTreeDebugPrint("total nodes used: " + ::toString(numnodes) + " (" + 
		::getNiceByteSize((int)numnodes * sizeof(js::ObjectTreeNode)) + ")");

	obTreeDebugPrint("total leafgeom size: " + ::toString(leafgeomsize) + " (" + 
		::getNiceByteSize((int)leafgeomsize * sizeof(INTERSECTABLE_TYPE*)) + ")");

	conPrint("Finished building tree.");
}





void ObjectTree::doBuild(int cur, //index of current node getting built
						const std::vector<INTERSECTABLE_TYPE*>& nodeobjs,//objects for current node
						int depth, //depth of current node
						int maxdepth, //max permissible depth
						const AABBox& cur_aabb)//AABB of current node
{
	assert(cur >= 0 && cur < (int)nodes.size());
	
	if(nodeobjs.size() == 0)
	{
		//no tris were allocated to this node.
		//so make it a leaf node with 0 tris
		nodes[cur] = ObjectTreeNode(
			0,
			0
			);
		return;
	}
	//------------------------------------------------------------------------
	//test for termination of splitting
	//------------------------------------------------------------------------
	const int SPLIT_THRESHOLD = 2;

	if(nodeobjs.size() <= SPLIT_THRESHOLD || depth >= maxdepth)
	{
		nodes[cur] = ObjectTreeNode(
			(uint32)leafgeom.size(),
			(uint32)nodeobjs.size()
			);

		for(unsigned int i=0; i<nodeobjs.size(); ++i)
			leafgeom.push_back(nodeobjs[i]);

		if(depth >= maxdepth)
			num_maxdepth_leafs++;
		else
			num_under_thresh_leafs++;

		return;
	}

	float smallest_cost = std::numeric_limits<float>::max();
	int best_axis = -1;
	float best_div_val = -666.0f;
	const float traversal_cost = 1.0f;
	const float intersection_cost = 10.0f;
	const float recip_aabb_surf_area = (1.0f / cur_aabb.getSurfaceArea());
	int best_num_in_neg = 0;
	int best_num_in_pos = 0;

	//------------------------------------------------------------------------
	//compute non-split cost
	//------------------------------------------------------------------------
	const unsigned int numtris = (unsigned int)nodeobjs.size();

	const unsigned int nonsplit_axes[3][2] = {{1, 2}, {0, 2}, {0, 1}};

	for(int axis=0; axis<3; ++axis)
	{
		const int axis1 = nonsplit_axes[axis][0];
		const int axis2 = nonsplit_axes[axis][1];
		//const float two_cap_area = (cur_aabb.max[axis1] - cur_aabb.min[axis1]) * (cur_aabb.max[axis2] - cur_aabb.min[axis2]) * 2.0f;
		//const float circum = ((cur_aabb.max[axis1] - cur_aabb.min[axis1]) + (cur_aabb.max[axis2] - cur_aabb.min[axis2])) * 2.0f;
		const float two_cap_area = cur_aabb.axisLength(axis1) * cur_aabb.axisLength(axis2) * 2.0f;
		const float circum = (cur_aabb.axisLength(axis1) + cur_aabb.axisLength(axis2)) * 2.0f;
		
		//------------------------------------------------------------------------
		//Sort lower and upper tri AABB bounds into ascending order
		//------------------------------------------------------------------------
		std::vector<float> lower(numtris);
		std::vector<float> upper(numtris);

		for(unsigned int i=0; i<numtris; ++i)
		{
			lower[i] = nodeobjs[i]->getAABBoxWS().min_[axis];//tri_boxes[nodetris[i]].min[axis];
			upper[i] = nodeobjs[i]->getAABBoxWS().max_[axis];//tri_boxes[nodetris[i]].max[axis];

			if(upper[i] == lower[i])
			{
				//tri lies totally in the plane perpendicular to the current axis.
				//So nudge up its upper bound a bit to force it into the positive volume.
				float upperval = upper[i];
				upper[i] += 0.000001f;
				upperval = upper[i];
				assert(upper[i] > lower[i]);
			}
		}

		std::sort(lower.begin(), lower.end());
		std::sort(upper.begin(), upper.end());

		//Tris in contact with splitting plane will not imply tri is in either volume, except
		//for tris lying totally on splitting plane which is considered to be in positive volume.

		unsigned int upper_index = 0;//Index of first tri with upper bound > then current split val.
		//Also the number of tris with upper bound <= current split val.
		//which is the number of tris *not* in pos volume

		float last_splitval = -1e20f;
		for(unsigned int i=0; i<numtris; ++i)
		{
			const float splitval = lower[i];

			if(splitval != last_splitval)//only consider first tri seen with a given lower bound.
			{
				if(splitval > cur_aabb.min_[axis] && splitval < cur_aabb.max_[axis])
				{
					//advance upper index to maintain invariant above
					while(upper_index < numtris && upper[upper_index] <= splitval)
					{
						//const float upperval = upper[upper_index];
						upper_index++;
					}

					assert(upper_index == numtris || upper[upper_index] > splitval);

					const int num_in_neg = i;//upper_index;
					const int num_in_pos = numtris - upper_index;//numtris - i;
					assert(num_in_neg >= 0 && num_in_neg <= (int)numtris);
					assert(num_in_pos >= 0 && num_in_pos <= (int)numtris);
					assert(num_in_neg + num_in_pos >= (int)numtris);
					//if(num_in_neg + num_in_pos < (int)numtris)
					//	num_in_pos = 
					const float negchild_surface_area = two_cap_area + (splitval - cur_aabb.min_[axis]) * circum;
					const float poschild_surface_area = two_cap_area + (cur_aabb.max_[axis] - splitval) * circum;

					const float cost = traversal_cost + 
						((float)num_in_neg * negchild_surface_area + 
						(float)num_in_pos * poschild_surface_area) * 
						recip_aabb_surf_area * intersection_cost;

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
			//if tri i and tri i+1 share upper bounds, advance to largest index of tris sharing same upper bound
			while(i<numtris-1 && upper[i] == upper[i+1])
				++i;

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
					const int num_in_pos = numtris - (i + 1);//numtris - lower_index;
					assert(num_in_neg >= 0 && num_in_neg <= (int)numtris);
					assert(num_in_pos >= 0 && num_in_pos <= (int)numtris);
					assert(num_in_neg + num_in_pos >= (int)numtris);
					const float negchild_surface_area = two_cap_area + (splitval - cur_aabb.min_[axis]) * circum;
					const float poschild_surface_area = two_cap_area + (cur_aabb.max_[axis] - splitval) * circum;

					const float cost = traversal_cost + 
						((float)num_in_neg * negchild_surface_area + 
						(float)num_in_pos * poschild_surface_area) * 
						recip_aabb_surf_area * intersection_cost;

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
		nodes[cur] = ObjectTreeNode(
			(uint32)leafgeom.size(),
			(uint32)nodeobjs.size()
			);

		for(unsigned int i=0; i<nodeobjs.size(); ++i)
			leafgeom.push_back(nodeobjs[i]);

		num_inseparable_tri_leafs++;
		return;
	}
	
	assert(best_axis >= 0 && best_axis <= 2);
	assert(best_div_val != -666.0f);

	assert(best_div_val >= cur_aabb.min_[best_axis]);
	assert(best_div_val <= cur_aabb.max_[best_axis]);

	//------------------------------------------------------------------------
	//compute AABBs of child nodes
	//------------------------------------------------------------------------
	assert(best_axis >= 0 && best_axis <= 2);

	AABBox negbox(cur_aabb.min_, cur_aabb.max_);
	negbox.max_[best_axis] = best_div_val;

	AABBox posbox(cur_aabb.min_, cur_aabb.max_);
	posbox.min_[best_axis] = best_div_val;

	//------------------------------------------------------------------------
	//assign tris to neg and pos child node bins
	//------------------------------------------------------------------------
	std::vector<INTERSECTABLE_TYPE*> neg_objs;
	neg_objs.reserve(best_num_in_neg);//nodeobjs.size() / 2);
	std::vector<INTERSECTABLE_TYPE*> pos_objs;
	pos_objs.reserve(best_num_in_pos);//nodeobjs.size() / 2);

	for(unsigned int i=0; i<nodeobjs.size(); ++i)//for each tri
	{
		bool inserted = false;
		if(intersectableIntersectsAABB(nodeobjs[i], negbox, nodes[cur].getSplittingAxis(), true))
		//if(triIntersectsAABB(nodetris[i], negbox, nodes[cur].getSplittingAxis(), true))
		{
			neg_objs.push_back(nodeobjs[i]);
			inserted = true;
		}

		if(intersectableIntersectsAABB(nodeobjs[i], posbox, nodes[cur].getSplittingAxis(), false))
		//if(triIntersectsAABB(nodetris[i], posbox, nodes[cur].getSplittingAxis(), false))
		{
			pos_objs.push_back(nodeobjs[i]);
			inserted = true;
		}

//		assert(inserted);	//check we haven't 'lost' any tris
	}

	const int num_in_neg = (int)neg_objs.size();
	const int num_in_pos = (int)pos_objs.size();
	const int num_tris_in_both = (int)neg_objs.size() + (int)pos_objs.size() - numtris;

	//assert(num_in_neg == best_num_in_neg);
	//assert(num_in_pos == best_num_in_pos);

	//::triTreeDebugPrint("split " + ::toString(nodetris.size()) + " tris: left: " + ::toString(neg_tris.size())
	//	+ ", right: " + ::toString(pos_tris.size()) + ", both: " + ::toString(num_tris_in_both) );

	if(neg_objs.size() == nodeobjs.size() && pos_objs.size() == nodeobjs.size())
	{
		//If we were unable to get a reduction in the number of tris in either of the children,
		//then splitting is pointless.  So make this a leaf node.
		nodes[cur] = ObjectTreeNode(
			(uint32)leafgeom.size(),
			(uint32)nodeobjs.size()
			);

		for(unsigned int i=0; i<nodeobjs.size(); ++i)
			leafgeom.push_back(nodeobjs[i]);
		return;
	}

	//------------------------------------------------------------------------
	//create negative child node, next in the array.
	//------------------------------------------------------------------------
	nodes.push_back(ObjectTreeNode());
	const int negchild_index = (int)nodes.size() - 1;

	//build left subtree, recursive call.
	doBuild(negchild_index, neg_objs, depth + 1, maxdepth, negbox);

	//------------------------------------------------------------------------
	//create positive child
	//------------------------------------------------------------------------
	nodes.push_back(ObjectTreeNode());
	
	// Set details for current node
	//nodes[cur].setPosChildIndex((int)nodes.size() - 1);//positive_child = new TreeNode(depth + 1);
	nodes[cur] = ObjectTreeNode(
		best_axis, // splitting axis
		best_div_val, // split
		(int)nodes.size() - 1 // right child node index
		);

	//build right subtree
	doBuild(nodes[cur].getPosChildIndex(), pos_objs, depth + 1, maxdepth, posbox);
}




bool ObjectTree::intersectableIntersectsAABB(INTERSECTABLE_TYPE* ob, const AABBox& aabb, int split_axis,
								bool is_neg_child)
{
	const AABBox& ob_aabb = ob->getAABBoxWS();

	//------------------------------------------------------------------------
	//test each separating axis
	//------------------------------------------------------------------------
	for(int i=0; i<3; ++i)//for each axis
	{
		if(i == split_axis)
		{
			//ONLY tris hitting the MIN plane are considered in AABB
			/*if(trimax[i] < aabb.min[i])//if less than min
				return false;//not in box
			
			if(trimin[i] >= aabb.max[i])//if tris is touching max in this axis
			{
				if(aabb.min[i] == aabb.max[i])//must hit the zero width box
				{
				}
				else
					return false;//not in box
			}*/
			if(is_neg_child)
			{
				//TEMP: pos side gets tris on split

				//then this box gets tris which touch the split, ie which hit
				//max[split_axis]

				if(ob_aabb.max_[i] < aabb.min_[i])
					return false;

				if(ob_aabb.min_[i] >= aabb.max_[i])
					return false;
			}
			else
			{
				//then this box gets everything that touches it, EXCEPT the
				//tris which hit the split. (ie hit min[split_axis])
				//if(tri_boxes[triindex].max[i] <= aabb.min[i])//reject even if touching
				//	return false;
				if(ob_aabb.max_[i] <= aabb.min_[i])
				{
					if(!(ob_aabb.max_[i] == ob_aabb.min_[i] &&
						ob_aabb.max_[i] == aabb.min_[i]))//if tri doesn't lie entirely on splitting plane
					return false;
				}

				if(ob_aabb.min_[i] > aabb.max_[i])
					return false;
			}
		}
		else
		{
			//tris touching min or max planes are considered in aabb	
			if(ob_aabb.max_[i] < aabb.min_[i])
				return false;
			
			if(ob_aabb.min_[i] > aabb.max_[i])
				return false;
		}
	}

	return true;
}

/*
void ObjectTree::writeTreeModel(std::ostream& stream)
{
	stream << "\t<mesh>\n";
	stream << "\t\t<name>aabb_mesh</name>\n";
	stream << "\t\t<embedded>\n";


	int num_verts = 0;
	doWriteModel(0, *root_aabb, stream, num_verts);

	stream << "\t\t</embedded>\n";
	stream << "\t</mesh>\n";

	stream << "\t<model>\n";
	stream << "\t\t<pos>0 0 0</pos>\n";
	stream << "\t\t<scale>1.0</scale>\n";
	stream << "\t\t<rotation>\n";
	stream << "\t\t\t<matrix>\n";
	stream << "\t\t\t\t1 0 0 0 1 0 0 0 1\n";
	stream << "\t\t\t</matrix>\n";
	stream << "\t\t</rotation>\n";
	stream << "\t\t<mesh_name>aabb_mesh</mesh_name>\n";
	stream << "\t</model>\n";
}
*/

/*
void ObjectTree::doWriteModel(int currentnode, const AABBox& node_aabb, std::ostream& stream, 
							  int& num_verts) const
{
	const TreeNode& node = nodes[currentnode];

	if(node.isLeafNode() && node.getNumLeafGeom() > 0)
		node_aabb.writeModel(stream, num_verts);//writeAABBModel(node_aabb, stream, num_verts);
	else
	{
		AABBox min_aabb = node_aabb;
		min_aabb.max_[node.getSplittingAxis()] = node.data2.dividing_val;

		//recurse to neg child
		doWriteModel(currentnode + 1, min_aabb, stream, num_verts);

		AABBox max_aabb = node_aabb;
		max_aabb.min_[node.getSplittingAxis()] = node.data2.dividing_val;

		//recurse to pos child
		doWriteModel(node.getPosChildIndex(), max_aabb, stream, num_verts);
	}
}*/

/*
void ObjectTree::writeAABBModel(const AABBox& aabb, std::ostream& stream, int& num_verts) const
{
	stream << "\t\t<vertex pos=\"" << aabb.min.x << " " << aabb.min.y << " " << aabb.max.z << "\" normal=\"0 0 1\"/>\n";
	stream << "\t\t<vertex pos=\"" << aabb.min.x << " " << aabb.max.y << " " << aabb.max.z << "\" normal=\"0 0 1\"/>\n";
	stream << "\t\t<vertex pos=\"" << aabb.max.x << " " << aabb.max.y << " " << aabb.max.z << "\" normal=\"0 0 1\"/>\n";
	stream << "\t\t<vertex pos=\"" << aabb.max.x << " " << aabb.min.y << " " << aabb.max.z << "\" normal=\"0 0 1\"/>\n";

	stream << "\t\t<vertex pos=\"" << aabb.min.x << " " << aabb.min.y << " " << aabb.min.z << "\" normal=\"0 0 -1\"/>\n";
	stream << "\t\t<vertex pos=\"" << aabb.min.x << " " << aabb.max.y << " " << aabb.min.z << "\" normal=\"0 0 -1\"/>\n";
	stream << "\t\t<vertex pos=\"" << aabb.max.x << " " << aabb.max.y << " " << aabb.min.z << "\" normal=\"0 0 -1\"/>\n";
	stream << "\t\t<vertex pos=\"" << aabb.max.x << " " << aabb.min.y << " " << aabb.min.z << "\" normal=\"0 0 -1\"/>\n";

	stream << "\t\t<triangle_set>\n";
	stream << "\t\t\t<material_name>aabb</material_name>\n";
	//top faces
	stream << "\t\t\t<tri>" << num_verts << " " << num_verts + 1 << " " << num_verts + 2 << "</tri>\n";
	stream << "\t\t\t<tri>" << num_verts << " " << num_verts + 2 << " " << num_verts + 3 << "</tri>\n";

	//bottom faces
	stream << "\t\t\t<tri>" << num_verts + 4 << " " << num_verts + 5 << " " << num_verts + 6 << "</tri>\n";
	stream << "\t\t\t<tri>" << num_verts + 4 << " " << num_verts + 6 << " " << num_verts + 7 << "</tri>\n";

	stream << "\t\t</triangle_set>\n";

	num_verts += 8;
}*/


void ObjectTree::printTree(int cur, int depth, std::ostream& out)
{

	if(nodes[cur].getNodeType() == ObjectTreeNode::NODE_TYPE_LEAF)//nodes[cur].isLeafNode())
	{
		for(int i=0; i<depth; ++i)
			out << "  ";

		out << "leaf node (num leaf tris: " << nodes[cur].getNumLeafGeom() << ")\n";
	
	}
	else
	{	
		//process neg child
		this->printTree(cur + 1, depth + 1, out);

		for(int i=0; i<depth; ++i)
			out << "  ";

		out << "interior node (split axis: "  << nodes[cur].getSplittingAxis() << ", split val: "
				<< nodes[cur].data2.dividing_val << ")\n";
		
		//process pos child
		this->printTree(nodes[cur].getPosChildIndex(), depth + 1, out);
	
	}
}

	//just for debugging
double ObjectTree::traceRayAgainstAllObjects(const Ray& ray, 
											ThreadContext& thread_context, 
											js::ObjectTreePerThreadData& object_context,
											const INTERSECTABLE_TYPE*& hitob_out, HitInfo& hitinfo_out) const
{
	hitob_out = NULL;
	hitinfo_out.hittriindex = 0;
	hitinfo_out.hittricoords.set(0.0f, 0.0f);

	double closest_dist = std::numeric_limits<double>::max();
	for(unsigned int i=0; i<objects.size(); ++i)
	{
		HitInfo hitinfo;
		const double dist = objects[i]->traceRay(ray, 1e9f, thread_context, *object_context.object_context, hitinfo);
		if(dist >= 0.0 && dist < closest_dist)
		{
			hitinfo_out = hitinfo;
			hitob_out = objects[i];
			closest_dist = dist;
		}
	}

	return hitob_out ? closest_dist : -1.0;
}


/*js::ObjectTreePerThreadData* ObjectTree::allocContext() const
{
	assert(nodestack_size > 0);//will fail if not built
	return new js::ObjectTreePerThreadData((int)this->objects.size(), nodestack_size);
}
*/




void ObjectTree::getTreeStats(ObjectTreeStats& stats_out, int cur, int depth)
{
	if(nodes.empty())
		return;

	if(nodes[cur].getNodeType() == ObjectTreeNode::NODE_TYPE_LEAF)// nodes[cur].isLeafNode())
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
		stats_out.num_tris = (int)objects.size();//num_intersect_tris;

		stats_out.average_leafnode_depth /= (double)stats_out.num_leaf_nodes;
		stats_out.average_numgeom_per_leafnode = (double)stats_out.num_leaf_geom_tris / (double)stats_out.num_leaf_nodes;
	
		stats_out.max_depth = max_depth;

		stats_out.total_node_mem = (int)nodes.size() * sizeof(ObjectTreeNode);
		stats_out.leafgeom_indices_mem = stats_out.num_leaf_geom_tris * sizeof(INTERSECTABLE_TYPE*);
		stats_out.tri_mem = (int)objects.size() * sizeof(INTERSECTABLE_TYPE*);

		stats_out.num_inseparable_tri_leafs = this->num_inseparable_tri_leafs;
		stats_out.num_maxdepth_leafs = this->num_maxdepth_leafs;
		stats_out.num_under_thresh_leafs = this->num_under_thresh_leafs;
	}
}





ObjectTreeStats::ObjectTreeStats()
{
	memset(this, 0, sizeof(ObjectTreeStats));
}

ObjectTreeStats::~ObjectTreeStats()
{
}






} //end namespace js






