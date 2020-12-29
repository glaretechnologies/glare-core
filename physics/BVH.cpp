/*=====================================================================
BVH.cpp
-------
Copyright Glare Technologies Limited 2015 -
File created by ClassTemplate on Sun Oct 26 17:19:14 2008
=====================================================================*/
#include "BVH.h"


#include "BVHImpl.h"
#include "BinningBVHBuilder.h"
#include "SBVHBuilder.h"
#include "jscol_aabbox.h"
//#include "../indigo/ThreadContext.h"
#include "../indigo/DistanceHitInfo.h"
#include "../simpleraytracer/raymesh.h"
#include "../physics/jscol_boundingsphere.h"
#include "../utils/PrintOutput.h"
#include "../utils/Timer.h"


namespace js
{


BVH::BVH(const RayMesh* const raymesh_)
:	raymesh(raymesh_)
{
	assert(raymesh);
	
	static_assert(sizeof(BVHNode) == 64, "sizeof(BVHNode) == 64");
}


BVH::~BVH()
{
}


// Throws Indigo::CancelledException if cancelled.
void BVH::build(PrintOutput& print_output, ShouldCancelCallback& should_cancel_callback, bool verbose, Indigo::TaskManager& task_manager)
{
	if(verbose) print_output.print("\tBVH::build()");
	Timer timer;

	const RayMesh::TriangleVectorType& raymesh_tris = raymesh->getTriangles();
	const size_t raymesh_tris_size = raymesh_tris.size();
	const RayMesh::VertexVectorType& raymesh_verts = raymesh->getVertices();

	BVHBuilderRef builder;

	const bool USE_SBVH = false;
	if(USE_SBVH)
	{
		js::Vector<BVHBuilderTri, 64> sbvh_tris(raymesh_tris_size);

		for(size_t i=0; i<raymesh_tris_size; ++i)
		{
			const RayMeshTriangle& tri = raymesh_tris[i];

			BVHBuilderTri t;
			t.v[0] = raymesh_verts[tri.vertex_indices[0]].pos.toVec4fPoint();
			t.v[1] = raymesh_verts[tri.vertex_indices[1]].pos.toVec4fPoint();
			t.v[2] = raymesh_verts[tri.vertex_indices[2]].pos.toVec4fPoint();
			sbvh_tris[i] = t;
		}

		builder = new SBVHBuilder(
			4, // leaf_num_object_threshold.  Since we are intersecting against 4 tris at once, as soon as we get down to 4 tris, make a leaf.
			BVHNode::maxNumGeom(), // max_num_objects_per_leaf
			60, // max depth
			1.f, // intersection_cost
			sbvh_tris.data(),
			(int)raymesh_tris_size
		);
	}
	else
	{
		// Make our BVH use BinningBVHBuilder.  This is because our BVH (as opposed to Embree's) is only used when the number of triangles is large, and in that case we want to use the binning builder so that we don't run out of mem.
		Reference<BinningBVHBuilder> binning_builder = new BinningBVHBuilder(
			4, // leaf_num_object_threshold.  Since we are intersecting against 4 tris at once, as soon as we get down to 4 tris, make a leaf.
			BVHNode::maxNumGeom(), // max_num_objects_per_leaf
			60, // max_depth
			4.f, // intersection_cost
			(int)raymesh_tris_size
		);
		builder = binning_builder;

		for(size_t i=0; i<raymesh_tris_size; ++i)
		{
			const RayMeshTriangle& tri = raymesh_tris[i];
			const Vec4f v0 = raymesh_verts[tri.vertex_indices[0]].pos.toVec4fPoint();
			const Vec4f v1 = raymesh_verts[tri.vertex_indices[1]].pos.toVec4fPoint();
			const Vec4f v2 = raymesh_verts[tri.vertex_indices[2]].pos.toVec4fPoint();
			js::AABBox tri_aabb(v0, v0);
			tri_aabb.enlargeToHoldPoint(v1);
			tri_aabb.enlargeToHoldPoint(v2);

			binning_builder->setObjectAABB(i, tri_aabb);
		}
	}
	
	js::Vector<ResultNode, 64> result_nodes;
	builder->build(
		task_manager,
		should_cancel_callback,
		print_output,
		verbose,
		result_nodes
	);
	const BVHBuilder::ResultObIndicesVec& result_ob_indices = builder->getResultObjectIndices();


	root_aabb = builder->getRootAABB();

	// Make leafgeom from leaf object indices.
	leafgeom.reserve(result_ob_indices.size());

	if(result_nodes.size() == 1)
	{
		// Special case where the root node is a leaf.  
		// Make root node, Assign triangles to left 'child'/AABB.
		nodes.resize(1);
		nodes[0].setToInterior();
		nodes[0].setLeftAABB(root_aabb);

		// 8 => 8
		// 7 => 8
		// 6 => 8
		// 5 => 8
		// 4 => 4
		const int begin = result_nodes[0].left;
		const int end   = result_nodes[0].right;
		const int num_tris = end - begin;
		assert(num_tris >= 0);
		const int num_4tris = Maths::roundedUpDivide(num_tris, 4);
		const int num_tris_rounded_up = num_4tris * 4;
		const int num_padding = num_tris_rounded_up - num_tris;

		assert(num_tris + num_padding == num_tris_rounded_up);
		assert(num_tris_rounded_up % 4 == 0);
		assert(num_4tris <= (int)BVHNode::maxNumGeom());

		nodes[0].setLeftToLeaf();
		nodes[0].setLeftGeomIndex((unsigned int)leafgeom.size());
		nodes[0].setLeftNumGeom(num_4tris);

		for(int i=begin; i<end; ++i)
			leafgeom.push_back(result_ob_indices[i]);

		// Add padding if needed
		for(int i=0; i<num_padding; ++i)
		{
			const uint32 back_tri_i = leafgeom.back();
			leafgeom.push_back(back_tri_i);
		}


		// Set the right AABB to something that will never get hit, assign 0 tris to it.
		AABBox right_aabb;
		right_aabb.min_.set(std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), std::numeric_limits<float>::infinity(), 1.0f);
		right_aabb.max_ = right_aabb.min_;
		nodes[0].setRightAABB(right_aabb); // Pick an AABB that will never get hit
		nodes[0].setRightToLeaf();
		nodes[0].setRightNumGeom(0);
	}
	else
	{
		//Timer timer2;

		// Convert result_nodes to BVHNodes.
		// Indices will change, since we will not explicitly store leaf nodes in the BVH.  Rather leaf geometry references are put into the child references of the node above.
		const size_t result_nodes_size = result_nodes.size();
		js::Vector<int, 16> new_node_indices(result_nodes_size); // For each old node, store the new index.
		int new_index = 0;
		for(size_t i=0; i<result_nodes_size; ++i)
			if(result_nodes[i].interior)
				new_node_indices[i] = new_index++;

		const int new_num_nodes = new_index;
		this->nodes.resizeNoCopy(new_num_nodes);

		new_index = 0;
		for(size_t i=0; i<result_nodes_size; ++i)
		{
			const ResultNode& result_node = result_nodes[i];
			if(result_node.interior)
			{
				BVHNode& node = this->nodes[new_index++];
				node.setToInterior();

				const ResultNode& left_child  = result_nodes[result_node.left];
				const ResultNode& right_child = result_nodes[result_node.right];

				node.setLeftAABB(left_child.aabb);
				node.setRightAABB(right_child.aabb);

				// Set node.child[0] and node.child[1]
				if(left_child.interior)
				{
					node.setLeftChildIndex(new_node_indices[result_node.left]);
					assert(node.getLeftChildIndex() >= 0 && (int)node.getLeftChildIndex() < new_num_nodes);
				}
				else
				{
					// 8 => 8
					// 7 => 8
					// 6 => 8
					// 5 => 8
					// 4 => 4
					const int begin = left_child.left;
					const int end   = left_child.right;
					const int num_tris = end - begin;
					assert(num_tris > 0);
					const int num_4tris = Maths::roundedUpDivide(num_tris, 4);
					const int num_tris_rounded_up = num_4tris * 4;
					assert(num_4tris <= (int)BVHNode::maxNumGeom());

					const size_t leaf_geom_size = leafgeom.size();

					node.setLeftToLeaf();
					node.setLeftGeomIndex((unsigned int)leaf_geom_size);
					node.setLeftNumGeom(num_4tris);

					leafgeom.resize(leaf_geom_size + num_tris_rounded_up);

					for(int z=0; z<num_tris; ++z)
						leafgeom[leaf_geom_size + z] = result_ob_indices[begin + z];

					// Pad with repeated copies of last actual tri index.
					for(int z=num_tris; z<num_tris_rounded_up; ++z)
						leafgeom[leaf_geom_size + z] = result_ob_indices[end-1];
				}

				if(right_child.interior)
				{
					node.setRightChildIndex(new_node_indices[result_node.right]);
					assert(node.getRightChildIndex() >= 0 && (int)node.getRightChildIndex() < new_num_nodes);
				}
				else
				{
					const int begin = right_child.left;
					const int end   = right_child.right;
					const int num_tris = end - begin;
					assert(num_tris > 0);
					const int num_4tris = Maths::roundedUpDivide(num_tris, 4);
					const int num_tris_rounded_up = num_4tris * 4;
					assert(num_4tris <= (int)BVHNode::maxNumGeom());

					const size_t leaf_geom_size = leafgeom.size();

					node.setRightToLeaf();
					node.setRightGeomIndex((unsigned int)leafgeom.size());
					node.setRightNumGeom(num_4tris);

					leafgeom.resize(leaf_geom_size + num_tris_rounded_up);

					for(int z=0; z<num_tris; ++z)
						leafgeom[leaf_geom_size + z] = result_ob_indices[begin + z];

					// Pad with repeated copies of last actual tri index.
					for(int z=num_tris; z<num_tris_rounded_up; ++z)
						leafgeom[leaf_geom_size + z] = result_ob_indices[end-1];
				}
			}
		}
	}

	
	//------------------------------------------------------------------------
	//alloc intersect tri array
	//------------------------------------------------------------------------
	if(verbose) print_output.print("\tAllocating intersect triangles...");
		
