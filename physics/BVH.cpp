/*=====================================================================
BVH.cpp
-------
File created by ClassTemplate on Sun Oct 26 17:19:14 2008
Code By Nicholas Chapman.
=====================================================================*/
#include "BVH.h"


#include "jscol_aabbox.h"
#include "../indigo/globals.h"
#include "../utils/timer.h"
#include "../simpleraytracer/raymesh.h"
#include "TreeUtils.h"
#include "BVHImpl.h"
#include "MollerTrumboreTri.h"
#include "../indigo/PrintOutput.h"
#include "../indigo/ThreadContext.h"
#include "../indigo/DistanceHitInfo.h"
#include "../utils/Sort.h"


namespace js
{


BVH::BVH(RayMesh* raymesh_)
:	raymesh(raymesh_),
	tri_aabbs(NULL)
{
	assert(raymesh);

	/*BVHNode node;
	node.setLeftChildIndex(45654);
	node.setRightChildIndex(456456);

	assert(node.getLeftChildIndex() == 45654);
	assert(node.getRightChildIndex() == 456456);

	node.setToInterior();
	node.setLeftToLeaf();
	node.setRightToLeaf();
	
	node.setLeftGeomIndex(3);
	node.setLeftNumGeom(7);
	node.setRightGeomIndex(567);
	node.setRightNumGeom(4565);

	
	assert(node.isLeftLeaf());
	assert(node.isRightLeaf());
	assert(node.getLeftGeomIndex() == 3);
	assert(node.getLeftNumGeom() == 7);
	assert(node.getRightGeomIndex() == 567);
	assert(node.getRightNumGeom() == 4565);*/

	num_maxdepth_leaves = 0;
	num_under_thresh_leaves = 0;
	num_leaves = 0;
	max_num_tris_per_leaf = 0;
	leaf_depth_sum = 0;
	max_leaf_depth = 0;
	num_cheaper_nosplit_leaves = 0;
	num_could_not_split_leaves = 0;

	assert(sizeof(BVHNode) == 64);
}


BVH::~BVH()
{
	assert(tri_aabbs == NULL);
}


const Vec3f& BVH::triVertPos(unsigned int tri_index, unsigned int vert_index_in_tri) const
{
	return raymesh->triVertPos(tri_index, vert_index_in_tri);
}


unsigned int BVH::numTris() const
{
	return raymesh->getNumTris();
}


/*
		root: left geom index, left num geom, right = not used

or

		root left index, right index
			/					\
	left child					right child
*/


class CenterPredicate
{
public:
	CenterPredicate(int axis_, const std::vector<Vec3f>& tri_centers_) : axis(axis_), tri_centers(tri_centers_) {}

	inline bool operator()(unsigned int t1, unsigned int t2)
	{
		return tri_centers[t1][axis] < tri_centers[t2][axis];
	}
private:
	int axis;
	const std::vector<Vec3f>& tri_centers;
};


class CenterKey
{
public:
	CenterKey(int axis_, const std::vector<Vec3f>& tri_centers_) : axis(axis_), tri_centers(tri_centers_) {}

