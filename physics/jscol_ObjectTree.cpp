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
#include "../indigo/globals.h"
#include "../indigo/PrintOutput.h"
#include "../utils/stringutils.h"
#include "../indigo/EnvSphereGeometry.h"
#include "../indigo/ThreadContext.h"
#include <string.h>


namespace js
{


//#define OBJECTTREE_VERBOSE 1


ObjectTree::ObjectTree()
{
	assertSSEAligned(this);

	nodestack_size = 0;

	nodes.push_back(ObjectTreeNode());//root node
	assert((uint64)&nodes[0] % 8 == 0);//assert aligned

	num_inseparable_tri_leafs = 0;
	num_maxdepth_leafs = 0;
	num_under_thresh_leafs = 0;
}


ObjectTree::~ObjectTree()
{
}


void ObjectTree::insertObject(INTERSECTABLE_TYPE* intersectable)
{
	intersectable->setObjectIndex((int)objects.size());
	objects.push_back(intersectable);
}


// Returns dist till hit tri, neg number if missed.
ObjectTree::Real ObjectTree::traceRay(const Ray& ray,
						   ThreadContext& thread_context,
						   double time,
						   const INTERSECTABLE_TYPE* last_object_hit,
						   unsigned int last_triangle_hit,
						   const INTERSECTABLE_TYPE*& hitob_out,
						   HitInfo& hitinfo_out) const
{
#ifdef OBJECTTREE_VERBOSE
	conPrint("-------------------------ObjectTree::traceRay()-----------------------------");
#endif
	assertSSEAligned(this);
	assertSSEAligned(&ray);
	assert(ray.unitDir().isUnitLength());

	//assert(!thread_context.in_object_tree_traversal);
	//thread_context.in_object_tree_traversal = true;

	js::ObjectTreePerThreadData& object_context = thread_context.getObjectTreeContext();

	if(object_context.last_test_time.size() < objects.size())
		object_context.last_test_time.resize(objects.size());

	object_context.time++;

	#ifdef OBJECTTREE_VERBOSE
	printVar(object_context.time);
	#endif

	hitob_out = NULL;
	hitinfo_out.sub_elem_index = 0;
	hitinfo_out.sub_elem_coords.set(0.0f, 0.0f);

	HitInfo ob_hit_info;

	if(leafgeom.empty())
	{
		//thread_context.in_object_tree_traversal = false;
		return -1.0;
	}

	assert(nodestack_size > 0);
	assert(!nodes.empty());

	__m128 near_t, far_t;
	root_aabb.rayAABBTrace(ray.startPosF().v, ray.getRecipRayDirF().v, near_t, far_t);
	near_t = _mm_max_ss(near_t, zeroVec()); // near_t = max(near_t, 0)

	if(_mm_comile_ss(near_t, far_t) == 0) // if(!(near_t <= far_t) == if near_t > far_t
	{
		//thread_context.in_object_tree_traversal = false;
		return -1.0;
	}


	SSE_ALIGN unsigned int ray_child_indices[8];
	TreeUtils::buildFlatRayChildIndices(ray, ray_child_indices);

#ifdef OBJECTTREE_VERBOSE
	printVar(aabb_enterdist);
	printVar(aabb_exitdist);
#endif
	Real closest_dist = std::numeric_limits<Real>::max();

	object_context.nodestack[0].node = 0;
	_mm_store_ss(&object_context.nodestack[0].tmin, near_t);
	_mm_store_ss(&object_context.nodestack[0].tmax, far_t);

	int stacktop = 0;//index of node on top of stack

	while(stacktop >= 0)//!nodestack.empty())
	{
		//pop node off stack
		unsigned int current = object_context.nodestack[stacktop].node;
		assert(current < nodes.size());
		__m128 tmin = _mm_load_ss(&object_context.nodestack[stacktop].tmin);
		__m128 tmax = _mm_load_ss(&object_context.nodestack[stacktop].tmax);

		stacktop--;

		tmax = _mm_min_ss(tmax, _mm_load_ss(&closest_dist));

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
					assert(stacktop < object_context.nodestack_size);
					object_context.nodestack[stacktop].node = child_nodes[ray_child_indices[splitting_axis + 4]]; // far node
					_mm_store_ss(&object_context.nodestack[stacktop].tmin, t_split);
					_mm_store_ss(&object_context.nodestack[stacktop].tmax, tmax);

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

			if(object_context.last_test_time[ob->getObjectIndex()] != object_context.time) // If this object has not already been intersected against during this traversal
			{
				const Real dist = ob->traceRay(
					ray,
					closest_dist,
					time,
					thread_context,
					(last_object_hit == ob ? last_triangle_hit : std::numeric_limits<unsigned int>::max()),
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

		if(_mm_comile_ss(_mm_load_ss(&closest_dist), tmax) != 0) // if(closest_dist <= tmax)
		{
			//thread_context.in_object_tree_traversal = false;
			return closest_dist; // If intersection point lies before ray exit from this leaf volume, then finished.
		}
	}

	//thread_context.in_object_tree_traversal = false;
	return hitob_out ? closest_dist : (Real)-1.0;
}


//For debugging
bool ObjectTree::allObjectsDoesFiniteRayHitAnything(const Ray& ray, Real length,
													ThreadContext& thread_context,
													double time) const
{
	const INTERSECTABLE_TYPE* hitob = NULL;
	HitInfo hitinfo;
	const Real dist = this->traceRayAgainstAllObjects(ray, thread_context, time, hitob, hitinfo);
	return dist >= 0.0 && dist < length;
}


bool ObjectTree::doesFiniteRayHit(const Ray& ray, Real ray_max_t,
								  ThreadContext& thread_context,
								  double time, const INTERSECTABLE_TYPE* ignore_object, unsigned int ignore_tri) const
{
#ifdef OBJECTTREE_VERBOSE
	conPrint("-------------------------ObjectTree::doesFiniteRayHitAnything()-----------------------------");
#endif
	js::ObjectTreePerThreadData& object_context = thread_context.getObjectTreeContext();

	if(object_context.last_test_time.size() < objects.size())
		object_context.last_test_time.resize(objects.size());

	//assert(!thread_context.in_object_tree_traversal);
	//thread_context.in_object_tree_traversal = true;

	assertSSEAligned(this);
	assertSSEAligned(&ray);
	assert(ray.unitDir().isUnitLength());
	assert(ray_max_t > 0.0);

	object_context.time++;

	#ifdef OBJECTTREE_VERBOSE
	printVar(object_context.time);
	#endif

	if(leafgeom.empty())
	{
		//thread_context.in_object_tree_traversal = false;
		return false;
	}

	assert(nodestack_size > 0);
	assert(!nodes.empty());

	__m128 near_t, far_t;
	root_aabb.rayAABBTrace(ray.startPosF().v, ray.getRecipRayDirF().v, near_t, far_t);
	near_t = _mm_max_ss(near_t, /*_mm_load_ss(&ray.minT())*/zeroVec()); // near_t = max(near_t, 0)

	const float ray_max_t_f = (float)ray_max_t;
	far_t = _mm_min_ss(far_t, _mm_load_ss(&ray_max_t_f)); // far_t = min(far_t, ray_max_t)

	if(_mm_comile_ss(near_t, far_t) == 0) // if(!(near_t <= far_t) == if near_t > far_t
	{
		//thread_context.in_object_tree_traversal = false;
		return false;
	}


	//object_context.nodestack[0] = StackFrame(0, root_t_min, root_t_max);
	object_context.nodestack[0].node = 0;
	_mm_store_ss(&object_context.nodestack[0].tmin, near_t);
	_mm_store_ss(&object_context.nodestack[0].tmax, far_t);


	SSE_ALIGN unsigned int ray_child_indices[8];
	TreeUtils::buildFlatRayChildIndices(ray, ray_child_indices);

	int stacktop = 0;//index of node on top of stack

	while(stacktop >= 0)//!nodestack.empty())
	{
		//pop node off stack
		unsigned int current = object_context.nodestack[stacktop].node;
		assert(current < nodes.size());
		__m128 tmin = _mm_load_ss(&object_context.nodestack[stacktop].tmin);
		__m128 tmax = _mm_load_ss(&object_context.nodestack[stacktop].tmax);

		stacktop--;

		while(nodes[current].getNodeType() != ObjectTreeNode::NODE_TYPE_LEAF)//!nodes[current].isLeafNode())
		{
			#ifdef DO_PREFETCHING
			_mm_prefetch((const char *)(&nodes[nodes[current].getPosChildIndex()]), _MM_HINT_T0);
			#endif

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
					assert(stacktop < object_context.nodestack_size);
					object_context.nodestack[stacktop].node = child_nodes[ray_child_indices[splitting_axis + 4]]; // far node
					_mm_store_ss(&object_context.nodestack[stacktop].tmin, t_split);
					_mm_store_ss(&object_context.nodestack[stacktop].tmax, tmax);

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

		unsigned int leaf_geom_index = nodes[current].getLeafGeomIndex();
		const unsigned int num_leaf_geom = nodes[current].getNumLeafGeom();
		#ifdef OBJECTTREE_VERBOSE
		printVar(leaf_geom_index);
		printVar(num_leaf_geom);
		#endif
		for(unsigned int i=0; i<num_leaf_geom; ++i)
		{
			assert(leaf_geom_index >= 0 && leaf_geom_index < leafgeom.size());
			const INTERSECTABLE_TYPE* ob = leafgeom[leaf_geom_index];
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
				if(ob->doesFiniteRayHit(ray, ray_max_t, time, thread_context,
					(ignore_object == ob ? ignore_tri : std::numeric_limits<unsigned int>::max())
					))
				{
					//thread_context.in_object_tree_traversal = false;
					return true;
				}
				object_context.last_test_time[ob->getObjectIndex()] = object_context.time;
			}
			leaf_geom_index++;
		}
	}//end while !bundlenodestack.empty()
	//thread_context.in_object_tree_traversal = false;
	return false;
}


void ObjectTree::build(PrintOutput& print_output)
{
	print_output.print("Building Object Tree...");
	print_output.print("\t" + ::toString((int)objects.size()) + " objects.");

	if(objects.size() == 0)
	{
		//assert(0);
		return;
	}

	//------------------------------------------------------------------------
	//calc root node's aabbox
	//------------------------------------------------------------------------
	print_output.print("\tcalcing root AABB.");
	{

	root_aabb = objects[0]->getAABBoxWS();

	for(unsigned int i=0; i<objects.size(); ++i)
		root_aabb.enlargeToHoldAABBox(objects[i]->getAABBoxWS());
	}

	print_output.print("\tAABB: (" + ::toString(root_aabb.min_.x[0]) + ", " + ::toString(root_aabb.min_.x[1]) + ", " + ::toString(root_aabb.min_.x[2]) + "), " +
						"(" + ::toString(root_aabb.max_.x[0]) + ", " + ::toString(root_aabb.max_.x[1]) + ", " + ::toString(root_aabb.max_.x[2]) + ")");

	assert(root_aabb.invariant());

	//------------------------------------------------------------------------
	//compute max allowable depth
	//------------------------------------------------------------------------
	const int numtris = (int)objects.size();
	max_depth = (int)(2.0 + logBase2((double)numtris) * 2.0);

	print_output.print("\tmax tree depth: " + ::toString(max_depth));

	// Alloc stack vector
	nodestack_size = max_depth + 1;
	//alignedSSEArrayMalloc(nodestack_size, nodestack);

	const int expected_numnodes = (int)((float)numtris * 1.0);
	const int nodemem = expected_numnodes * sizeof(js::ObjectTreeNode);

	print_output.print("\treserving N nodes: " + ::toString(expected_numnodes) + "("
		+ ::getNiceByteSize(nodemem) + ")");

	//------------------------------------------------------------------------
	//get array of tri indices for root node
	//------------------------------------------------------------------------
	std::vector<INTERSECTABLE_TYPE*> rootobs = objects;

	//const int tris_size = tris->size();

	//triTreeDebugPrint("calling doBuild()...");
	std::vector<SortedBoundInfo> lower(numtris);
	std::vector<SortedBoundInfo> upper(numtris);
	doBuild(0, rootobs, 0, max_depth, root_aabb, lower, upper);
	//triTreeDebugPrint("doBuild() done.");

	if(!nodes.empty())
	{
		assert(SSE::isAlignedTo(&nodes[0], sizeof(js::ObjectTreeNode)));
	}

	const uint64 numnodes = nodes.size();
	const uint64 leafgeomsize = leafgeom.size();

	print_output.print("\ttotal nodes used: " + ::toString(numnodes) + " (" +
		::getNiceByteSize((int)numnodes * sizeof(js::ObjectTreeNode)) + ")");

	print_output.print("\ttotal leafgeom size: " + ::toString(leafgeomsize) + " (" +
		::getNiceByteSize((int)leafgeomsize * sizeof(INTERSECTABLE_TYPE*)) + ")");

	/*ObjectTreeStats stats;
	this->getTreeStats(stats);
	print_output.print("\ttotal_num_nodes: " + toString(stats.total_num_nodes));
	print_output.print("\tnum_interior_nodes: " + toString(stats.num_interior_nodes));
	print_output.print("\tnum_leaf_nodes: " + toString(stats.num_leaf_nodes));
	print_output.print("\tnum_objects: " + toString(stats.num_objects));
	print_output.print("\tnum_leafgeom_objects: " + toString(stats.num_leafgeom_objects));
	print_output.print("\taverage_leafnode_depth: " + toString(stats.average_leafnode_depth));
	print_output.print("\taverage_objects_per_leafnode: " + toString(stats.average_objects_per_leafnode));
	print_output.print("\tmax_leaf_objects: " + toString(stats.max_leaf_objects));
	print_output.print("\tmax_leafnode_depth: " + toString(stats.max_leafnode_depth));
	print_output.print("\tmax_depth: " + toString(stats.max_depth));
	print_output.print("\tnum_inseparable_tri_leafs: " + toString(stats.num_inseparable_tri_leafs));
	print_output.print("\tnum_maxdepth_leafs: " + toString(stats.num_maxdepth_leafs));
	print_output.print("\tnum_under_thresh_leafs: " + toString(stats.num_under_thresh_leafs));*/

	print_output.print("Finished building tree.");
}


static inline bool SortedBoundInfoLowerPred(const ObjectTree::SortedBoundInfo& a, const ObjectTree::SortedBoundInfo& b)
{
   return a.lower < b.lower;
}


static inline bool SortedBoundInfoUpperPred(const ObjectTree::SortedBoundInfo& a, const ObjectTree::SortedBoundInfo& b)
{
   return a.upper < b.upper;
}


void ObjectTree::doBuild(int cur, //index of current node getting built
						const std::vector<INTERSECTABLE_TYPE*>& nodeobjs,//objects for current node
						int depth, //depth of current node
						int maxdepth, //max permissible depth
						const AABBox& cur_aabb,//AABB of current node
						std::vector<SortedBoundInfo>& upper,
						std::vector<SortedBoundInfo>& lower
					)
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

	if(nodeobjs.size() <= SPLIT_THRESHOLD || depth >= maxdepth || cur_aabb.getSurfaceArea() == 0.0f)
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

	//------------------------------------------------------------------------
	//compute non-split cost
	//------------------------------------------------------------------------
	// SAH cost constants
	const float traversal_cost = 1.0f;
	const float intersection_cost = 4.0f;
	float smallest_cost = (float)nodeobjs.size() * intersection_cost;
	//float smallest_cost = std::numeric_limits<float>::max();
	int best_axis = -1;
	float best_div_val = -666.0f;
	int best_num_in_neg = 0;
	int best_num_in_pos = 0;

	const float aabb_surface_area = cur_aabb.getSurfaceArea();
	const float recip_aabb_surf_area = 1.0f / aabb_surface_area;

	bool best_push_right = false;

	const unsigned int numtris = (unsigned int)nodeobjs.size();

	const unsigned int nonsplit_axes[3][2] = {{1, 2}, {0, 2}, {0, 1}};

	for(int axis=0; axis<3; ++axis)
	{
		const int axis1 = nonsplit_axes[axis][0];
		const int axis2 = nonsplit_axes[axis][1];
		const float two_cap_area = cur_aabb.axisLength(axis1) * cur_aabb.axisLength(axis2) * 2.0f;
		const float circum = (cur_aabb.axisLength(axis1) + cur_aabb.axisLength(axis2)) * 2.0f;

		if(cur_aabb.axisLength(axis) == 0.0f)
			continue; // Don't try to split a zero-width bounding box

		//------------------------------------------------------------------------
		//Sort lower and upper tri AABB bounds into ascending order
		//------------------------------------------------------------------------
		for(unsigned int i=0; i<numtris; ++i)
		{
			lower[i].lower = nodeobjs[i]->getAABBoxWS().min_.x[axis];
			lower[i].upper = nodeobjs[i]->getAABBoxWS().max_.x[axis];
			upper[i].lower = nodeobjs[i]->getAABBoxWS().min_.x[axis];
			upper[i].upper = nodeobjs[i]->getAABBoxWS().max_.x[axis];
		}

		std::sort(lower.begin(), lower.begin() + numtris, SortedBoundInfoLowerPred);
		std::sort(upper.begin(), upper.begin() + numtris, SortedBoundInfoUpperPred);

		unsigned int upper_index = 0;
		//Index of first triangle in upper volume
		//Index of first tri with upper bound >= then current split val.
		//Also the number of tris with upper bound <= current split val.
		//which is the number of tris *not* in pos volume

		float last_splitval = -std::numeric_limits<float>::max();
		for(unsigned int i=0; i<numtris; ++i)
		{
			const float splitval = lower[i].lower;
			//assert(splitval >= cur_aabb.min_.x[axis] && splitval <= cur_aabb.max_.x[axis]);

			if(splitval != last_splitval) // Only consider first tri seen with a given lower bound.
			{
				if(splitval >= cur_aabb.min_.x[axis] && splitval <= cur_aabb.max_.x[axis]) // If split val is actually in AABB
				{

					// Get number of tris on splitting plane
					int num_on_splitting_plane = 0;
					for(unsigned int z=i; z<numtris && lower[z].lower == splitval; ++z) // For all other triangles that share the current splitting plane as a lower bound
						if(lower[z].upper == splitval) // If the tri has zero extent along the current axis
							num_on_splitting_plane++; // Then it lies on the splitting plane.

					while(upper_index < numtris && upper[upper_index].upper <= splitval)
						upper_index++;

					assert(upper_index == numtris || upper[upper_index].upper > splitval);

					const int num_in_neg = i;
					if(i == 7500)
						int a = 0;//TEMP
					const int num_in_pos = numtris - upper_index;
					assert(num_in_neg >= 0 && num_in_neg <= (int)numtris);
					assert(num_in_pos >= 0 && num_in_pos <= (int)numtris);
					assert(num_in_neg + num_in_pos + num_on_splitting_plane >= (int)numtris);


					const float negchild_surface_area = two_cap_area + (splitval - cur_aabb.min_.x[axis]) * circum;
					const float poschild_surface_area = two_cap_area + (cur_aabb.max_.x[axis] - splitval) * circum;
					assert(negchild_surface_area >= 0.f && negchild_surface_area <= aabb_surface_area * (1.0f + NICKMATHS_EPSILON));
					assert(poschild_surface_area >= 0.f && negchild_surface_area <= aabb_surface_area * (1.0f + NICKMATHS_EPSILON));
					assert(::epsEqual(negchild_surface_area + poschild_surface_area - two_cap_area, aabb_surface_area, aabb_surface_area * 1.0e-6f));

					if(num_on_splitting_plane == 0)
					{
						const float cost = traversal_cost + ((float)num_in_neg * negchild_surface_area + (float)num_in_pos * poschild_surface_area) *
							recip_aabb_surf_area * intersection_cost;
						assert(cost >= 0.0);

						//conPrint("axis: " + toString(axis) + ", i: " + toString(i) + ", cost: " + toString(cost));

						if(cost < smallest_cost)
						{
							smallest_cost = cost;
							best_axis = axis;
							best_div_val = splitval;
							best_num_in_neg = num_in_neg;
							best_num_in_pos = num_in_pos;
							//best_num_on_split_plane = 0;
						}
					}
					else
					{
						// Try pushing tris on splitting plane to left
						const float push_left_cost = traversal_cost + ((float)(num_in_neg + num_on_splitting_plane) * negchild_surface_area + (float)num_in_pos * poschild_surface_area) *
							recip_aabb_surf_area * intersection_cost;
						assert(push_left_cost >= 0.0);

						if(push_left_cost < smallest_cost)
						{
							smallest_cost = push_left_cost;
							best_axis = axis;
							best_div_val = splitval;
							best_num_in_neg = num_in_neg + num_on_splitting_plane;
							best_num_in_pos = num_in_pos;
							//best_num_on_split_plane = 0;
							best_push_right = false;
						}

						// Try pushing tris on splitting plane to right
						const float push_right_cost = traversal_cost + ((float)num_in_neg * negchild_surface_area + (float)(num_in_pos + num_on_splitting_plane) * poschild_surface_area) *
							recip_aabb_surf_area * intersection_cost;
						assert(push_right_cost >= 0.0);

						if(push_right_cost < smallest_cost)
						{
							smallest_cost = push_right_cost;
							best_axis = axis;
							best_div_val = splitval;
							best_num_in_neg = num_in_neg;
							best_num_in_pos = num_in_pos + num_on_splitting_plane;
							//best_num_on_split_plane = 0;
							best_push_right = true;
						}
					}
				}
				last_splitval = splitval;
			}
		}
		unsigned int lower_index = 0;//index of first tri with lower bound >= current split val
		//Also the number of tris that are in the negative volume

		for(unsigned int i=0; i<numtris; ++i)
		{
			// Advance to greatest index with given upper bound
			while(i+1 < numtris && upper[i].upper == upper[i+1].upper) // While triangle i has some upper bound as triangle i+1
				++i;


			const float splitval = upper[i].upper;
			//assert(splitval >= cur_aabb.min_.x[axis] && splitval <= cur_aabb.max_.x[axis]);

			if(splitval >= cur_aabb.min_.x[axis] && splitval <= cur_aabb.max_.x[axis])
			{

				int num_on_splitting_plane = 0;
				for(int z=i; z>=0 && upper[z].upper == splitval; --z) // For each triangle sharing an upper bound with the current triangle
					if(upper[z].lower == splitval) // If tri has zero extent along this axis
						num_on_splitting_plane++;

				while(lower_index < numtris && lower[lower_index].lower < splitval)
					lower_index++;

				// Postcondition:
				assert(lower_index == numtris || lower[lower_index].lower >= splitval);

				const int num_in_neg = lower_index;
				const int num_in_pos = numtris - (i + 1);
				assert(num_in_neg >= 0 && num_in_neg <= (int)numtris);
				assert(num_in_pos >= 0 && num_in_pos <= (int)numtris);
				assert(num_in_neg + num_in_pos + num_on_splitting_plane >= (int)numtris);

				//const int num_on_splitting_plane = numtris - (num_in_neg + num_in_pos);

				const float negchild_surface_area = two_cap_area + (splitval - cur_aabb.min_.x[axis]) * circum;
				const float poschild_surface_area = two_cap_area + (cur_aabb.max_.x[axis] - splitval) * circum;
				assert(negchild_surface_area >= 0.f && negchild_surface_area <= aabb_surface_area * (1.0f + NICKMATHS_EPSILON));
				assert(poschild_surface_area >= 0.f && negchild_surface_area <= aabb_surface_area * (1.0f + NICKMATHS_EPSILON));
				assert(::epsEqual(negchild_surface_area + poschild_surface_area - two_cap_area, aabb_surface_area, aabb_surface_area * 1.0e-6f));

				if(num_on_splitting_plane == 0)
				{
					const float cost = traversal_cost + ((float)num_in_neg * negchild_surface_area + (float)num_in_pos * poschild_surface_area) *
						recip_aabb_surf_area * intersection_cost;
					assert(cost >= 0.0);

					if(cost < smallest_cost)
					{
						smallest_cost = cost;
						best_axis = axis;
						best_div_val = splitval;
						best_num_in_neg = num_in_neg;
						best_num_in_pos = num_in_pos;
						//best_num_on_split_plane = 0;
					}
				}
				else
				{
					// Try pushing tris on splitting plane to left
					const float push_left_cost = traversal_cost + ((float)(num_in_neg + num_on_splitting_plane) * negchild_surface_area + (float)num_in_pos * poschild_surface_area) *
						recip_aabb_surf_area * intersection_cost;
					assert(push_left_cost >= 0.0);

					if(push_left_cost < smallest_cost)
					{
						smallest_cost = push_left_cost;
						best_axis = axis;
						best_div_val = splitval;
						best_num_in_neg = num_in_neg + num_on_splitting_plane;
						best_num_in_pos = num_in_pos;
						//best_num_on_split_plane = 0;
						best_push_right = false;
					}

					// Try pushing tris on splitting plane to right
					const float push_right_cost = traversal_cost + ((float)num_in_neg * negchild_surface_area + (float)(num_in_pos + num_on_splitting_plane) * poschild_surface_area) *
						recip_aabb_surf_area * intersection_cost;
					assert(push_right_cost >= 0.0);

					if(push_right_cost < smallest_cost)
					{
						smallest_cost = push_right_cost;
						best_axis = axis;
						best_div_val = splitval;
						best_num_in_neg = num_in_neg;
						best_num_in_pos = num_in_pos + num_on_splitting_plane;
						//best_num_on_split_plane = 0;
						best_push_right = true;
					}
				}
			}
		}
	}


		//Tris in contact with splitting plane will not imply tri is in either volume, except
		//for tris lying totally on splitting plane which is considered to be in positive volume.

		/*unsigned int upper_index = 0;//Index of first tri with upper bound > then current split val.
		//Also the number of tris with upper bound <= current split val.
		//which is the number of tris *not* in pos volume

		float last_splitval = -1e20f;
		for(unsigned int i=0; i<numtris; ++i)
		{
			const float splitval = lower[i];

			if(splitval != last_splitval)//only consider first tri seen with a given lower bound.
			{
				if(splitval > cur_aabb.min_.x[axis] && splitval < cur_aabb.max_.x[axis])
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
					const float negchild_surface_area = two_cap_area + (splitval - cur_aabb.min_.x[axis]) * circum;
					const float poschild_surface_area = two_cap_area + (cur_aabb.max_.x[axis] - splitval) * circum;

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
				if(splitval > cur_aabb.min_.x[axis] && splitval < cur_aabb.max_.x[axis])
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
					const float negchild_surface_area = two_cap_area + (splitval - cur_aabb.min_.x[axis]) * circum;
					const float poschild_surface_area = two_cap_area + (cur_aabb.max_.x[axis] - splitval) * circum;

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
	}*/

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

	//if(best_num_in_neg == 15002 && best_num_in_pos == 1)
	//	conPrint("Splitting along axis " + toString(best_axis) + ", best_num_in_neg: " + toString(best_num_in_neg) + ", best_num_in_pos: " + toString(best_num_in_pos));

	assert(best_axis >= 0 && best_axis <= 2);
	assert(best_div_val != -666.0f);

	assert(best_div_val >= cur_aabb.min_.x[best_axis]);
	assert(best_div_val <= cur_aabb.max_.x[best_axis]);

	//------------------------------------------------------------------------
	//compute AABBs of child nodes
	//------------------------------------------------------------------------
	assert(best_axis >= 0 && best_axis <= 2);

	AABBox negbox(cur_aabb.min_, cur_aabb.max_);
	negbox.max_.x[best_axis] = best_div_val;

	AABBox posbox(cur_aabb.min_, cur_aabb.max_);
	posbox.min_.x[best_axis] = best_div_val;

	//------------------------------------------------------------------------
	//assign tris to neg and pos child node bins
	//------------------------------------------------------------------------
	std::vector<INTERSECTABLE_TYPE*> neg_objs;
	std::vector<INTERSECTABLE_TYPE*> pos_objs;
	//neg_objs.reserve(best_num_in_neg);//nodeobjs.size() / 2);
	//std::vector<INTERSECTABLE_TYPE*> pos_objs;
	//pos_objs.reserve(best_num_in_pos);//nodeobjs.size() / 2);

	/*for(unsigned int i=0; i<nodeobjs.size(); ++i)//for each tri
	{
		bool inserted = false;
		if(intersectableIntersectsAABB(nodeobjs[i], negbox, best_axis, true))
		{
			neg_objs.push_back(nodeobjs[i]);
			inserted = true;
		}

		if(intersectableIntersectsAABB(nodeobjs[i], posbox, best_axis, false))
		{
			pos_objs.push_back(nodeobjs[i]);
			inserted = true;
		}
	}*/

	const float split = best_div_val;
	for(unsigned int i=0; i<numtris; ++i)//for each tri
	{
		if(nodeobjs[i]->isEnvSphereGeometry())
		{
			if(doesEnvSphereObjectIntersectAABB(nodeobjs[i], negbox))
				neg_objs.push_back(nodeobjs[i]);
		}
		else
		{
			const float tri_lower = nodeobjs[i]->getAABBoxWS().min_[best_axis]; //nodetris[i].lower[best_axis];
			const float tri_upper = nodeobjs[i]->getAABBoxWS().max_[best_axis]; //nodetris[i].upper[best_axis];

			if(tri_lower <= split) // Tri touches left volume, including splitting plane
			{
				if(tri_lower < split) // Tri touches left volume, not including splitting plane
				{
					neg_objs.push_back(nodeobjs[i]);
				}
				else
				{
					// else tri_lower == split
					if(tri_upper == split && !best_push_right)
						neg_objs.push_back(nodeobjs[i]);
				}
			}
			// Else tri_lower > split, so doesn't intersect left box of splitting plane.
		}
	}

	const int num_in_neg = (int)neg_objs.size();


	//child_tris.resize(0);

	for(unsigned int i=0; i<numtris; ++i) // For each tri
	{
		if(nodeobjs[i]->isEnvSphereGeometry())
		{
			if(doesEnvSphereObjectIntersectAABB(nodeobjs[i], posbox))
				pos_objs.push_back(nodeobjs[i]);
		}
		else
		{
			const float tri_lower = nodeobjs[i]->getAABBoxWS().min_[best_axis]; //nodetris[i].lower[best_axis];
			const float tri_upper = nodeobjs[i]->getAABBoxWS().max_[best_axis]; //nodetris[i].upper[best_axis];

			if(tri_upper >= split) // Tri touches right volume, including splitting plane
			{
				if(tri_upper > split) // Tri touches right volume, not including splitting plane
				{
					pos_objs.push_back(nodeobjs[i]);
				}
				else
				{
					// else tri_upper == split
					if(tri_lower == split && best_push_right)
						pos_objs.push_back(nodeobjs[i]);
				}
			}
			// Else tri_upper < split, so doesn't intersect right box of splitting plane.
		}
	}


	const int num_in_pos = (int)pos_objs.size();
	const int num_tris_in_both = num_in_neg + num_in_pos - numtris;

	//assert(num_in_neg == best_num_in_neg);
	//assert(num_in_pos == best_num_in_pos);

	//::triTreeDebugPrint("split " + ::toString(nodetris.size()) + " tris: left: " + ::toString(neg_tris.size())
	//	+ ", right: " + ::toString(pos_tris.size()) + ", both: " + ::toString(num_tris_in_both) );

	if(num_in_neg == numtris && num_in_pos == numtris)
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

	// Build left subtree, recursive call.
	doBuild(negchild_index, neg_objs, depth + 1, maxdepth, negbox, lower, upper);

	//------------------------------------------------------------------------
	//create positive child
	//------------------------------------------------------------------------
	nodes.push_back(ObjectTreeNode());

	// Set details for current node
	nodes[cur] = ObjectTreeNode(
		best_axis, // splitting axis
		best_div_val, // split
		(int)nodes.size() - 1 // right child node index
		);

	// Build right subtree
	doBuild(nodes[cur].getPosChildIndex(), pos_objs, depth + 1, maxdepth, posbox, lower, upper);
}


bool ObjectTree::doesEnvSphereObjectIntersectAABB(INTERSECTABLE_TYPE* ob, const AABBox& aabb)
{
	assert(ob->isEnvSphereGeometry());

	const Vec4f origin(0,0,0,1.f);

	for(unsigned int i=0; i<8; ++i) // For each corner of 'aabb'
	{
		const unsigned int x = i & 0x1;
		const unsigned int y = (i >> 1) & 0x1;
		const unsigned int z = (i >> 2) & 0x1;

		const Vec4f corner(
			x == 0 ? aabb.min_.x[0] : aabb.max_.x[0],
			y == 0 ? aabb.min_.x[1] : aabb.max_.x[1],
			z == 0 ? aabb.min_.x[2] : aabb.max_.x[2],
			1.0f);

		const float corner_dist = corner.getDist(origin);

		if(corner_dist >= ((const EnvSphereGeometry*)(&ob->getGeometry()))->getBoundingRadius())
			return true;
	}

	return false;
}


bool ObjectTree::intersectableIntersectsAABB(INTERSECTABLE_TYPE* ob, const AABBox& aabb, int split_axis,
								bool is_neg_child)
{
	const AABBox& ob_aabb = ob->getAABBoxWS();

	// Test for special case in which aabb lies entirely inside the environment sphere geometry.
	if(ob->isEnvSphereGeometry())
	{
		const Vec4f origin(0,0,0,1.f);

		for(unsigned int i=0; i<8; ++i) // For each corner of 'aabb'
		{
			const unsigned int x = i & 0x1;
			const unsigned int y = (i >> 1) & 0x1;
			const unsigned int z = (i >> 2) & 0x1;

			const Vec4f corner(
				x == 0 ? aabb.min_.x[0] : aabb.max_.x[0],
				y == 0 ? aabb.min_.x[1] : aabb.max_.x[1],
				z == 0 ? aabb.min_.x[2] : aabb.max_.x[2],
				1.0f);

			const float corner_dist = corner.getDist(origin);

			if(corner_dist >= ((const EnvSphereGeometry*)(&ob->getGeometry()))->getBoundingRadius())
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------
	//test each separating axis
	//------------------------------------------------------------------------
	/*for(int axis=0; axis<3; ++axis) // For each axis
	{
		if(axis == split_axis)
		{
			if(ob_aabb.min_[axis] == ob_aabb.max_[axis]) // Object
		}
		else
		{
			if(ob_aabb.max_.x[axis] <= aabb.min_.x[axis] ||
				ob_aabb.min_.x[axis] >= aabb.max_.x[axis])
			{
				return false;
			}
		}
	}*/
	return true;

#if 0
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

				if(ob_aabb.max_.x[i] < aabb.min_.x[i])
					return false;

				if(ob_aabb.min_.x[i] >= aabb.max_.x[i])
					return false;
			}
			else
			{
				//then this box gets everything that touches it, EXCEPT the
				//tris which hit the split. (ie hit min[split_axis])
				//if(tri_boxes[triindex].max[i] <= aabb.min[i])//reject even if touching
				//	return false;
				if(ob_aabb.max_.x[i] <= aabb.min_.x[i])
				{
					if(!(ob_aabb.max_.x[i] == ob_aabb.min_.x[i] &&
						ob_aabb.max_.x[i] == aabb.min_.x[i]))//if tri doesn't lie entirely on splitting plane
					return false;
				}

				if(ob_aabb.min_.x[i] > aabb.max_.x[i])
					return false;
			}
		}
		else
		{
			//tris touching min or max planes are considered in aabb
			if(ob_aabb.max_.x[i] < aabb.min_.x[i])
				return false;

			if(ob_aabb.min_.x[i] > aabb.max_.x[i])
				return false;
		}
	}

	return true;
#endif
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
ObjectTree::Real ObjectTree::traceRayAgainstAllObjects(const Ray& ray,
											ThreadContext& thread_context,
											//js::ObjectTreePerThreadData& object_context,
											double time,
											const INTERSECTABLE_TYPE*& hitob_out,
											HitInfo& hitinfo_out) const
{
	hitob_out = NULL;
	hitinfo_out.sub_elem_index = 0;
	hitinfo_out.sub_elem_coords.set(0.0, 0.0);

	Real closest_dist = std::numeric_limits<Real>::max();
	for(unsigned int i=0; i<objects.size(); ++i)
	{
		HitInfo hitinfo;
		const Real dist = objects[i]->traceRay(ray, 1e9f, time, thread_context, std::numeric_limits<unsigned int>::max(), hitinfo);
		if(dist >= 0.0 && dist < closest_dist)
		{
			hitinfo_out = hitinfo;
			hitob_out = objects[i];
			closest_dist = dist;
		}
	}

	return hitob_out ? closest_dist : (Real)-1.0;
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
		stats_out.num_leafgeom_objects += (int)nodes[cur].getNumLeafGeom();
		stats_out.average_leafnode_depth += (double)depth;
		stats_out.max_leaf_objects = myMax(stats_out.max_leaf_objects, (int)nodes[cur].getNumLeafGeom());
		stats_out.max_leafnode_depth = myMax(stats_out.max_leafnode_depth, depth);
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
		stats_out.num_objects = (int)objects.size();//num_intersect_tris;

		stats_out.average_leafnode_depth /= (double)stats_out.num_leaf_nodes;
		stats_out.average_objects_per_leafnode = (double)stats_out.num_leafgeom_objects / (double)stats_out.num_leaf_nodes;

		stats_out.max_depth = max_depth;

		stats_out.total_node_mem = (int)nodes.size() * sizeof(ObjectTreeNode);
		stats_out.leafgeom_indices_mem = stats_out.num_leafgeom_objects * sizeof(INTERSECTABLE_TYPE*);
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