	intersect_tris.resize(raymesh_tris_size);

	assert(intersect_tris.size() == intersect_tris.capacity());

	if(verbose)
		print_output.print("\t\tDone.  Intersect_tris mem usage: " + ::getNiceByteSize(intersect_tris.size() * sizeof(INTERSECT_TRI_TYPE)));

	// Copy tri data.
	for(size_t i=0; i<raymesh_tris_size; ++i)
	{
		const RayMeshTriangle& tri = raymesh_tris[i];
		intersect_tris[i].set(raymesh_verts[tri.vertex_indices[0]].pos, raymesh_verts[tri.vertex_indices[1]].pos, raymesh_verts[tri.vertex_indices[2]].pos);
	}

	// conPrint("BVH build done.  Elapsed: " + timer.elapsedStringNPlaces(5));
}


class TraceRayFunctions
{
public:
	inline static bool testAgainstTriangles(const BVH& bvh, unsigned int leaf_geom_index, unsigned int num_leaf_tris, 
		const Ray& ray,
		//float use_min_t,
		float epsilon,
		HitInfo& hitinfo_out,
		float& best_t
		//ThreadContext& thread_context
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

			if((hit.i[0] != 0) && t.f[0] < best_t)
			{
				best_t = t.f[0];
				hitinfo_out.sub_elem_index = bvh.leafgeom[leaf_geom_index + 0];
				hitinfo_out.sub_elem_coords.set(u.f[0], v.f[0]);
			}
			if((hit.i[1] != 0) && t.f[1] < best_t)
			{
				best_t = t.f[1];
				hitinfo_out.sub_elem_index = bvh.leafgeom[leaf_geom_index + 1];
				hitinfo_out.sub_elem_coords.set(u.f[1], v.f[1]);
			}
			if((hit.i[2] != 0) && t.f[2] < best_t)
			{
				best_t = t.f[2];
				hitinfo_out.sub_elem_index = bvh.leafgeom[leaf_geom_index + 2];
				hitinfo_out.sub_elem_coords.set(u.f[2], v.f[2]);
			}
			if((hit.i[3] != 0) && t.f[3] < best_t)
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


BVH::DistType BVH::traceRay(const Ray& ray, HitInfo& hitinfo_out) const
{
	//hitinfo_out.hit_opaque_ob = false; // Just consider the hit as non-opaque.  This is the conservative option.

	//ThreadContext& thread_context = *ThreadContext::getThreadLocalContext();

	return BVHImpl::traceRay<TraceRayFunctions>(*this, ray,
		//thread_context,
		//thread_context.getTreeContext(),
		hitinfo_out
	);
}


BVH::DistType BVH::traceSphere(const Ray& ray_ws, const Matrix4f& to_object, const Matrix4f& to_world, float radius_ws, Vec4f& hit_normal_ws_out) const
{
	//ThreadContext& thread_context = *ThreadContext::getThreadLocalContext();

	const Vec4f start_ws = ray_ws.startPos();
	const Vec4f end_ws   = ray_ws.pointf(ray_ws.maxT());

	// Compute the path traced by the sphere in object space.
	// To do this we will compute the start and end path AABBs in world space, transform those to object space, then take the union of the object-space end AABBs.
	const js::AABBox start_aabb_ws(start_ws - Vec4f(radius_ws, radius_ws, radius_ws, 0), start_ws + Vec4f(radius_ws, radius_ws, radius_ws, 0));
	const js::AABBox end_aabb_ws  (end_ws   - Vec4f(radius_ws, radius_ws, radius_ws, 0), end_ws   + Vec4f(radius_ws, radius_ws, radius_ws, 0));

	const js::AABBox start_aabb_os = start_aabb_ws.transformedAABB(to_object);
	const js::AABBox end_aabb_os =   end_aabb_ws  .transformedAABB(to_object);

	const js::AABBox spherepath_aabb_os = AABBUnion(start_aabb_os, end_aabb_os);

	const SSE_ALIGN unsigned int mask[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0 };
	const __m128 maskWToZero = _mm_load_ps((const float*)mask);

	float closest_dist_ws = ray_ws.maxT(); // This is the distance along the ray to the minimum of the closest hit so far and the maximum length of the ray

	//uint32* const bvh_stack = thread_context.getTreeContext().bvh_stack;
	uint32 bvh_stack[js::Tree::MAX_TREE_DEPTH + 1];
	bvh_stack[0] = 0;
	int stacktop = 0; // Index of node on top of stack
stack_pop:
	while(stacktop >= 0)
	{
		// Pop node off stack
		int current = bvh_stack[stacktop];
		stacktop--;

		// While current is indexing a suitable internal node...
		while(current >= 0)
		{
			_mm_prefetch((const char*)(&nodes[0] + nodes[current].getLeftChildIndex()), _MM_HINT_T0);
			_mm_prefetch((const char*)(&nodes[0] + nodes[current].getRightChildIndex()), _MM_HINT_T0);

			const __m128 a = _mm_load_ps(nodes[current].box);
			const __m128 b = _mm_load_ps(nodes[current].box + 4);
			const __m128 c = _mm_load_ps(nodes[current].box + 8);

			// Test ray against left child
			int left_disjoint;
			{
				// a = [rmax.x, rmin.x, lmax.x, lmin.x]
				// b = [rmax.y, rmin.y, lmax.y, lmin.y]
				// c = [rmax.z, rmin.z, lmax.z, lmin.z]
				__m128
					box_min = _mm_shuffle_ps(a, b, _MM_SHUFFLE(0, 0, 0, 0)); // box_min = [lmin.y, lmin.y, lmin.x, lmin.x]
					box_min = _mm_shuffle_ps(box_min, c, _MM_SHUFFLE(0, 0, 2, 0)); // box_min = [lmin.z, lmin.z, lmin.y, lmin.x]
				__m128
					box_max = _mm_shuffle_ps(a, b, _MM_SHUFFLE(1, 1, 1, 1)); // box_max = [lmax.y, lmax.y, lmax.x, lmax.x]
					box_max = _mm_shuffle_ps(box_max, c, _MM_SHUFFLE(1, 1, 2, 0)); // box_max = [lmax.z, lmax.z, lmax.y, lmax.x]

				// Test if sphere path AABB is disjoint with this child:
				__m128 lower_separation = _mm_cmplt_ps(spherepath_aabb_os.max_.v, box_min); // [spherepath.max.x < leftchild.min.x, spherepath.max.y < leftchild.min.y, ...]
				__m128 upper_separation = _mm_cmplt_ps(box_max, spherepath_aabb_os.min_.v);// [leftchild.max.x < spherepath.min.x, leftchild.max.y < spherepath.min.y, ...]
				__m128 either = _mm_and_ps(_mm_or_ps(lower_separation, upper_separation), maskWToZero); // Will have a bit set if there is a separation on any axis
				left_disjoint = _mm_movemask_ps(either); // Creates a 4-bit mask from the most significant bits
			}

			// Test against right child
			int right_disjoint;
			{
				__m128
					box_min = _mm_shuffle_ps(a, b, _MM_SHUFFLE(2, 2, 2, 2)); // box_min = [rmin.y, rmin.y, rmin.x, rmin.x]
					box_min = _mm_shuffle_ps(box_min, c, _MM_SHUFFLE(2, 2, 2, 0)); // box_min = [rmin.z, rmin.z, rmin.x, rmin.x]
				__m128
					box_max = _mm_shuffle_ps(a, b, _MM_SHUFFLE(3, 3, 3, 3)); // box_max = [rmax.y, rmax.y, rmax.x, rmax.x]
					box_max = _mm_shuffle_ps(box_max, c, _MM_SHUFFLE(3, 3, 2, 0)); // box_max = [rmax.z, rmax.z, rmax.y, rmax.x]

				// Test if sphere path AABB is disjoint with this child:
				__m128 lower_separation = _mm_cmplt_ps(spherepath_aabb_os.max_.v, box_min);
				__m128 upper_separation = _mm_cmplt_ps(box_max, spherepath_aabb_os.min_.v);
				__m128 either = _mm_and_ps(_mm_or_ps(lower_separation, upper_separation), maskWToZero); // Will have a bit set if there is a separation on any axis
				right_disjoint = _mm_movemask_ps(either); // Creates a 4-bit mask from the most significant bits
			}

			int left_geom_index = 0;
			int left_num_geom = 0;
			int right_geom_index = 0;
			int right_num_geom = 0;
			if(left_disjoint == 0) // If hit left child:
			{
				if(right_disjoint == 0) // If hit right child also:
				{
					// TODO: push further object on stack instead of just right child.
					
					if(nodes[current].isRightLeaf()) // If right 'child' is a leaf:
					{
						// Intersect with tris in right leaf.
						right_geom_index = nodes[current].getRightGeomIndex();
						right_num_geom = nodes[current].getRightNumGeom();
					}
					else
					{
						stacktop++;
						assert(stacktop < js::Tree::MAX_TREE_DEPTH + 1);
						bvh_stack[stacktop] = nodes[current].getRightChildIndex(); // Push right child onto stack.
					}

					if(nodes[current].isLeftLeaf()) // If left 'child' is a leaf:
					{
						// Intersect with tris in left leaf.
						left_geom_index = nodes[current].getLeftGeomIndex();
						left_num_geom = nodes[current].getLeftNumGeom();
						current = -1; // intersect tris then pop node
					}
					else
					{
						current = nodes[current].getLeftChildIndex(); // Traverse to left child
					}
				}
				else // Else not hit right, so just hit left.  Traverse to left child.
				{
					if(nodes[current].isLeftLeaf())
					{
						// Intersect with tris in left leaf.
						left_geom_index = nodes[current].getLeftGeomIndex();
						left_num_geom = nodes[current].getLeftNumGeom();
						current = -1; // intersect tris then pop node
					}
					else
					{
						current = nodes[current].getLeftChildIndex(); // Traverse to left child						
					}
				}
			}
			else // Else if not hit left:
			{
				if(right_disjoint == 0) // If hit right child:
				{
					if(nodes[current].isRightLeaf())
					{
						// Intersect with tris in right leaf.
						right_geom_index = nodes[current].getRightGeomIndex();
						right_num_geom = nodes[current].getRightNumGeom();
						current = -1; // intersect tris then pop node
					}
					else
					{
						current = nodes[current].getRightChildIndex(); // Traverse to right child.
					}
				}
				else
					goto stack_pop; // Hit zero children, pop node off stack
			}

			intersectSphereAgainstLeafTris(ray_ws, to_world, radius_ws, left_num_geom,  left_geom_index,  closest_dist_ws, hit_normal_ws_out);
			intersectSphereAgainstLeafTris(ray_ws, to_world, radius_ws, right_num_geom, right_geom_index, closest_dist_ws, hit_normal_ws_out);

			// We may have hit a triangle, which will have decreased closest_dist.  So recompute the sphere path AABB.
			//TEMP endpos = ray.startPos() + ray.unitDir() * closest_dist;
			//TEMP spherepath_aabb = js::AABBox(min(ray.startPos(), endpos) - Vec4f(radius, radius, radius, 0), max(ray.startPos(), endpos) + Vec4f(radius, radius, radius, 0));
		}
	}


	if(closest_dist_ws < ray_ws.maxT())
		return closest_dist_ws;
	else
		return -1.0f; // Missed all tris
}


// Adapted from RaySphere::traceRay().
static inline float traceRayAgainstSphere(const Vec4f& ray_origin, const Vec4f& ray_unitdir, const Vec4f& sphere_centre, float radius)
{
	// We are using a numerically robust ray-sphere intersection algorithm as described here: http://www.cg.tuwien.ac.at/courses/CG1/textblaetter/englisch/10%20Ray%20Tracing%20(engl).pdf

	const Vec4f raystart_to_centre = sphere_centre - ray_origin;

	const float r2 = radius*radius;

	// Return zero if ray origin is inside sphere.
	if(raystart_to_centre.length2() <= r2)
		return 0.f;

	const float u_dot_del_p = dot(raystart_to_centre, ray_unitdir);

	const float discriminant = r2 - raystart_to_centre.getDist2(ray_unitdir * u_dot_del_p);

	if(discriminant < 0)
		return -1; // No intersection.

	const float sqrt_discriminant = std::sqrt(discriminant);

	const float t_0 = u_dot_del_p - sqrt_discriminant; // t_0 is the smaller of the two solutions.
	return t_0;
}


void BVH::intersectSphereAgainstLeafTris(const Ray& ray_ws, const Matrix4f& to_world, float radius_ws, 
	int num_geom, int geom_index, float& closest_dist_ws, Vec4f& hit_normal_ws_out) const
{
	const Vec4f sourcePoint3_ws(ray_ws.startPos());
	const Vec4f unitdir3_ws(ray_ws.unitDir());

	for(int i=0; i<num_geom; ++i)
	{
		for(int z=0; z<4; ++z)
		{
			const MollerTrumboreTri& tri = intersect_tris[leafgeom[geom_index + z]];

			const Vec4f v0_os(tri.data[0], tri.data[1], tri.data[2], 0.f); // W-coord should be 1, but can leave as zero due to using mul3Point() below.
			const Vec4f e1_os(tri.data[3], tri.data[4], tri.data[5], 0.f);
			const Vec4f e2_os(tri.data[6], tri.data[7], tri.data[8], 0.f);

			const Vec4f v0_ws = to_world.mul3Point(v0_os);
			const Vec4f e1_ws = to_world.mul3Vector(e1_os);
			const Vec4f e2_ws = to_world.mul3Vector(e2_os);

			// The rest of the intersection will take place in world space.
			const Vec4f normal = normalise(crossProduct(e1_ws, e2_ws));

			const js::Triangle js_tri(v0_ws, e1_ws, e2_ws, normal);

			const Planef tri_plane(v0_ws, normal);

			// Determine the distance from the plane to the sphere center
			float pDist = tri_plane.signedDistToPoint(sourcePoint3_ws);

			//-----------------------------------------------------------------
			//Invert normal if doing backface collision, so 'usenormal' is always facing
			//towards sphere center.
			//-----------------------------------------------------------------
			Vec4f usenormal = normal;
			if(pDist < 0)
			{
				usenormal = -usenormal;
				pDist = -pDist;
			}

			assert(pDist >= 0);

			//-----------------------------------------------------------------
			//check if sphere is heading away from tri
			//-----------------------------------------------------------------
			const float approach_rate = -dot(usenormal, unitdir3_ws);
			if(approach_rate <= 0)
				continue;

			assert(approach_rate > 0);

			// trans_len_needed = dist to approach / dist approached per unit translation len
			const float trans_len_needed = (pDist - radius_ws) / approach_rate;

			if(closest_dist_ws < trans_len_needed)
				continue; // then sphere will never get to plane

			//-----------------------------------------------------------------
			//calc the point where the sphere intersects with the triangle plane (planeIntersectionPoint)
			//-----------------------------------------------------------------
			Vec4f planeIntersectionPoint;

			// Is the plane embedded in the sphere?
			if(trans_len_needed <= 0)//pDist <= sphere.getRadius())//make == trans_len_needed < 0
			{
				// Calculate the plane intersection point
				planeIntersectionPoint = tri_plane.closestPointOnPlane(sourcePoint3_ws);

			}
			else
			{
				assert(trans_len_needed >= 0);

				planeIntersectionPoint = sourcePoint3_ws + (unitdir3_ws * trans_len_needed) - (usenormal * radius_ws);

				//assert point is actually on plane
				//assert(epsEqual(tri.getTriPlane().signedDistToPoint(planeIntersectionPoint), 0.0f, 0.0001f));
			}

			//-----------------------------------------------------------------
			//now restrict collision point on tri plane to inside tri if neccessary.
			//-----------------------------------------------------------------
			Vec4f triIntersectionPoint = planeIntersectionPoint;

			const bool point_in_tri = js_tri.pointInTri(triIntersectionPoint);
			float dist; // Distance until sphere hits triangle
			if(point_in_tri)
			{
				dist = myMax(0.f, trans_len_needed);
			}
			else
			{
				// Restrict to inside tri
				triIntersectionPoint = js_tri.closestPointOnTriangle(triIntersectionPoint);

				// Using the triIntersectionPoint, we need to reverse-intersect with the sphere
				dist = traceRayAgainstSphere(/*ray_origin=*/triIntersectionPoint, /*ray_unitdir=*/-ray_ws.unitDir(), /*sphere_centre=*/ray_ws.startPos(), radius_ws);
			}

			if(dist >= 0 && dist < closest_dist_ws)
			{
				closest_dist_ws = dist;

				//-----------------------------------------------------------------
				//calc hit normal
				//-----------------------------------------------------------------
				if(point_in_tri)
					hit_normal_ws_out = usenormal;
				else
				{
					//-----------------------------------------------------------------
					//calc point sphere will be when it hits edge of tri
					//-----------------------------------------------------------------
					const Vec4f hit_spherecenter = sourcePoint3_ws + unitdir3_ws * dist;

					hit_normal_ws_out = normalise(hit_spherecenter - triIntersectionPoint);
				}
			}
		}

		geom_index += 4;
	}
}


void BVH::appendCollPoints(const Vec4f& sphere_pos_ws, float radius_ws, const Matrix4f& to_object, const Matrix4f& to_world, std::vector<Vec4f>& points_ws_in_out) const
{
	//ThreadContext& thread_context = *ThreadContext::getThreadLocalContext();

	// The BVH traversal will be done in object space, using the object-space bounds of the sphere.  Intersection with the triangles in the BVH leaves will be done in world space.

	const float radius_ws2 = radius_ws*radius_ws;
	const js::AABBox sphere_aabb_ws(sphere_pos_ws - Vec4f(radius_ws, radius_ws, radius_ws, 0), sphere_pos_ws + Vec4f(radius_ws, radius_ws, radius_ws, 0));
	const js::AABBox sphere_aabb_os = sphere_aabb_ws.transformedAABB(to_object);

	const SSE_ALIGN unsigned int mask[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0 };
	const __m128 maskWToZero = _mm_load_ps((const float*)mask);

	//uint32* const bvh_stack = thread_context.getTreeContext().bvh_stack;
	uint32 bvh_stack[js::Tree::MAX_TREE_DEPTH + 1];

	bvh_stack[0] = 0;
	int stacktop = 0; // Index of node on top of stack
stack_pop:
	while(stacktop >= 0)
	{
		// Pop node off stack
		int current = bvh_stack[stacktop];
		stacktop--;

		// While current is indexing a suitable internal node...
		while(current >= 0)
		{
			_mm_prefetch((const char*)(&nodes[0] + nodes[current].getLeftChildIndex()), _MM_HINT_T0);
			_mm_prefetch((const char*)(&nodes[0] + nodes[current].getRightChildIndex()), _MM_HINT_T0);

			const __m128 a = _mm_load_ps(nodes[current].box);
			const __m128 b = _mm_load_ps(nodes[current].box + 4);
			const __m128 c = _mm_load_ps(nodes[current].box + 8);

			// Test ray against left child
			int left_disjoint;
			{
				// a = [rmax.x, rmin.x, lmax.x, lmin.x]
				// b = [rmax.y, rmin.y, lmax.y, lmin.y]
				// c = [rmax.z, rmin.z, lmax.z, lmin.z]
				__m128
					box_min = _mm_shuffle_ps(a, b, _MM_SHUFFLE(0, 0, 0, 0)); // box_min = [lmin.y, lmin.y, lmin.x, lmin.x]
					box_min = _mm_shuffle_ps(box_min, c, _MM_SHUFFLE(0, 0, 2, 0)); // box_min = [lmin.z, lmin.z, lmin.y, lmin.x]
				__m128
					box_max = _mm_shuffle_ps(a, b, _MM_SHUFFLE(1, 1, 1, 1)); // box_max = [lmax.y, lmax.y, lmax.x, lmax.x]
					box_max = _mm_shuffle_ps(box_max, c, _MM_SHUFFLE(1, 1, 2, 0)); // box_max = [lmax.z, lmax.z, lmax.y, lmax.x]

				// Test if sphere path AABB is disjoint with this child:
				__m128 lower_separation = _mm_cmplt_ps(sphere_aabb_os.max_.v, box_min); // [spherepath.max.x < leftchild.min.x, spherepath.max.y < leftchild.min.y, ...]
				__m128 upper_separation = _mm_cmplt_ps(box_max, sphere_aabb_os.min_.v);// [leftchild.max.x < spherepath.min.x, leftchild.max.y < spherepath.min.y, ...]
				__m128 either = _mm_and_ps(_mm_or_ps(lower_separation, upper_separation), maskWToZero); // Will have a bit set if there is a separation on any axis
				left_disjoint = _mm_movemask_ps(either); // Creates a 4-bit mask from the most significant bits
			}

			// Test against right child
			int right_disjoint;
			{
				__m128
					box_min = _mm_shuffle_ps(a, b, _MM_SHUFFLE(2, 2, 2, 2)); // box_min = [rmin.y, rmin.y, rmin.x, rmin.x]
					box_min = _mm_shuffle_ps(box_min, c, _MM_SHUFFLE(2, 2, 2, 0)); // box_min = [rmin.z, rmin.z, rmin.x, rmin.x]
				__m128
					box_max = _mm_shuffle_ps(a, b, _MM_SHUFFLE(3, 3, 3, 3)); // box_max = [rmax.y, rmax.y, rmax.x, rmax.x]
					box_max = _mm_shuffle_ps(box_max, c, _MM_SHUFFLE(3, 3, 2, 0)); // box_max = [rmax.z, rmax.z, rmax.y, rmax.x]

				 // Test if sphere path AABB is disjoint with this child:
				__m128 lower_separation = _mm_cmplt_ps(sphere_aabb_os.max_.v, box_min);
				__m128 upper_separation = _mm_cmplt_ps(box_max, sphere_aabb_os.min_.v);
				__m128 either = _mm_and_ps(_mm_or_ps(lower_separation, upper_separation), maskWToZero); // Will have a bit set if there is a separation on any axis
				right_disjoint = _mm_movemask_ps(either); // Creates a 4-bit mask from the most significant bits
			}

			int left_geom_index = 0;
			int left_num_geom = 0;
			int right_geom_index = 0;
			int right_num_geom = 0;
			if(left_disjoint == 0) // If hit left child:
			{
				if(right_disjoint == 0) // If hit right child also:
				{
					// TODO: push further object on stack instead of just right child.

					if(nodes[current].isRightLeaf()) // If right 'child' is a leaf:
					{
						// Intersect with tris in right leaf.
						right_geom_index = nodes[current].getRightGeomIndex();
						right_num_geom = nodes[current].getRightNumGeom();
					}
					else
					{
						stacktop++;
						assert(stacktop < js::Tree::MAX_TREE_DEPTH + 1);// (int)bvh_stack.size());
						bvh_stack[stacktop] = nodes[current].getRightChildIndex(); // Push right child onto stack.
					}

					if(nodes[current].isLeftLeaf()) // If left 'child' is a leaf:
					{
						// Intersect with tris in left leaf.
						left_geom_index = nodes[current].getLeftGeomIndex();
						left_num_geom = nodes[current].getLeftNumGeom();
						current = -1; // intersect tris then pop node
					}
					else
					{
						current = nodes[current].getLeftChildIndex(); // Traverse to left child
					}
				}
				else // Else not hit right, so just hit left.  Traverse to left child.
				{
					if(nodes[current].isLeftLeaf())
					{
						// Intersect with tris in left leaf.
						left_geom_index = nodes[current].getLeftGeomIndex();
						left_num_geom = nodes[current].getLeftNumGeom();
						current = -1; // intersect tris then pop node
					}
					else
					{
						current = nodes[current].getLeftChildIndex(); // Traverse to left child						
					}
				}
			}
			else // Else if not hit left:
			{
				if(right_disjoint == 0) // If hit right child:
				{
					if(nodes[current].isRightLeaf())
					{
						// Intersect with tris in right leaf.
						right_geom_index = nodes[current].getRightGeomIndex();
						right_num_geom = nodes[current].getRightNumGeom();
						current = -1; // intersect tris then pop node
					}
					else
					{
						current = nodes[current].getRightChildIndex(); // Traverse to right child.
					}
				}
				else
					goto stack_pop; // Hit zero children, pop node off stack
			}

			// Intersect against leaf geometry
			for(int i=0; i<left_num_geom; ++i)
			{
				for(int z=0; z<4; ++z)
				{
					// Get triangle transformed to world-space
					const MollerTrumboreTri& moller_tri = intersect_tris[leafgeom[left_geom_index + z]];

					const Vec4f v0_os(moller_tri.data[0], moller_tri.data[1], moller_tri.data[2], 0.f); // W-coord should be 1, but can leave as zero due to using mul3Point() below.
					const Vec4f e1_os(moller_tri.data[3], moller_tri.data[4], moller_tri.data[5], 0.f);
					const Vec4f e2_os(moller_tri.data[6], moller_tri.data[7], moller_tri.data[8], 0.f);

					const Vec4f v0_ws = to_world.mul3Point(v0_os);
					const Vec4f e1_ws = to_world.mul3Vector(e1_os);
					const Vec4f e2_ws = to_world.mul3Vector(e2_os);

					// The rest of the intersection will take place in world space.
					const Vec4f normal = normalise(crossProduct(e1_ws, e2_ws));
					const js::Triangle tri(v0_ws, e1_ws, e2_ws, normal);

					// See if sphere is touching plane
					const Planef tri_plane(v0_ws, normal);
					const float disttoplane = tri_plane.signedDistToPoint(sphere_pos_ws);
					if(fabs(disttoplane) > radius_ws)
						continue;

					// Get closest point on plane to sphere center
					Vec4f planepoint = tri_plane.closestPointOnPlane(sphere_pos_ws);

					// Restrict point to inside tri
					if(!tri.pointInTri(planepoint))
						planepoint = tri.closestPointOnTriangle(planepoint);

					if(planepoint.getDist2(sphere_pos_ws) <= radius_ws2)
					{
						if(points_ws_in_out.empty() || (planepoint != points_ws_in_out.back())) // HACK: Don't add if same as last point.  May happen due to packing of 4 tris together with possible duplicates.
							points_ws_in_out.push_back(planepoint);
					}
				}

				left_geom_index += 4;
			}
			for(int i=0; i<right_num_geom; ++i)
			{
				for(int z=0; z<4; ++z)
				{
					// Get triangle transformed to world-space
					const MollerTrumboreTri& moller_tri = intersect_tris[leafgeom[right_geom_index + z]];

					const Vec4f v0_os(moller_tri.data[0], moller_tri.data[1], moller_tri.data[2], 0.f); // W-coord should be 1, but can leave as zero due to using mul3Point() below.
					const Vec4f e1_os(moller_tri.data[3], moller_tri.data[4], moller_tri.data[5], 0.f);
					const Vec4f e2_os(moller_tri.data[6], moller_tri.data[7], moller_tri.data[8], 0.f);

					const Vec4f v0_ws = to_world.mul3Point(v0_os);
					const Vec4f e1_ws = to_world.mul3Vector(e1_os);
					const Vec4f e2_ws = to_world.mul3Vector(e2_os);

					// The rest of the intersection will take place in world space.
					const Vec4f normal = normalise(crossProduct(e1_ws, e2_ws));
					const js::Triangle tri(v0_ws, e1_ws, e2_ws, normal);

					// See if sphere is touching plane
					const Planef tri_plane(v0_ws, normal);
					const float disttoplane = tri_plane.signedDistToPoint(sphere_pos_ws);
					if(fabs(disttoplane) > radius_ws)
						continue;

					// Get closest point on plane to sphere center
					Vec4f planepoint = tri_plane.closestPointOnPlane(sphere_pos_ws);

					// Restrict point to inside tri
					if(!tri.pointInTri(planepoint))
						planepoint = tri.closestPointOnTriangle(planepoint);

					if(planepoint.getDist2(sphere_pos_ws) <= radius_ws2)
					{
						if(points_ws_in_out.empty() || (planepoint != points_ws_in_out.back())) // HACK: Don't add if same as last point.  May happen due to packing of 4 tris together with possible duplicates.
							points_ws_in_out.push_back(planepoint);
					}
				}

				right_geom_index += 4;
			}
		}
	}
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
		float& best_t
		//ThreadContext& thread_context
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

			if((hit.i[0] != 0) && t.f[0] < best_t)
				recordHit(t.f[0], u.f[0], v.f[0], bvh.leafgeom[leaf_geom_index + 0], hitinfos_out);

			if((hit.i[1] != 0) && t.f[1] < best_t)
				recordHit(t.f[1], u.f[1], v.f[1], bvh.leafgeom[leaf_geom_index + 1], hitinfos_out);
			
			if((hit.i[2] != 0) && t.f[2] < best_t)
				recordHit(t.f[2], u.f[2], v.f[2], bvh.leafgeom[leaf_geom_index + 2], hitinfos_out);
			
			if((hit.i[3] != 0) && t.f[3] < best_t)
				recordHit(t.f[3], u.f[3], v.f[3], bvh.leafgeom[leaf_geom_index + 3], hitinfos_out);

			leaf_geom_index += 4;
		}

		return false; // Don't early out from BVHImpl::traceRay()
	}
};