	inline float operator()(uint32 i)
	{
		return tri_centers[i][axis];
	}
private:
	int axis;
	const std::vector<Vec3f>& tri_centers;
};


static inline void convertPos(const Vec3f& p, Vec4f& pos_out)
{
	pos_out.x[0] = p.x;
	pos_out.x[1] = p.y;
	pos_out.x[2] = p.z;
	pos_out.x[3] = 1.0f;
}


void BVH::build(PrintOutput& print_output, bool verbose)
{
	if(verbose) print_output.print("\tBVH::build()");
	
	try
	{
		const int num_tris = (int)numTris();

		TreeUtils::buildRootAABB(*raymesh, root_aabb, print_output, verbose);
		assert(root_aabb.invariant());

		//original_tri_index.resize(num_tris);
		//new_tri_index.resize(num_tris);

		{

		// Build tri AABBs
		SSE::alignedSSEArrayMalloc(numTris(), tri_aabbs);

		for(unsigned int i=0; i<numTris(); ++i)
		{
			Vec4f p;
			convertPos(triVertPos(i, 0), p);
			tri_aabbs[i].min_ = p;
			tri_aabbs[i].max_ = p;

			convertPos(triVertPos(i, 1), p);
			tri_aabbs[i].enlargeToHoldPoint(p);

			convertPos(triVertPos(i, 2), p);
			tri_aabbs[i].enlargeToHoldPoint(p);
		}

		// Build tri centers
		std::vector<Vec3f> tri_centers(numTris());

		// Had to disable this for mac because gcc 4.2 is too aids to do
		// openmp in pthreads as per bug...
		//		http://gcc.gnu.org/bugzilla/show_bug.cgi?id=36242
		#ifndef OSX
		#pragma omp parallel for
		#endif
		for(int i=0; i<num_tris; ++i)
			tri_centers[i] = toVec3f(tri_aabbs[i].centroid());

		std::vector<std::vector<TRI_INDEX> > tris(3);
		std::vector<std::vector<TRI_INDEX> > temp(2);
		temp[0].resize(numTris());
		temp[1].resize(numTris());

		// Sort indices based on center position along the axes
		if(verbose) print_output.print("\tSorting...");
		Timer sort_timer;

		#ifndef OSX
		#pragma omp parallel for
		#endif
		for(int axis=0; axis<3; ++axis)
		{
			tris[axis].resize(numTris());
			for(unsigned int i=0; i<numTris(); ++i)
				tris[axis][i] = i;

			// Sort based on center along axis 'axis'

			// Use std sort for arrays of less than 320 items, because std sort is faster for smaller arrays.
			if(tris[axis].size() < 320)
				std::sort<std::vector<unsigned int>::iterator, CenterPredicate>(tris[axis].begin(), tris[axis].end(), CenterPredicate(axis, tri_centers));
			else
				Sort::radixSort<uint32, CenterKey>(&(*tris[axis].begin()), tris[axis].size(), CenterKey(axis, tri_centers));

			if(false) // Enable this code to check the triangle indices are sorted.
			{
				for(size_t i=1; i<tris[axis].size(); ++i)
				{
					if(tri_centers[tris[axis][i]][axis] <tri_centers[tris[axis][i-1]][axis])
					{
						print_output.print("Error: not sorted!");	
						exit(1);
					}
				}
			}
		}
		if(verbose) print_output.print("\t\tDone (" + toString(sort_timer.getSecondsElapsed()) + " s).");

		// In the best case, Each leaf node has pointers to 4 tris for left AABB, and 4 tris for right AABB, for a total of 8 tris.
		// So there are N / 8 leaf nodes, or N / 4 total nodes.
		nodes.reserve(numTris() / 4);
		nodes.resize(1);

		this->tri_max.resize(numTris());
		//this->tri_max = new std::vector<float>(numTris());

		// Make root node
		nodes[0].setToInterior();
		nodes[0].setLeftAABB(root_aabb);
		AABBox rootaabb;
		rootaabb.min_.set(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), 1.0f);
		rootaabb.max_ = rootaabb.min_;
		nodes[0].setRightAABB(rootaabb); // Pick an AABB that will never get hit
		nodes[0].setRightToLeaf();
		nodes[0].setRightNumGeom(0);

		doBuild(
			root_aabb, // AABB
			tris,
			temp,
			tri_centers,
			0, // left
			numTris(), // right
			0, // depth
			0, // parent index
			0 // child index (=left)
			);
		}

		// Free triangle AABB array.
		SSE::alignedSSEArrayFree(tri_aabbs);
		tri_aabbs = NULL;

		// Free tri_max
		//delete this->tri_max;
		//this->tri_max = NULL;
		this->tri_max.clearAndFreeMem();

		//------------------------------------------------------------------------
		//alloc intersect tri array
		//------------------------------------------------------------------------
		if(verbose) print_output.print("\tAllocating intersect triangles...");
		
		intersect_tris.resize(numTris());

		assert(intersect_tris.size() == intersect_tris.capacity());

		if(verbose)
			print_output.print("\t\tDone.  Intersect_tris mem usage: " + ::getNiceByteSize(intersect_tris.size() * sizeof(INTERSECT_TRI_TYPE)));

		// Copy tri data.
		for(unsigned int i=0; i<intersect_tris.size(); ++i)
			intersect_tris[i].set(triVertPos(i, 0), triVertPos(i, 1), triVertPos(i, 2));

		this->tree_specific_min_t = TreeUtils::getTreeSpecificMinT(this->root_aabb);

		if(verbose)
		{
			print_output.print("\tBuild Stats:");
			print_output.print("\t\tTotal nodes used: " + ::toString((unsigned int)nodes.size()) + " (" + ::getNiceByteSize(nodes.size() * sizeof(BVHNode)) + ")");
			print_output.print("\t\tTotal nodes capacity: " + ::toString((unsigned int)nodes.capacity()) + " (" + ::getNiceByteSize(nodes.capacity() * sizeof(BVHNode)) + ")");
			print_output.print("\t\tTotal tri indices used: " + ::toString((unsigned int)leafgeom.size()) + " (" + ::getNiceByteSize(leafgeom.size() * sizeof(TRI_INDEX)) + ")");
			print_output.print("\t\tNum sub threshold leaves: " + toString(num_under_thresh_leaves));
			print_output.print("\t\tNum max depth leaves: " + toString(num_maxdepth_leaves));
			print_output.print("\t\tNum cheaper no-split leaves: " + toString(num_cheaper_nosplit_leaves));
			print_output.print("\t\tNum could not split leaves: " + toString(num_could_not_split_leaves));
			print_output.print("\t\tMean tris per leaf: " + toString((float)numTris() / (float)num_leaves));
			print_output.print("\t\tMax tris per leaf: " + toString(max_num_tris_per_leaf));
			print_output.print("\t\tMean leaf depth: " + toString((float)leaf_depth_sum / (float)num_leaves));
			print_output.print("\t\tMax leaf depth: " + toString(max_leaf_depth));

			print_output.print("\tFinished building tree.");
		}
	}
	catch(std::bad_alloc& e)
	{
		throw TreeExcep(e.what());
	}
}


