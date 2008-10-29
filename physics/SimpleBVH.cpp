/*=====================================================================
SimpleBVH.cpp
-------------
File created by ClassTemplate on Mon Oct 27 20:35:09 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "SimpleBVH.h"


#include "jscol_aabbox.h"
#include "../indigo/globals.h"
#include "../utils/timer.h"
#include "../simpleraytracer/raymesh.h"
#include "TreeUtils.h"
#include <omp.h>


namespace js
{


SimpleBVH::SimpleBVH(RayMesh* raymesh_)
:	raymesh(raymesh_),
	num_intersect_tris(0),
	intersect_tris(NULL),
	root_aabb(NULL),
	tri_aabbs(NULL)
{
	assert(raymesh);

	num_intersect_tris = 0;
	intersect_tris = NULL;

	root_aabb = (js::AABBox*)alignedSSEMalloc(sizeof(AABBox));
	new(root_aabb) AABBox(Vec3f(0,0,0), Vec3f(0,0,0));

	num_maxdepth_leaves = 0;
	num_under_thresh_leaves = 0;
	num_leaves = 0;
	max_num_tris_per_leaf = 0;
	leaf_depth_sum = 0;
	max_leaf_depth = 0;
	num_cheaper_nosplit_leaves = 0;

	assert(sizeof(SimpleBVHNode) == 64);
}


SimpleBVH::~SimpleBVH()
{
	assert(tri_aabbs == NULL);
	alignedArrayFree(nodes);
	alignedSSEFree(root_aabb);
	alignedSSEFree(intersect_tris);
	intersect_tris = NULL;
}


const Vec3f& SimpleBVH::triVertPos(unsigned int tri_index, unsigned int vert_index_in_tri) const
{
	return raymesh->triVertPos(tri_index, vert_index_in_tri);
}


unsigned int SimpleBVH::numTris() const
{
	return raymesh->getNumTris();
}


class CenterPredicate
{
public:
	CenterPredicate(int axis_, const std::vector<Vec3f>& tri_centers_) : axis(axis_), tri_centers(tri_centers_) {}

	inline bool operator()(unsigned int t1, unsigned int t2)
	{
		//return sbvh->triCenter(t1, axis) < sbvh->triCenter(t2, axis);
		return tri_centers[t1][axis] < tri_centers[t2][axis];
	}

private:
	int axis;
	//SimpleBVH* sbvh;
	const std::vector<Vec3f>& tri_centers;
};


void SimpleBVH::build()
{
	conPrint("\tSimpleBVH::build()");

	//------------------------------------------------------------------------
	//Calc root node's aabbox
	//NOTE: could do this faster by looping over vertices instead.
	//------------------------------------------------------------------------
	const int num_tris = (int)numTris();

	Timer timer;

	TreeUtils::buildRootAABB(*raymesh, *root_aabb);
	assert(root_aabb->invariant());

	//------------------------------------------------------------------------
	//alloc intersect tri array
	//------------------------------------------------------------------------
	conPrint("\tAllocating intersect triangles...");
	this->num_intersect_tris = numTris();
	if(::atDebugLevel(DEBUG_LEVEL_VERBOSE))
		conPrint("\t\tIntersect_tris mem usage: " + ::getNiceByteSize(num_intersect_tris * sizeof(INTERSECT_TRI_TYPE)));

	::alignedSSEArrayMalloc(num_intersect_tris, intersect_tris);
	if(intersect_tris == NULL)
		throw TreeExcep("Memory allocation failure.");
	intersect_tri_i = 0;

	{
	// Build tri AABBs
	::alignedSSEArrayMalloc(numTris(), tri_aabbs);
	if(tri_aabbs == NULL)
		throw TreeExcep("Memory allocation failure.");

	for(unsigned int i=0; i<numTris(); ++i)
	{
		const SSE_ALIGN PaddedVec3f v1(triVertPos(i, 1));
		const SSE_ALIGN PaddedVec3f v2(triVertPos(i, 2));

		tri_aabbs[i].min_ = tri_aabbs[i].max_ = triVertPos(i, 0);
		tri_aabbs[i].enlargeToHoldAlignedPoint(v1);
		tri_aabbs[i].enlargeToHoldAlignedPoint(v2);
	}

	// Build tri centers
	std::vector<Vec3f> tri_centers(numTris());
#pragma omp parallel for
	for(int i=0; i<num_tris; ++i)
	{
		//SSE_ALIGN AABBox aabb;
		//triAABB(i, aabb);
		tri_centers[i] = tri_aabbs[i].centroid();
	}

	std::vector<std::vector<TRI_INDEX> > tris(3);
	std::vector<std::vector<TRI_INDEX> > temp(2);
	temp[0].resize(numTris());
	temp[1].resize(numTris());

	conPrint("\tSorting...");
	Timer sort_timer;
	// Sort indices based on center position along the axes
	//printVar(omp_get_num_procs());
	//printVar(omp_get_max_threads());
#pragma omp parallel for
	for(int axis=0; axis<3; ++axis)
	{
		//printVar(axis);
		//printVar(omp_get_thread_num());
		//printVar(omp_get_num_threads());

		tris[axis].resize(numTris());
		for(unsigned int i=0; i<numTris(); ++i)
			tris[axis][i] = i;

		// Sort based on center along axis 'axis'
		std::sort<std::vector<unsigned int>::iterator, CenterPredicate>(tris[axis].begin(), tris[axis].end(), CenterPredicate(axis, tri_centers));
	}
	conPrint("\t\tDone (" + toString(sort_timer.getSecondsElapsed()) + " s).");


	conPrint("\tPrebuild time: " + toString(timer.getSecondsElapsed()) + " s.");

	nodes_capacity = myMax(2u, numTris() / 4);
	alignedArrayMalloc(nodes_capacity, SimpleBVHNode::requiredAlignment(), nodes);
	if(nodes == NULL)
		throw TreeExcep("Memory allocation failure.");
	num_nodes = 1;
	doBuild(
		*root_aabb, // AABB
		tris,
		temp,
		tri_centers,
		0, // left
		numTris(), // right
		0, // node index to use
		0 // depth
		);

	assert(intersect_tri_i == num_intersect_tris);
	}

	::alignedSSEArrayFree(tri_aabbs);
	tri_aabbs = NULL;

	conPrint("\tBuild Stats:");
	conPrint("\t\tTotal nodes used: " + ::toString((unsigned int)num_nodes) + " (" + ::getNiceByteSize(num_nodes * sizeof(SimpleBVHNode)) + ")");
	conPrint("\t\tNum sub threshold leaves: " + toString(num_under_thresh_leaves));
	conPrint("\t\tNum max depth leaves: " + toString(num_maxdepth_leaves));
	conPrint("\t\tNum cheaper no-split leaves: " + toString(num_cheaper_nosplit_leaves));
	conPrint("\t\tMean tris per leaf: " + toString((float)numTris() / (float)num_leaves));
	conPrint("\t\tMax tris per leaf: " + toString(max_num_tris_per_leaf));
	conPrint("\t\tMean leaf depth: " + toString((float)leaf_depth_sum / (float)num_leaves));
	conPrint("\t\tMax leaf depth: " + toString(max_leaf_depth));

	conPrint("\tFinished building tree.");	
}


void SimpleBVH::markLeafNode(const std::vector<std::vector<TRI_INDEX> >& tris, unsigned int node_index, int left, int right)
{
	nodes[node_index].setLeaf(true);
	nodes[node_index].setGeomIndex(intersect_tri_i);

	for(int i=left; i<right; ++i)
	{
		const unsigned int source_tri = tris[0][i];
		assert(intersect_tri_i < num_intersect_tris);
		intersect_tris[intersect_tri_i++].set(triVertPos(source_tri, 0), triVertPos(source_tri, 1), triVertPos(source_tri, 2));
	}

	nodes[node_index].setNumGeom(myMax(right - left, 0));

	// Update build stats
	max_num_tris_per_leaf = myMax(max_num_tris_per_leaf, right - left);
	num_leaves++;
}


/*float SimpleBVH::triCenter(unsigned int tri_index, unsigned int axis)
{
	//const float min = myMin(triVertPos(tri_index, 0)[axis], triVertPos(tri_index, 1)[axis], triVertPos(tri_index, 2)[axis]);
	//const float max = myMax(triVertPos(tri_index, 0)[axis], triVertPos(tri_index, 1)[axis], triVertPos(tri_index, 2)[axis]);
	//return 0.5f * (min + max);

	SSE_ALIGN AABBox aabb;
	triAABB(tri_index, aabb);
	return 0.5f * (aabb.min_[axis] + aabb.max_[axis]);
}*/