void BVH::getAllHits(const Ray& ray, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	//ThreadContext& thread_context = *ThreadContext::getThreadLocalContext();

	hitinfos_out.resize(0);

	BVHImpl::traceRay<GetAllHitsFunctions>(*this, ray, 
		//thread_context,
		//thread_context.getTreeContext(),
		hitinfos_out
	);
}


BVH::Real BVH::traceRayAgainstAllTris(const Ray& ray, Real max_t, HitInfo& hitinfo_out) const
{
	float best_t = 1.0e30f;
	assert((leafgeom.size() % 4) == 0);
	for(size_t z=0; z<leafgeom.size(); z += 4)
	{
		const size_t leaf_geom_index = z;
		const float* const t0 = intersect_tris[leafgeom[leaf_geom_index + 0]].data;
		const float* const t1 = intersect_tris[leafgeom[leaf_geom_index + 1]].data;
		const float* const t2 = intersect_tris[leafgeom[leaf_geom_index + 2]].data;
		const float* const t3 = intersect_tris[leafgeom[leaf_geom_index + 3]].data;

		UnionVec4 u, v, t, hit;
		MollerTrumboreTri::intersectTris(&ray, ray.minT(), t0, t1, t2, t3, 
			&u, &v, &t, &hit
			);

		if((hit.i[0] != 0) && t.f[0] < best_t)
		{
			best_t = t.f[0];
			hitinfo_out.sub_elem_index = leafgeom[leaf_geom_index + 0];
			hitinfo_out.sub_elem_coords.set(u.f[0], v.f[0]);
		}
		if((hit.i[1] != 0) && t.f[1] < best_t)
		{
			best_t = t.f[1];
			hitinfo_out.sub_elem_index = leafgeom[leaf_geom_index + 1];
			hitinfo_out.sub_elem_coords.set(u.f[1], v.f[1]);
		}
		if((hit.i[2] != 0) && t.f[2] < best_t)
		{
			best_t = t.f[2];
			hitinfo_out.sub_elem_index = leafgeom[leaf_geom_index + 2];
			hitinfo_out.sub_elem_coords.set(u.f[2], v.f[2]);
		}
		if((hit.i[3] != 0) && t.f[3] < best_t)
		{
			best_t = t.f[3];
			hitinfo_out.sub_elem_index = leafgeom[leaf_geom_index + 3];
			hitinfo_out.sub_elem_coords.set(u.f[3], v.f[3]);
		}
	}

	return (best_t < 1.0e30f) ? best_t : -1.f;
}