void BVH::markLeafNode(/*BVHNode* nodes, */unsigned int parent_index, unsigned int child_index, int left, int right, const std::vector<std::vector<TRI_INDEX> >& tris)
{
	// 8 => 8
	// 7 => 8
	// 6 => 8
	// 5 => 8
	// 4 => 4
	const int num_tris = myMax(right - left, 0);
	const int num_4tris = ((num_tris % 4) == 0) ? (num_tris / 4) : (num_tris / 4) + 1;
	const int num_tris_rounded_up = num_4tris * 4;
	const int num_padding = num_tris_rounded_up - num_tris;

	assert(num_tris + num_padding == num_tris_rounded_up);
	assert(num_tris_rounded_up % 4 == 0);

	if(num_4tris > (int)BVHNode::maxNumGeom())
		throw TreeExcep("BVH Build failure, too many triangles in leaf.");

	if(child_index == 0)
	{
		nodes[parent_index].setLeftToLeaf();
		nodes[parent_index].setLeftGeomIndex((unsigned int)leafgeom.size()); //intersect_tri_i);
		nodes[parent_index].setLeftNumGeom(num_4tris);
	}
	else
	{
		nodes[parent_index].setRightToLeaf();
		nodes[parent_index].setRightGeomIndex((unsigned int)leafgeom.size()); // intersect_tri_i);
		nodes[parent_index].setRightNumGeom(num_4tris);
	}

	for(int i=left; i<right; ++i)
	{
		const TRI_INDEX source_tri = tris[0][i];
		/*assert(intersect_tri_i < num_intersect_tris);
		original_tri_index[intersect_tri_i] = source_tri;
		new_tri_index[source_tri] = intersect_tri_i;
		intersect_tris[intersect_tri_i++].set(triVertPos(source_tri, 0), triVertPos(source_tri, 1), triVertPos(source_tri, 2));*/
		leafgeom.push_back(source_tri);
	}

	for(int i=0; i<num_padding; ++i)
		leafgeom.push_back(leafgeom.back());

	// Update build stats
	max_num_tris_per_leaf = myMax(max_num_tris_per_leaf, right - left);
	num_leaves++;
}