void SimpleBVH::triAABB(unsigned int tri_index, AABBox& aabb_out)
{
	/*const SSE_ALIGN PaddedVec3f v1(triVertPos(tri_index, 1));
	const SSE_ALIGN PaddedVec3f v2(triVertPos(tri_index, 2));

	aabb_out.min_ = aabb_out.max_ = triVertPos(tri_index, 0);
	aabb_out.enlargeToHoldAlignedPoint(v1);
	aabb_out.enlargeToHoldAlignedPoint(v2);*/

	aabb_out = tri_aabbs[tri_index];
}


/*
The triangles [left, right) are considered to belong to this node.
The AABB for this node (build_nodes[node_index]) should be set already.
*/
void SimpleBVH::doBuild(const AABBox& aabb, std::vector<std::vector<TRI_INDEX> >& tris, std::vector<std::vector<TRI_INDEX> >& temp, const std::vector<Vec3f>& tri_centers,
						int left, int right, unsigned int node_index, int depth)
{
	//assert(left >= 0 && left < (int)numTris() && right >= 0 && right < (int)numTris());
	assert(node_index < num_nodes);
#ifdef DEBUG
	// Check ordering
	for(int axis=0; axis<3; ++axis)
		for(int i=left + 1; i<right; ++i)
		{
			//assert(triCenter(tris[axis][i-1], axis) <= triCenter(tris[axis][i], axis));
			assert(tri_centers[tris[axis][i-1]][axis] <= tri_centers[tris[axis][i]][axis]);
		}
#endif

	const int LEAF_NUM_TRI_THRESHOLD = 4;

	const int MAX_DEPTH = js::Tree::MAX_TREE_DEPTH;

	if(right - left <= LEAF_NUM_TRI_THRESHOLD || depth >= MAX_DEPTH)
	{
		markLeafNode(tris, node_index, left, right);

		// Update build stats
		if(depth >= MAX_DEPTH)
			num_maxdepth_leaves++;
		else
			num_under_thresh_leaves++;
		leaf_depth_sum += depth;
		max_leaf_depth = myMax(max_leaf_depth, depth);
		return;
	}

	// Compute best split plane
	const float traversal_cost = 1.0f;
	const float intersection_cost = 4.0f;
	const float aabb_surface_area = aabb.getSurfaceArea();
	const float recip_aabb_surf_area = 1.0f / aabb_surface_area;

	const unsigned int nonsplit_axes[3][2] = {{1, 2}, {0, 2}, {0, 1}};

	//int best_axis = -1;
	//float best_div_val = std::numeric_limits<float>::infinity();

	// Compute non-split cost
	float smallest_cost = (float)(right - left) * intersection_cost;

	//int best_i = -1;

	//Timer timer;

	float axis_smallest_cost[3] = {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};
	float axis_best_div_val[3] = {std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()};

	// For each axis
//#pragma omp parallel for
	for(int axis=0; axis<3; ++axis)
	{
		//printVar(axis);
		//printVar(omp_get_thread_num());
		//printVar(omp_get_num_threads());

		const std::vector<TRI_INDEX>& axis_tris = tris[axis];

		// SAH stuff
		const unsigned int axis1 = nonsplit_axes[axis][0];
		const unsigned int axis2 = nonsplit_axes[axis][1];
		const float two_cap_area = (aabb.axisLength(axis1) * aabb.axisLength(axis2)) * 2.0f;
		const float circum = (aabb.axisLength(axis1) + aabb.axisLength(axis2)) * 2.0f;
	
		// For Each triangle centroid
		for(int i=left; i<right; ++i)
		{
			const float splitval = tri_centers[axis_tris[i]][axis];//triCenter(tris[axis][i], axis);

			// Compute the SAH cost at the centroid position
			const int N_L = (i - left) + 1;
			const int N_R = (right - left) - N_L;

			//assert(N_L >= 0 && N_L <= (int)centers.size() && N_R >= 0 && N_R <= (int)centers.size());

			// Compute SAH cost
			const float negchild_surface_area = two_cap_area + (splitval - aabb.min_[axis]) * circum;
			const float poschild_surface_area = two_cap_area + (aabb.max_[axis] - splitval) * circum;

			const float cost = traversal_cost + ((float)N_L * negchild_surface_area + (float)N_R * poschild_surface_area) * 
						recip_aabb_surf_area * intersection_cost;

			if(cost < axis_smallest_cost[axis])
			{
				axis_smallest_cost[axis] = cost;
				//best_axis = axis;
				axis_best_div_val[axis] = splitval;
				//best_i = i;
			}
		}
	}

	int best_axis = -1;
	float best_div_val;
	for(int axis=0; axis<3; ++axis)
		if(axis_smallest_cost[axis] < smallest_cost)
		{
			smallest_cost = axis_smallest_cost[axis];
			best_axis = axis;
			best_div_val = axis_best_div_val[axis];
		}


	//const int MAX_TIMER_REPORT_DEPTH = 2;
	//if(depth <= MAX_TIMER_REPORT_DEPTH)
	//	conPrint("SAH eval time: " + toString(timer.getSecondsElapsed()) + " s");

	if(best_axis == -1)
	{
		markLeafNode(tris, node_index, left, right);

		// Update build stats
		num_cheaper_nosplit_leaves++;
		leaf_depth_sum += depth;
		max_leaf_depth = myMax(max_leaf_depth, depth);
		return;
	}

	//timer.reset();
	// Now we need to partition the triangle index lists, while maintaining ordering.
	int split_i;
	for(int axis=0; axis<3; ++axis)
	{
		std::vector<TRI_INDEX>& axis_tris = tris[axis];

		int num_left_tris = 0;
		int num_right_tris = 0;

		for(int i=left; i<right; ++i)
		{
			if(tri_centers[axis_tris[i]][best_axis]/*triCenter(tris[axis][i], best_axis)*/ <= best_div_val) // If on Left side
				temp[0][num_left_tris++] = axis_tris[i];
			else // else if on Right side
				temp[1][num_right_tris++] = axis_tris[i];
		}

		// Copy temp back to the tri list

		for(int z=0; z<num_left_tris; ++z)
			axis_tris[left + z] = temp[0][z];

		for(int z=0; z<num_right_tris; ++z)
			axis_tris[left + num_left_tris + z] = temp[1][z];

		split_i = left + num_left_tris;
		assert(split_i >= left && split_i <= right);
	}

	//if(depth <= MAX_TIMER_REPORT_DEPTH)
	//	conPrint("Partition time: " + toString(timer.getSecondsElapsed()) + " s");

	//timer.reset();

	// Compute AABBs for children
	SSE_ALIGN AABBox left_aabb(
		Vec3f(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()),
		Vec3f(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity())
		);

	SSE_ALIGN AABBox right_aabb(
		Vec3f(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity()),
		Vec3f(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity())
		);

	for(int i=left; i<split_i; ++i)
	{
		//SSE_ALIGN AABBox tri_aabb;
		//triAABB(tris[0][i], tri_aabb);

		left_aabb.enlargeToHoldAABBox(tri_aabbs[tris[0][i]]);
	}

	for(int i=split_i; i<right; ++i)
	{
		//SSE_ALIGN AABBox tri_aabb;
		//triAABB(tris[0][i], tri_aabb);

		right_aabb.enlargeToHoldAABBox(tri_aabbs[tris[0][i]]);
	}

	//if(depth <= MAX_TIMER_REPORT_DEPTH)
	//	conPrint("child AABB construction time: " + toString(timer.getSecondsElapsed()) + " s");

	const unsigned int left_child_index = num_nodes;
	const unsigned int right_child_index = left_child_index + 1;

	nodes[node_index].setLeftAABB(left_aabb);
	nodes[node_index].setRightAABB(right_aabb);
	nodes[node_index].setLeaf(false);
	nodes[node_index].setLeftChildIndex(left_child_index);
	nodes[node_index].setRightChildIndex(right_child_index);

	// Reserve space for children
	const unsigned int new_num_nodes = num_nodes + 2;
	if(new_num_nodes > nodes_capacity)
	{
		nodes_capacity *= 2;
		SimpleBVHNode* new_nodes;
		alignedArrayMalloc(nodes_capacity, SimpleBVHNode::requiredAlignment(), new_nodes); // Alloc new array
		if(new_nodes == NULL)
			throw TreeExcep("Memory allocation failure.");
		memcpy(new_nodes, nodes, sizeof(SimpleBVHNode) * num_nodes); // Copy over old array
		alignedArrayFree(nodes); // Free old array
		nodes = new_nodes; // Update nodes pointer to point at new array
	}
	num_nodes = new_num_nodes; // Update size

	// Recurse to build left subtree
	doBuild(left_aabb, tris, temp, tri_centers, left, split_i, left_child_index, depth+1);

	// Recurse to build right subtree
	doBuild(right_aabb, tris, temp, tri_centers, split_i, right, right_child_index, depth+1);
}