void BVH::getAllHitsAllTris(const Ray& ray, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	hitinfos_out.clear();

	float best_t = 1.0e30f;
	assert((leafgeom.size() % 4) == 0);
	for(size_t z=0; z<leafgeom.size(); z += 4)
	{
		const size_t leaf_geom_index = z;
		const float* const t0 = intersect_tris[leafgeom[leaf_geom_index + 0]].data;
		const float* const t1 = intersect_tris[leafgeom[leaf_geom_index + 1]].data;
		const float* const t2 = intersect_tris[leafgeom[leaf_geom_index + 2]].data;
		const float* const t3 = intersect_tris[leafgeom[leaf_geom_index + 3]].data;

		UnionVec4 u, v, t, hit;
		MollerTrumboreTri::intersectTris(&ray, ray.minT(), t0, t1, t2, t3,
			&u, &v, &t, &hit
		);

		if((hit.i[0] != 0) && t.f[0] < best_t)
			recordHit(t.f[0], u.f[0], v.f[0], leafgeom[leaf_geom_index + 0], hitinfos_out);

		if((hit.i[1] != 0) && t.f[1] < best_t)
			recordHit(t.f[1], u.f[1], v.f[1], leafgeom[leaf_geom_index + 1], hitinfos_out);

		if((hit.i[2] != 0) && t.f[2] < best_t)
			recordHit(t.f[2], u.f[2], v.f[2], leafgeom[leaf_geom_index + 2], hitinfos_out);

		if((hit.i[3] != 0) && t.f[3] < best_t)
			recordHit(t.f[3], u.f[3], v.f[3], leafgeom[leaf_geom_index + 3], hitinfos_out);
	}
}


const js::AABBox& BVH::getAABBox() const
{
	return root_aabb;
}


size_t BVH::getTotalMemUsage() const
{
	return sizeof(root_aabb) + nodes.capacitySizeBytes() + leafgeom.capacitySizeBytes() + intersect_tris.capacity()*sizeof(INTERSECT_TRI_TYPE);
}


/*const Vec3f BVH::triGeometricNormal(unsigned int tri_index) const //slow
{
	//return intersect_tris[new_tri_index[tri_index]].getNormal();
	return intersect_tris[tri_index].getNormal();
}*/


} //end namespace js