/*
The triangles [left, right) are considered to belong to this node.
The AABB for this node (build_nodes[node_index]) should be set already.
*/
void BVH::doBuild(const AABBox& aabb, std::vector<std::vector<TRI_INDEX> >& tris, std::vector<std::vector<TRI_INDEX> >& temp, 
		const std::vector<Vec3f>& tri_centers, int left, int right, int depth, unsigned int parent_index, unsigned int child_index)
{
	//assert(left >= 0 && left < (int)numTris() && right >= 0 && right < (int)numTris());
	//assert(node_index < build_nodes.size());

	const int LEAF_NUM_TRI_THRESHOLD = 4;

	const int MAX_DEPTH = js::Tree::MAX_TREE_DEPTH;

	if(right - left <= LEAF_NUM_TRI_THRESHOLD || depth >= MAX_DEPTH)
	{
		markLeafNode(/*nodes, */parent_index, child_index, left, right, tris);

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

	// Compute non-split cost
	float smallest_cost = (float)(right - left) * intersection_cost;

	int best_N_L = -1;
	int best_axis = -1;
	float best_div_val;
	
	// for each axis 0..2
	for(unsigned int axis=0; axis<3; ++axis)
	{
		float last_split_val = -std::numeric_limits<float>::infinity();

		const std::vector<TRI_INDEX>& axis_tris = tris[axis];

		// SAH stuff
		const unsigned int axis1 = nonsplit_axes[axis][0];
		const unsigned int axis2 = nonsplit_axes[axis][1];
		const float two_cap_area = (aabb.axisLength(axis1) * aabb.axisLength(axis2)) * 2.0f;
		const float circum = (aabb.axisLength(axis1) + aabb.axisLength(axis2)) * 2.0f;

		// Go from left to right, Building the current max bound seen so far for tris 0...i
		float running_max = -std::numeric_limits<float>::infinity();
		for(int i=left; i<right; ++i)
		{
			running_max = myMax(running_max, tri_aabbs[axis_tris[i]].max_[axis]);
			(tri_max)[i-left] = running_max;
		}

		// For Each triangle centroid
		float running_min = std::numeric_limits<float>::infinity();
		for(int i=right-2; i>=left; --i)
		{
			const float splitval = tri_centers[axis_tris[i]][axis];
			if(splitval != last_split_val) // If this is the first such split position seen.
			{
				running_min = myMin(running_min, tri_aabbs[axis_tris[i]].min_[axis]);
				const float current_max = (tri_max)[i-left];

				// Compute the SAH cost at the centroid position
				const int N_L = (i - left) + 1;
				const int N_R = (right - left) - N_L;

				// Compute SAH cost
				const float negchild_surface_area = two_cap_area + (current_max - aabb.min_[axis]) * circum;
				const float poschild_surface_area = two_cap_area + (aabb.max_[axis] - running_min) * circum;

				const float cost = traversal_cost + ((float)N_L * negchild_surface_area + (float)N_R * poschild_surface_area) * 
							recip_aabb_surf_area * intersection_cost;

				if(cost < smallest_cost) // axis_smallest_cost[axis])
				{
					best_N_L = N_L;
					smallest_cost = cost;
					best_axis = axis;
					best_div_val = splitval;
				}
				last_split_val = splitval;
			}
		}
	}

	if(best_axis == -1)
	{
		// If the least cost is to not split the node, then make this node a leaf node
		markLeafNode(parent_index, child_index, left, right, tris);

		// Update build stats
		num_cheaper_nosplit_leaves++;
		leaf_depth_sum += depth;
		max_leaf_depth = myMax(max_leaf_depth, depth);
		return;
	}

	// Now we need to partition the triangle index lists, while maintaining ordering.
	int split_i;
	for(int axis=0; axis<3; ++axis)
	{
		std::vector<TRI_INDEX>& axis_tris = tris[axis];

		int num_left_tris = 0;
		int num_right_tris = 0;

		for(int i=left; i<right; ++i)
		{
			if(tri_centers[axis_tris[i]][best_axis] <= best_div_val) // If on Left side
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
		//TEMP DISABLED WAS FAILING assert(num_left_tris == best_N_L);

		if(num_left_tris == 0 || num_left_tris == right - left)
		{
			markLeafNode(parent_index, child_index, left, right, tris);
			// Update build stats
			num_could_not_split_leaves++;
			leaf_depth_sum += depth;
			max_leaf_depth = myMax(max_leaf_depth, depth);
			return;
		}
	}


	// Compute AABBs for children
	AABBox left_aabb;
	left_aabb.min_.set(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), 1.0f);
	left_aabb.max_.set(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1.0f);

	AABBox right_aabb;
	right_aabb.min_.set(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), 1.0f);
	right_aabb.max_.set(-std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity(), 1.0f);

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

	
	// Alloc space for this node
	const unsigned int node_index = nodes.size();
	nodes.resize(nodes.size() + 1);

	//assert(node_index < num_nodes);
	nodes[node_index].setLeftAABB(left_aabb);
	nodes[node_index].setRightAABB(right_aabb);
	nodes[node_index].setToInterior(); // may be overridden later

	// Update link from parent to this node
	if(child_index == 0)
		nodes[parent_index].setLeftChildIndex(node_index);
	else
		nodes[parent_index].setRightChildIndex(node_index);

	// Recurse to build left subtree
	doBuild(left_aabb, tris, temp, tri_centers, left, split_i, depth+1, node_index, 0);

	// Recurse to build right subtree
	doBuild(right_aabb, tris, temp, tri_centers, split_i, right, depth+1, node_index, 1);
}