bool SimpleBVH::doesFiniteRayHit(const ::Ray& ray, double raylength, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object) const
{
	//NOTE: can speed this up
	HitInfo hitinfo;
	const double dist = traceRay(ray, raylength, thread_context, context, object, hitinfo);
	return dist >= 0.0 && dist < raylength;
}


double SimpleBVH::traceRay(const Ray& ray, double ray_max_t, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const
{
	assertSSEAligned(&ray);
	assert(ray.unitDir().isUnitLength());
	assert(ray_max_t >= 0.0);

	hitinfo_out.sub_elem_index = 0;
	hitinfo_out.sub_elem_coords.set(0.0, 0.0);

	float aabb_enterdist, aabb_exitdist;
	if(root_aabb->rayAABBTrace(ray.startPosF(), ray.getRecipRayDirF(), aabb_enterdist, aabb_exitdist) == 0)
		return -1.0; // Ray missed aabbox
	assert(aabb_enterdist <= aabb_exitdist);

	const float root_t_min = myMax(0.0f, aabb_enterdist);
	const float root_t_max = myMin((float)ray_max_t, aabb_exitdist);
	if(root_t_min > root_t_max)
		return -1.0; // Ray interval does not intersect AABB

	float closest_dist = std::numeric_limits<float>::infinity();

	context.nodestack[0] = StackFrame(0, root_t_min, root_t_max);

	int stacktop = 0; // Index of node on top of stack

	const __m128 raystartpos = _mm_load_ps(&ray.startPosF().x);
	const __m128 inv_dir = _mm_load_ps(&ray.getRecipRayDirF().x);

	while(stacktop >= 0)
	{
		// Pop node off stack
		unsigned int current = context.nodestack[stacktop].node;
		__m128 tmin = _mm_load_ss(&context.nodestack[stacktop].tmin);
		__m128 tmax = _mm_load_ss(&context.nodestack[stacktop].tmax);

		tmax = _mm_min_ps(tmax, _mm_load_ss(&closest_dist));

		stacktop--;

		while(nodes[current].getLeaf() == 0)
		{
			const unsigned int left = nodes[current].getLeftChildIndex();
			const unsigned int right = nodes[current].getRightChildIndex();

			const __m128 a = _mm_load_ps(nodes[current].box);
			const __m128 b = _mm_load_ps(nodes[current].box + 4);
			const __m128 c = _mm_load_ps(nodes[current].box + 8);

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
			assert(temp[0] == nodes[current].box[0] && temp[1] == nodes[current].box[4] && temp[2] == nodes[current].box[8]);
			_mm_store_ps(temp, box_max);
			assert(temp[0] == nodes[current].box[1] && temp[1] == nodes[current].box[5] && temp[2] == nodes[current].box[9]);
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
			left_near_t = _mm_max_ps(left_near_t, tmin);
			left_far_t = _mm_min_ps(left_far_t, tmax);

			// Test against right child
			__m128 right_near_t, right_far_t;
			{
			// a = [rmax.x, rmin.x, lmax.x, lmin.x]
			// b = [rmax.y, rmin.y, lmax.y, lmin.y]
			// c = [rmax.z, rmin.z, lmax.z, lmin.z]
			__m128 
			box_min = _mm_shuffle_ps(a, b,			_MM_SHUFFLE(2, 2, 2, 2)); // box_min = [rmin.y, rmin.y, rmin.x, rmin.x]
			box_min = _mm_shuffle_ps(box_min, c,	_MM_SHUFFLE(2, 2, 2, 0)); // box_min = [rmin.z, rmin.z, rmin.x, rmin.x]
			__m128 
			box_max = _mm_shuffle_ps(a, b,			_MM_SHUFFLE(3, 3, 3, 3)); // box_max = [rmax.y, rmax.y, rmax.x, rmax.x]
			box_max = _mm_shuffle_ps(box_max, c,	_MM_SHUFFLE(3, 3, 2, 0)); // box_max = [rmax.z, rmax.z, rmax.y, rmax.x]

			#ifdef DEBUG
			SSE_ALIGN float temp[4];
			_mm_store_ps(temp, box_min);
			assert(temp[0] == nodes[current].box[2] && temp[1] == nodes[current].box[6] && temp[2] == nodes[current].box[10]);
			_mm_store_ps(temp, box_max);
			assert(temp[0] == nodes[current].box[3] && temp[1] == nodes[current].box[7] && temp[2] == nodes[current].box[11]);
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
			right_near_t = _mm_max_ps(right_near_t, tmin);
			right_far_t = _mm_min_ps(right_far_t, tmax);

			if(_mm_comile_ss(right_near_t, right_far_t) != 0) // If ray hits right AABB
			{
				if(_mm_comile_ss(left_near_t, left_far_t) != 0) // If ray hits left AABB
				{
					// Push right child onto stack
					stacktop++;
					assert(stacktop < context.nodestack_size);
					context.nodestack[stacktop].node = right;
					_mm_store_ss(&context.nodestack[stacktop].tmin, right_near_t);
					_mm_store_ss(&context.nodestack[stacktop].tmax, right_far_t);

					current = left; tmin = left_near_t; tmax = left_far_t; // next = L
				}
				else // Else ray missed left AABB, so process right child next
				{
					current = right; tmin = right_near_t; tmax = right_far_t; // next = R
				}
			}
			else // Else ray misssed right AABB
			{
				if(_mm_comile_ss(left_near_t, left_far_t) != 0) // If ray hits left AABB
				{
					current = left; tmin = left_near_t; tmax = left_far_t; // next = L
				}
				else
				{
					// ray missed both child AABBs.  So break to popping node off stack
					goto after_tri_test;
				}
			}
		}

		// At this point, the current node is a leaf.
		assert(nodes[current].getLeaf() != 0);
		
		// Test against leaf triangles

		unsigned int leaf_geom_index = nodes[current].getGeomIndex();
		const unsigned int num_leaf_tris = nodes[current].getNumGeom();

		for(unsigned int i=0; i<num_leaf_tris; ++i)
		{
			float u, v, raydist;
			if(intersect_tris[leaf_geom_index].rayIntersect(ray, closest_dist, raydist, u, v))
			{
				closest_dist = raydist;
				hitinfo_out.sub_elem_index = leaf_geom_index;
				hitinfo_out.sub_elem_coords.set(u, v);
			}
			++leaf_geom_index;
		}
after_tri_test:
		int dummy = 7; // dummy statement
	}

	if(closest_dist < std::numeric_limits<float>::infinity())
		return closest_dist;
	else
		return -1.0f; // Missed all tris
}


void SimpleBVH::getAllHits(const Ray& ray, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	hitinfos_out.resize(0);
}


const js::AABBox& SimpleBVH::getAABBoxWS() const
{
	return *root_aabb;
}


const Vec3f& SimpleBVH::triGeometricNormal(unsigned int tri_index) const //slow
{
	return intersect_tris[tri_index].getNormal();
}


} //end namespace js