/*static inline void updateVals(
	const BVH& bvh, unsigned int leaf_geom_index,
	const Vec4& u,
	const Vec4& v,
	const Vec4& t,
	const Vec4& hit,
	HitInfo& hitinfo_out
	)
{
}*/


class TraceRayFunctions
{
public:
	inline static bool testAgainstTriangles(const BVH& bvh, unsigned int leaf_geom_index, unsigned int num_leaf_tris, 
		const Ray& ray,
		//float use_min_t,
		float epsilon,
		HitInfo& hitinfo_out,
		float& best_t,
		ThreadContext& thread_context,
		const Object* object,
		unsigned int ignore_tri
		)
	{
		for(unsigned int i=0; i<num_leaf_tris; ++i)
		{
			const float* const t0 = bvh.intersect_tris[bvh.leafgeom[leaf_geom_index + 0]].data;
			const float* const t1 = bvh.intersect_tris[bvh.leafgeom[leaf_geom_index + 1]].data;
			const float* const t2 = bvh.intersect_tris[bvh.leafgeom[leaf_geom_index + 2]].data;
			const float* const t3 = bvh.intersect_tris[bvh.leafgeom[leaf_geom_index + 3]].data;

			UnionVec4 u, v, t, hit;
			MollerTrumboreTri::intersectTris(&ray, epsilon, t0, t1, t2, t3, 
				&u, &v, &t, &hit
				);

			if((hit.i[0] != 0) && t.f[0] < best_t && (!object || object->isNonNullAtHit(thread_context, ray, (double)t.f[0], bvh.leafgeom[leaf_geom_index + 0], u.f[0], v.f[0])))
			{
				best_t = t.f[0];
				hitinfo_out.sub_elem_index = bvh.leafgeom[leaf_geom_index + 0];
				hitinfo_out.sub_elem_coords.set(u.f[0], v.f[0]);
			}
			if((hit.i[1] != 0) && t.f[1] < best_t && (!object || object->isNonNullAtHit(thread_context, ray, (double)t.f[1], bvh.leafgeom[leaf_geom_index + 1], u.f[1], v.f[1])))
			{
				best_t = t.f[1];
				hitinfo_out.sub_elem_index = bvh.leafgeom[leaf_geom_index + 1];
				hitinfo_out.sub_elem_coords.set(u.f[1], v.f[1]);
			}
			if((hit.i[2] != 0) && t.f[2] < best_t && (!object || object->isNonNullAtHit(thread_context, ray, (double)t.f[2], bvh.leafgeom[leaf_geom_index + 2], u.f[2], v.f[2])))
			{
				best_t = t.f[2];
				hitinfo_out.sub_elem_index = bvh.leafgeom[leaf_geom_index + 2];
				hitinfo_out.sub_elem_coords.set(u.f[2], v.f[2]);
			}
			if((hit.i[3] != 0) && t.f[3] < best_t && (!object || object->isNonNullAtHit(thread_context, ray, (double)t.f[3], bvh.leafgeom[leaf_geom_index + 3], u.f[3], v.f[3])))
			{
				best_t = t.f[3];
				hitinfo_out.sub_elem_index = bvh.leafgeom[leaf_geom_index + 3];
				hitinfo_out.sub_elem_coords.set(u.f[3], v.f[3]);
			}

			leaf_geom_index += 4;
		}

		return false; // Don't early out from BVHImpl::traceRay()
	}
};


BVH::Real BVH::traceRay(const Ray& ray, Real ray_max_t, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri, HitInfo& hitinfo_out) const
{
	return BVHImpl::traceRay<TraceRayFunctions>(*this, ray, ray_max_t, 
		thread_context, 
		thread_context.getTreeContext(),
		object, 
		ignore_tri,
		hitinfo_out
		);
}


class DoesFiniteRayHitFunctions
{
public:
	inline static bool testAgainstTriangles(const BVH& bvh, unsigned int leaf_geom_index, unsigned int num_leaf_tris, 
		const Ray& ray,
		//float use_min_t,
		float epsilon,
		HitInfo& hitinfo_out,
		float& best_t,
		ThreadContext& thread_context,
		const Object* object,
		unsigned int ignore_tri
		)
	{
		for(unsigned int i=0; i<num_leaf_tris; ++i)
		{
			const float* const t0 = bvh.intersect_tris[bvh.leafgeom[leaf_geom_index + 0]].data;
			const float* const t1 = bvh.intersect_tris[bvh.leafgeom[leaf_geom_index + 1]].data;
			const float* const t2 = bvh.intersect_tris[bvh.leafgeom[leaf_geom_index + 2]].data;
			const float* const t3 = bvh.intersect_tris[bvh.leafgeom[leaf_geom_index + 3]].data;

			UnionVec4 u, v, t, hit;
			MollerTrumboreTri::intersectTris(&ray, /*use_min_t, */epsilon, t0, t1, t2, t3, 
				&u, &v, &t, &hit
				);

			if((hit.i[0] != 0) && t.f[0] < best_t && (!object || object->isNonNullAtHit(thread_context, ray, (double)t.f[0], bvh.leafgeom[leaf_geom_index + 0], u.f[0], v.f[0])) && bvh.leafgeom[leaf_geom_index + 0] != ignore_tri)
				return true;
			if((hit.i[1] != 0) && t.f[1] < best_t && (!object || object->isNonNullAtHit(thread_context, ray, (double)t.f[1], bvh.leafgeom[leaf_geom_index + 1], u.f[1], v.f[1])) && bvh.leafgeom[leaf_geom_index + 1] != ignore_tri)
				return true;
			if((hit.i[2] != 0) && t.f[2] < best_t && (!object || object->isNonNullAtHit(thread_context, ray, (double)t.f[2], bvh.leafgeom[leaf_geom_index + 2], u.f[2], v.f[2])) && bvh.leafgeom[leaf_geom_index + 2] != ignore_tri)
				return true;
			if((hit.i[3] != 0) && t.f[3] < best_t && (!object || object->isNonNullAtHit(thread_context, ray, (double)t.f[3], bvh.leafgeom[leaf_geom_index + 3], u.f[3], v.f[3])) && bvh.leafgeom[leaf_geom_index + 3] != ignore_tri)
				return true;

			leaf_geom_index += 4;
		}

		return false; // Don't early out from BVHImpl::traceRay()
	}
};


bool BVH::doesFiniteRayHit(const ::Ray& ray, Real raylength, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri) const
{
	HitInfo hitinfo;
	const double t = BVHImpl::traceRay<DoesFiniteRayHitFunctions>(*this, ray, raylength, 
		thread_context, 
		thread_context.getTreeContext(), // context, 
		object,
		ignore_tri,
		hitinfo
	);
	return t >= 0.0;
}


inline static void recordHit(float t, float u, float v, unsigned int tri_index, std::vector<DistanceHitInfo>& hitinfos_out)
{
	bool already_got_hit = false;
	for(unsigned int z=0; z<hitinfos_out.size(); ++z)
		if(hitinfos_out[z].sub_elem_index == tri_index)//if tri index is the same
			already_got_hit = true;

	if(!already_got_hit)
	{
		hitinfos_out.push_back(DistanceHitInfo(
			tri_index,
			HitInfo::SubElemCoordsType(u, v),
			t
			));
	}
}


class GetAllHitsFunctions
{
public:
	inline static bool testAgainstTriangles(const BVH& bvh, unsigned int leaf_geom_index, unsigned int num_leaf_tris, 
		const Ray& ray,
		float use_min_t,
		std::vector<DistanceHitInfo>& hitinfos_out,
		float& best_t,
		ThreadContext& thread_context,
		const Object* object,
		unsigned int ignore_tri
		)
	{
		for(unsigned int i=0; i<num_leaf_tris; ++i)
		{
			const float* const t0 = bvh.intersect_tris[bvh.leafgeom[leaf_geom_index + 0]].data;
			const float* const t1 = bvh.intersect_tris[bvh.leafgeom[leaf_geom_index + 1]].data;
			const float* const t2 = bvh.intersect_tris[bvh.leafgeom[leaf_geom_index + 2]].data;
			const float* const t3 = bvh.intersect_tris[bvh.leafgeom[leaf_geom_index + 3]].data;

			UnionVec4 u, v, t, hit;
			MollerTrumboreTri::intersectTris(&ray, use_min_t, t0, t1, t2, t3, 
				&u, &v, &t, &hit
				);

			if((hit.i[0] != 0) && t.f[0] < best_t && (!object || object->isNonNullAtHit(thread_context, ray, (double)t.f[0], bvh.leafgeom[leaf_geom_index + 0], u.f[0], v.f[0])))
				recordHit(t.f[0], u.f[0], v.f[0], bvh.leafgeom[leaf_geom_index + 0], hitinfos_out);

			if((hit.i[1] != 0) && t.f[1] < best_t && (!object || object->isNonNullAtHit(thread_context, ray, (double)t.f[1], bvh.leafgeom[leaf_geom_index + 1], u.f[1], v.f[1])))
				recordHit(t.f[1], u.f[1], v.f[1], bvh.leafgeom[leaf_geom_index + 1], hitinfos_out);
			
			if((hit.i[2] != 0) && t.f[2] < best_t && (!object || object->isNonNullAtHit(thread_context, ray, (double)t.f[2], bvh.leafgeom[leaf_geom_index + 2], u.f[2], v.f[2])))
				recordHit(t.f[2], u.f[2], v.f[2], bvh.leafgeom[leaf_geom_index + 2], hitinfos_out);
			
			if((hit.i[3] != 0) && t.f[3] < best_t && (!object || object->isNonNullAtHit(thread_context, ray, (double)t.f[3], bvh.leafgeom[leaf_geom_index + 3], u.f[3], v.f[3])))
				recordHit(t.f[3], u.f[3], v.f[3], bvh.leafgeom[leaf_geom_index + 3], hitinfos_out);

			leaf_geom_index += 4;
		}

		return false; // Don't early out from BVHImpl::traceRay()
	}
};


void BVH::getAllHits(const Ray& ray, ThreadContext& thread_context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	hitinfos_out.resize(0);

	BVHImpl::traceRay<GetAllHitsFunctions>(*this, ray, std::numeric_limits<float>::max(), thread_context, 
		thread_context.getTreeContext(), // context, 
		object, 
		std::numeric_limits<unsigned int>::max(), // ignore_tri
		hitinfos_out
	);
}


const js::AABBox& BVH::getAABBoxWS() const
{
	return root_aabb;
}


/*const Vec3f BVH::triGeometricNormal(unsigned int tri_index) const //slow
{
	//return intersect_tris[new_tri_index[tri_index]].getNormal();
	return intersect_tris[tri_index].getNormal();
}*/


} //end namespace js
