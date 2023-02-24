/*=====================================================================
BVH.cpp
-------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "BVH.h"


#include "BinningBVHBuilder.h"
#include "jscol_boundingsphere.h"
#include "../simpleraytracer/raymesh.h"
#include "../utils/PrintOutput.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


namespace js
{


BVH::BVH(const RayMesh* const raymesh_)
:	raymesh(raymesh_)
{
	assert(raymesh);
	
	static_assert(sizeof(BVHNode) == 64, "sizeof(BVHNode) == 64");
}


BVH::~BVH()
{}


// Throws glare::CancelledException if cancelled.
void BVH::build(PrintOutput& print_output, ShouldCancelCallback& should_cancel_callback, glare::TaskManager& task_manager)
{
	Timer timer;

	const RayMesh::TriangleVectorType& raymesh_tris = raymesh->getTriangles();
	const int raymesh_tris_size = (int)raymesh_tris.size();
	const RayMesh::VertexVectorType& raymesh_verts = raymesh->getVertices();

	Reference<BinningBVHBuilder> builder = new BinningBVHBuilder(
		1, // leaf_num_object_threshold.
		31, // max_num_objects_per_leaf
		64, // max_depth
		1.f, // intersection_cost
		raymesh_tris_size
	);

	for(int i=0; i<raymesh_tris_size; ++i)
	{
		const RayMeshTriangle& tri = raymesh_tris[i];
		const Vec4f v0 = raymesh_verts[tri.vertex_indices[0]].pos.toVec4fPoint();
		const Vec4f v1 = raymesh_verts[tri.vertex_indices[1]].pos.toVec4fPoint();
		const Vec4f v2 = raymesh_verts[tri.vertex_indices[2]].pos.toVec4fPoint();
		js::AABBox tri_aabb(v0, v0);
		tri_aabb.enlargeToHoldPoint(v1);
		tri_aabb.enlargeToHoldPoint(v2);

		builder->setObjectAABB(i, tri_aabb);
	}
	
	js::Vector<ResultNode, 64> result_nodes;
	builder->build(
		task_manager,
		should_cancel_callback,
		print_output,
		result_nodes
	);
	const BVHBuilder::ResultObIndicesVec& result_ob_indices = builder->getResultObjectIndices();

	root_aabb = builder->getRootAABB();

	if(result_nodes.size() == 1)
	{
		// Special case where the root node is a leaf.  
		const int num = result_nodes[0].right - result_nodes[0].left;
		const int offset = result_nodes[0].left;
		assert(num < 32);
		int c = 0x80000000 | num | (offset << 5);
		this->root_node_index = c;
	}
	else
	{
		this->root_node_index = 0;

		// Convert result_nodes to BVHNodes.
		// Indices will change, since we will not explicitly store leaf nodes in the BVH object tree.  Rather leaf geometry references are put into the child references of the node above.
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

				const ResultNode& result_left_child  = result_nodes[result_node.left];
				const ResultNode& result_right_child = result_nodes[result_node.right];

				Vec4f mins = shuffle<0, 1, 0, 1>(result_left_child.aabb.min_, result_right_child.aabb.min_); // (left_min_x, left_min_y, right_min_x, right_min_y)
				Vec4f maxs = shuffle<0, 1, 0, 1>(result_left_child.aabb.max_, result_right_child.aabb.max_); // (left_max_x, left_max_y, right_max_x, right_max_y)

				node.x = shuffle<0, 2, 0, 2>(mins, maxs); // (left_min_x, right_min_x, left_max_x, right_max_x)
				node.y = shuffle<1, 3, 1, 3>(mins, maxs); // (left_min_y, right_min_y, left_max_y, right_max_y)

				mins = shuffle<2, 2, 2, 2>(result_left_child.aabb.min_, result_right_child.aabb.min_); // (left_min_z, left_min_z, right_min_z, right_min_z)
				maxs = shuffle<2, 2, 2, 2>(result_left_child.aabb.max_, result_right_child.aabb.max_); // (left_max_z, left_max_z, right_max_z, right_max_z)

				node.z = shuffle<0, 2, 0, 2>(mins, maxs); // (left_min_z, right_min_z, left_max_z, right_max_z)

				assert(node.x[0] == result_left_child.aabb.min_[0] && node.x[1] == result_right_child.aabb.min_[0] && node.x[2] == result_left_child.aabb.max_[0] && node.x[3] == result_right_child.aabb.max_[0]);
				assert(node.y[0] == result_left_child.aabb.min_[1] && node.y[1] == result_right_child.aabb.min_[1] && node.y[2] == result_left_child.aabb.max_[1] && node.y[3] == result_right_child.aabb.max_[1]);
				assert(node.z[0] == result_left_child.aabb.min_[2] && node.z[1] == result_right_child.aabb.min_[2] && node.z[2] == result_left_child.aabb.max_[2] && node.z[3] == result_right_child.aabb.max_[2]);

				//node.x = Vec4f(result_left_child.aabb.min_.x[0], result_right_child.aabb.min_.x[0], result_left_child.aabb.max_.x[0], result_right_child.aabb.max_.x[0]);
				//node.y = Vec4f(result_left_child.aabb.min_.x[1], result_right_child.aabb.min_.x[1], result_left_child.aabb.max_.x[1], result_right_child.aabb.max_.x[1]);
				//node.z = Vec4f(result_left_child.aabb.min_.x[2], result_right_child.aabb.min_.x[2], result_left_child.aabb.max_.x[2], result_right_child.aabb.max_.x[2]);

				// Set node.child[0] and node.child[1]
				if(result_left_child.interior)
				{
					node.child[0] = new_node_indices[result_node.left];
					assert(node.child[0] >= 0 && node.child[0] < new_num_nodes);
				}
				else
				{
					// Number of objects is in lower 6 bits.  Set sign bit to 1.
					const int num = result_left_child.right - result_left_child.left;
					const int offset = result_left_child.left;
					assert(num < 32);
					int c = 0x80000000 | num | (offset << 5);
					node.child[0] = c;
				}

				if(result_right_child.interior)
				{
					node.child[1] = new_node_indices[result_node.right];
					assert(node.child[1] >= 0 && node.child[1] < new_num_nodes);
				}
				else
				{
					// Number of objects is in lower 6 bits.  Set sign bit to 1.
					const int num = result_right_child.right - result_right_child.left;
					const int offset = result_right_child.left;
					assert(num < 32);
					int c = 0x80000000 | num | (offset << 5);
					node.child[1] = c;
				}
			}
		}
	}

	// Build leaf_tri_indices
	const size_t result_ob_ind_size = result_ob_indices.size();
	leaf_tri_indices.resizeNoCopy(result_ob_ind_size);
	for(size_t i=0; i<result_ob_ind_size; ++i)
		leaf_tri_indices[i] = result_ob_indices[i];

	// We will access the RayMesh tri data directly for now instead of copying to intersect_tris.
	
	/*intersect_tris.resize(raymesh_tris_size);

	assert(intersect_tris.size() == intersect_tris.capacity());

	if(verbose)
		print_output.print("\t\tDone.  Intersect_tris mem usage: " + ::getNiceByteSize(intersect_tris.size() * sizeof(INTERSECT_TRI_TYPE)));

	// Copy tri data.
	for(size_t i=0; i<raymesh_tris_size; ++i)
	{
		const RayMeshTriangle& tri = raymesh_tris[i];
		intersect_tris[i].set(raymesh_verts[tri.vertex_indices[0]].pos, raymesh_verts[tri.vertex_indices[1]].pos, raymesh_verts[tri.vertex_indices[2]].pos);
	}*/

	// conPrint("BVH build done.  Elapsed: " + timer.elapsedStringNPlaces(5));
}


// NOTE: Uses SEE3 instruction _mm_shuffle_epi8.
static GLARE_STRONG_INLINE const Vec4f shuffle8(const Vec4f& a, const Vec4i& shuf) { return _mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128(a.v), shuf.v)); }
static GLARE_STRONG_INLINE const Vec4f vec4XOR(const Vec4f& a, const Vec4i& b) { return _mm_castsi128_ps(_mm_xor_si128(_mm_castps_si128(a.v), b.v)); }


BVH::DistType BVH::traceRay(const Ray& ray_, HitInfo& hitinfo_out) const
{
	Ray ray = ray_;
	HitInfo ob_hit_info;

	int stack[64];
	float dist_stack[64];
	int stack_top = 0;
	stack[0] = root_node_index; // Push root node onto stack
	dist_stack[0] = -std::numeric_limits<float>::infinity(); // Near distances to nodes on the stack.

	const Vec4f r_x = copyToAll<0>(ray.startpos_f); // (r_x, r_x, r_x, r_x)
	const Vec4f r_y = copyToAll<1>(ray.startpos_f); // (r_y, r_y, r_y, r_y)
	const Vec4f r_z = copyToAll<2>(ray.startpos_f); // (r_z, r_z, r_z, r_z)

	const Vec4i negate(0x00000000, 0x00000000, 0x80000000, 0x80000000); // To flip sign bits on floats 2 and 3.
	const Vec4f rdir_x = vec4XOR(copyToAll<0>(ray.recip_unitdir_f), negate); // (1/d_x, 1/d_x, -1/d_x, -1/d_x)
	const Vec4f rdir_y = vec4XOR(copyToAll<1>(ray.recip_unitdir_f), negate); // (1/d_y, 1/d_y, -1/d_y, -1/d_y)
	const Vec4f rdir_z = vec4XOR(copyToAll<2>(ray.recip_unitdir_f), negate); // (1/d_z, 1/d_z, -1/d_z, -1/d_z)

	// Near_far will store current ray segment as (near, near, -far, -far)
	const Vec4f ray_near_far = loadVec4f(&ray.min_t); // (near, far, junk, junk)
	Vec4f near_far = vec4XOR(swizzle<0, 0, 1, 1>(ray_near_far), negate); // (near, near, -far, -far)
	assert(near_far == Vec4f(ray.minT(), ray.minT(), -ray.maxT(), -ray.maxT()));

	const Vec4i identity(_mm_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0));
	const Vec4i swap(_mm_set_epi8(7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8));

	const Vec4i x_shuffle = ray.recip_unitdir_f[0] > 0 ? identity : swap; // TODO: do this without branching
	const Vec4i y_shuffle = ray.recip_unitdir_f[1] > 0 ? identity : swap;
	const Vec4i z_shuffle = ray.recip_unitdir_f[2] > 0 ? identity : swap;

stack_pop:
	while(stack_top >= 0) // While still one or more nodes on the stack:
	{
		int cur = stack[stack_top]; // Pop node off top of stack.
		float popped_node_near = dist_stack[stack_top];
		stack_top--;
		// If current interval far < popped near, then we don't need to process this node.
		// far < popped_near = -far > -popped_near
		if(near_far.x[2] > -popped_node_near)
			goto stack_pop; // Pop another node.

		while(cur >= 0) // While this is a proper interior node:
		{
			const BVHNode& node = nodes[cur];

			// Intersect with the node bounding boxes

			// Get signed distance from ray origin to near and far planes
			// left_near_delta_x = rdir.x > 0 ? node_min_x - r_x : node_max_x - r_x
			const Vec4f near_far_x = shuffle8(node.x, x_shuffle) - r_x; // (left_near_delta_x, right_near_delta_x, left_far_delta_x, right_far_delta_x)
			const Vec4f near_far_y = shuffle8(node.y, y_shuffle) - r_y;
			const Vec4f near_far_z = shuffle8(node.z, z_shuffle) - r_z;

			// Get dist along ray to planes.  Far distances will be negated due to negated rdir components 2 and 3.
			const __m128 minmax_d_x = _mm_mul_ps(near_far_x.v, rdir_x.v); // (min_left_d_x, min_right_d_x, -max_left_d_x, -max_right_d_x)
			const __m128 minmax_d_y = _mm_mul_ps(near_far_y.v, rdir_y.v); // (min_left_d_x, min_right_d_x, -max_left_d_x, -max_right_d_x)
			const __m128 minmax_d_z = _mm_mul_ps(near_far_z.v, rdir_z.v); // (min_left_d_x, min_right_d_x, -max_left_d_x, -max_right_d_x)

			// near_left  = max(near, min_left_d_x, min_left_d_y, min_left_d_z)
			// near_right = max(near, min_right_d_x, min_right_d_y, min_right_d_z)
			// far_left = min(far, max_left_d_x, max_left_d_y, max_left_d_z)
			// -far_left = max(-far, -max_left_d_x, -max_left_d_y, -max_left_d_z)
			const Vec4f new_near_far(_mm_max_ps(_mm_max_ps(minmax_d_x, minmax_d_y), _mm_max_ps(near_far.v, minmax_d_z))); // (near_left, near_right, -far_left, -far_right)
			const Vec4f non_negated_near_far(vec4XOR(new_near_far, negate)); // (near_left, near_right, far_left, far_right)
			const Vec4f shuffled_near_far(shuffle8(non_negated_near_far, swap)); //  // (far_left, far_right, near_left, near_right)
			const Vec4i near_le_far(_mm_castps_si128(_mm_cmple_ps(new_near_far.v, shuffled_near_far.v))); // (near_left <= far_left, near_right <= far_right, ., .)

			if(near_le_far.x[0] != 0) // If hit left
			{
				if(near_le_far.x[1] != 0) // If hit right as well, then ray has hit both:
				{
					if(new_near_far.x[0] < new_near_far.x[1]) // If left child is closer
					{
						// Push right child onto stack
						stack_top++;
						assert(stack_top < 64);
						stack[stack_top] = node.child[1];
						dist_stack[stack_top] = new_near_far.x[1]; // push near_right

						cur = node.child[0];
					}
					else
					{
						// Push left child onto stack
						stack_top++;
						assert(stack_top < 64);
						stack[stack_top] = node.child[0];
						dist_stack[stack_top] = new_near_far.x[0]; // push near_left

						cur = node.child[1];
					}
				}
				else // Else not hit right, so just hit left.  Traverse to left child.
				{
					cur = node.child[0];
				}
			}
			else // Else if not hit left:
			{
				if(near_le_far.x[1] != 0) // If hit right child:
					cur = node.child[1];
				else
					goto stack_pop; // Hit zero children, pop node off stack
			}
		}

		// current node is a leaf.  Intersect objects.
		cur ^= 0x80000000; // Zero sign bit
		const size_t ofs = size_t(cur) >> 5;
		const size_t num = size_t(cur) & 0x1F;
		for(size_t i=ofs; i<ofs+num; i++)
		{
			const uint32 tri_index = leaf_tri_indices[i];

			MollerTrumboreTri tri;
			tri.set(raymesh->triVertPos(tri_index, 0), raymesh->triVertPos(tri_index, 1), raymesh->triVertPos(tri_index, 2)); // TODO: use unaligned loads here for speed etc..

			Real dist;
			if(tri.referenceIntersect(ray, &ob_hit_info.sub_elem_coords.x, &ob_hit_info.sub_elem_coords.y, &dist)) // TODO: use a fast intersect method (SSE)
			{
				if(dist >= ray.minT() && dist < ray.maxT())
				{
					ray.setMaxT(dist);

					hitinfo_out.sub_elem_coords = ob_hit_info.sub_elem_coords;
					hitinfo_out.sub_elem_index = tri_index;

					// Update far to min(dist, far)
					const Vec4f new_neg_far = Vec4f(_mm_max_ps(Vec4f(-dist).v, near_far.v)); // (., ., -min(far, dist), -min(far, dist))
					near_far = _mm_shuffle_ps(near_far.v, new_neg_far.v, _MM_SHUFFLE(3, 2, 1, 0)); // (near, near, -far, -far)
				}
			}
		}
	}

	return (ray.maxT() < ray_.maxT()) ? ray.maxT() : -1.f;
}


BVH::DistType BVH::traceSphere(const Ray& ray_ws_, const Matrix4f& to_object, const Matrix4f& to_world, float radius_ws, Vec4f& hit_pos_ws_out, Vec4f& hit_normal_ws_out, bool& point_in_tri_out) const
{
	Ray ray_ws = ray_ws_;
	const Vec4f start_ws = ray_ws_.startPos();
	const Vec4f end_ws   = ray_ws_.pointf(ray_ws_.maxT());

	// Compute the path traced by the sphere in object space.
	// To do this we will compute the start and end path AABBs in world space, transform those to object space, then take the union of the object-space end AABBs.
	const js::AABBox start_aabb_ws(start_ws - Vec4f(radius_ws, radius_ws, radius_ws, 0), start_ws + Vec4f(radius_ws, radius_ws, radius_ws, 0));
	const js::AABBox end_aabb_ws  (end_ws   - Vec4f(radius_ws, radius_ws, radius_ws, 0), end_ws   + Vec4f(radius_ws, radius_ws, radius_ws, 0));

	const js::AABBox start_aabb_os = start_aabb_ws.transformedAABBFast(to_object);
	const js::AABBox end_aabb_os =   end_aabb_ws  .transformedAABBFast(to_object);

	const js::AABBox spherepath_aabb_os = AABBUnion(start_aabb_os, end_aabb_os);

	int stack[64];
	int stack_top = 0;
	stack[0] = root_node_index; // Push root node onto stack

stack_pop:
	while(stack_top >= 0) // While still one or more nodes on the stack:
	{
		int cur = stack[stack_top--]; // Pop node off top of stack.

		while(cur >= 0) // While this is a proper interior node:
		{
			const BVHNode& node = nodes[cur];

			// Intersect with the node bounding boxes

			const Vec4f xvals = node.x; // (left_min_x, right_min_x, left_max_x, right_max_x)
			const Vec4f yvals = node.y; // (left_min_y, right_min_y, left_max_y, right_max_y)
			const Vec4f zvals = node.z; // (left_min_z, right_min_z, left_max_z, right_max_z)

			Vec4f left_min = shuffle<0, 0, 0, 0>(xvals, yvals); // (left_min_x, left_min_x, left_min_y, left_min_y)
			left_min = shuffle<0, 2, 0, 0>(left_min, zvals);    // (left_min_x, left_min_y, left_min_z, left_min_z)

			Vec4f right_min = shuffle<1, 1, 1, 1>(xvals, yvals); // (right_min_x, right_min_x, right_min_y, right_min_y)
			right_min = shuffle<0, 2, 1, 1>(right_min, zvals);   // (right_min_x, right_min_y, right_min_z, right_min_z)

			Vec4f left_max = shuffle<2, 2, 2, 2>(xvals, yvals); // (left_max_x, left_max_x, left_max_y, left_max_y)
			left_max = shuffle<0, 2, 2, 2>(left_max, zvals);    // (left_max_x, left_max_y, left_max_z, left_max_z)

			Vec4f right_max = shuffle<3, 3, 3, 3>(xvals, yvals); // (right_max_x, right_max_x, right_max_y, right_max_y)
			right_max = shuffle<0, 2, 3, 3>(right_max, zvals);   // (right_max_x, right_max_y, right_max_z, right_max_z)

			assert(xvals[0] == left_min[0] && xvals[1] == right_min[0] && xvals[2] == left_max[0] && xvals[3] == right_max[0]);
			assert(yvals[0] == left_min[1] && yvals[1] == right_min[1] && yvals[2] == left_max[1] && yvals[3] == right_max[1]);
			assert(zvals[0] == left_min[2] && zvals[1] == right_min[2] && zvals[2] == left_max[2] && zvals[3] == right_max[2]);

			const js::AABBox left_aabb (left_min,  left_max);  // Note that W components will be != 1.
			const js::AABBox right_aabb(right_min, right_max); // Note that W components will be != 1.

			const int left_disjoint  = left_aabb .disjoint(spherepath_aabb_os) & 0x7; // Ignore W bit of movemask result
			const int right_disjoint = right_aabb.disjoint(spherepath_aabb_os) & 0x7;

			if(left_disjoint == 0) // If hit left
			{
				if(right_disjoint == 0) // If hit right as well, then ray has hit both:
				{
					// Push left child onto stack
					stack_top++;
					stack[stack_top] = node.child[0];

					cur = node.child[1];
				}
				else // Else not hit right, so just hit left.  Traverse to left child.
				{
					cur = node.child[0];
				}
			}
			else // Else if not hit left:
			{
				if(right_disjoint == 0) // If hit right child:
					cur = node.child[1];
				else
					goto stack_pop; // Hit zero children, pop node off stack
			}
		}

		// current node is a leaf.  Intersect objects.
		cur ^= 0x80000000; // Zero sign bit
		const size_t ofs = size_t(cur) >> 5;
		const size_t num = size_t(cur) & 0x1F;
		for(size_t i=ofs; i<ofs+num; i++)
		{
			const uint32 tri_index = leaf_tri_indices[i];
			intersectSphereAgainstLeafTri(ray_ws, to_world, radius_ws, tri_index, hit_pos_ws_out, hit_normal_ws_out, point_in_tri_out);
		}
	}

	return (ray_ws.maxT() < ray_ws_.maxT()) ? ray_ws.maxT() : -1.f;
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


void BVH::intersectSphereAgainstLeafTri(Ray& ray_ws, const Matrix4f& to_world, float radius_ws, TRI_INDEX tri_index, Vec4f& hit_pos_ws_out, Vec4f& hit_normal_ws_out, bool& point_in_tri_out) const
{
	const Vec4f sourcePoint3_ws(ray_ws.startPos());
	const Vec4f unitdir3_ws(ray_ws.unitDir());

	MollerTrumboreTri tri;
	tri.set(raymesh->triVertPos(tri_index, 0), raymesh->triVertPos(tri_index, 1), raymesh->triVertPos(tri_index, 2));

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
		return;

	assert(approach_rate > 0);

	// trans_len_needed = dist to approach / dist approached per unit translation len
	const float trans_len_needed = (pDist - radius_ws) / approach_rate;

	if(ray_ws.maxT() < trans_len_needed)
		return; // then sphere will never get to plane

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

	if(dist >= 0 && dist < ray_ws.maxT())
	{
		ray_ws.setMaxT(dist);

		hit_pos_ws_out = triIntersectionPoint;
		point_in_tri_out = point_in_tri;

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


void BVH::appendCollPoints(const Vec4f& sphere_pos_ws, float radius_ws, const Matrix4f& to_object, const Matrix4f& to_world, std::vector<Vec4f>& points_ws_in_out) const
{
	const float radius_ws2 = radius_ws*radius_ws;
	const js::AABBox sphere_aabb_ws(sphere_pos_ws - Vec4f(radius_ws, radius_ws, radius_ws, 0), sphere_pos_ws + Vec4f(radius_ws, radius_ws, radius_ws, 0));
	const js::AABBox sphere_aabb_os = sphere_aabb_ws.transformedAABBFast(to_object);

	int stack[64];
	int stack_top = 0;
	stack[0] = root_node_index; // Push root node onto stack

stack_pop:
	while(stack_top >= 0) // While still one or more nodes on the stack:
	{
		int cur = stack[stack_top--]; // Pop node off top of stack.

		while(cur >= 0) // While this is a proper interior node:
		{
			const BVHNode& node = nodes[cur];

			// Intersect with the node bounding boxes

			const Vec4f xvals = node.x; // (left_min_x, right_min_x, left_max_x, right_max_x)
			const Vec4f yvals = node.y; // (left_min_y, right_min_y, left_max_y, right_max_y)
			const Vec4f zvals = node.z; // (left_min_z, right_min_z, left_max_z, right_max_z)

			Vec4f left_min = shuffle<0, 0, 0, 0>(xvals, yvals); // (left_min_x, left_min_x, left_min_y, left_min_y)
			left_min = shuffle<0, 2, 0, 0>(left_min, zvals);    // (left_min_x, left_min_y, left_min_z, left_min_z)

			Vec4f right_min = shuffle<1, 1, 1, 1>(xvals, yvals); // (right_min_x, right_min_x, right_min_y, right_min_y)
			right_min = shuffle<0, 2, 1, 1>(right_min, zvals);   // (right_min_x, right_min_y, right_min_z, right_min_z)

			Vec4f left_max = shuffle<2, 2, 2, 2>(xvals, yvals); // (left_max_x, left_max_x, left_max_y, left_max_y)
			left_max = shuffle<0, 2, 2, 2>(left_max, zvals);    // (left_max_x, left_max_y, left_max_z, left_max_z)

			Vec4f right_max = shuffle<3, 3, 3, 3>(xvals, yvals); // (right_max_x, right_max_x, right_max_y, right_max_y)
			right_max = shuffle<0, 2, 3, 3>(right_max, zvals);   // (right_max_x, right_max_y, right_max_z, right_max_z)

			assert(xvals[0] == left_min[0] && xvals[1] == right_min[0] && xvals[2] == left_max[0] && xvals[3] == right_max[0]);
			assert(yvals[0] == left_min[1] && yvals[1] == right_min[1] && yvals[2] == left_max[1] && yvals[3] == right_max[1]);
			assert(zvals[0] == left_min[2] && zvals[1] == right_min[2] && zvals[2] == left_max[2] && zvals[3] == right_max[2]);

			const js::AABBox left_aabb (left_min,  left_max);  // Note that W components will be != 1.
			const js::AABBox right_aabb(right_min, right_max); // Note that W components will be != 1.

			const int left_disjoint  = left_aabb .disjoint(sphere_aabb_os) & 0x7; // Ignore W bit of movemask result
			const int right_disjoint = right_aabb.disjoint(sphere_aabb_os) & 0x7;

			if(left_disjoint == 0) // If hit left
			{
				if(right_disjoint == 0) // If hit right as well, then ray has hit both:
				{
					// Push left child onto stack
					stack_top++;
					stack[stack_top] = node.child[0];

					cur = node.child[1];
				}
				else // Else not hit right, so just hit left.  Traverse to left child.
				{
					cur = node.child[0];
				}
			}
			else // Else if not hit left:
			{
				if(right_disjoint == 0) // If hit right child:
					cur = node.child[1];
				else
					goto stack_pop; // Hit zero children, pop node off stack
			}
		}

		// current node is a leaf.  Intersect objects.
		cur ^= 0x80000000; // Zero sign bit
		const size_t ofs = size_t(cur) >> 5;
		const size_t num = size_t(cur) & 0x1F;
		for(size_t i=ofs; i<ofs+num; i++)
		{
			const uint32 tri_index = leaf_tri_indices[i];

			MollerTrumboreTri moller_tri;
			moller_tri.set(raymesh->triVertPos(tri_index, 0), raymesh->triVertPos(tri_index, 1), raymesh->triVertPos(tri_index, 2));

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
				points_ws_in_out.push_back(planepoint);
		}
	}
}


const js::AABBox& BVH::getAABBox() const
{
	return root_aabb;
}


size_t BVH::getTotalMemUsage() const
{
	return sizeof(root_aabb) + nodes.capacitySizeBytes() + leaf_tri_indices.capacitySizeBytes();
}


} // end namespace js


#if BUILD_TESTS


#include "../dll/include/IndigoMesh.h"
#include "../dll/include/IndigoException.h"
#include "../dll/IndigoStringUtils.h"
#include "../utils/TestUtils.h"
#include "../maths/PCG32.h"
#include "../utils/StandardPrintOutput.h"
#include "../utils/StringUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/TaskManager.h"
#include "../utils/Timer.h"
#include "../utils/FileUtils.h"
#include "../utils/ShouldCancelCallback.h"


// Test tracing some rays through the BVH
static void testTracingRays(js::BVH& bvh, const RayMesh& raymesh)
{
	const int N = 10000;
	PCG32 rng(1);
	int num_hit = 0;
	Timer timer;
	for(int z=0; z<N; ++z)
	{
		Ray ray(
			Vec4f(
				bvh.getAABBox().min_[0] + (-0.5f + rng.unitRandom() * 2.f) * (bvh.getAABBox().max_[0] - bvh.getAABBox().min_[0]),
				bvh.getAABBox().min_[1] + (-0.5f + rng.unitRandom() * 2.f) * (bvh.getAABBox().max_[1] - bvh.getAABBox().min_[1]),
				bvh.getAABBox().min_[2] + (-0.5f + rng.unitRandom() * 2.f) * (bvh.getAABBox().max_[2] - bvh.getAABBox().min_[2]),
				1.f
			),
			normalise(Vec4f(-1.f + rng.unitRandom()*2, -1.f + rng.unitRandom()*2, -1.f + rng.unitRandom()*2, 0)),
			0.f, // min_t
			1.0e20f // max_t
		);

		HitInfo hitinfo;
		const auto dist = bvh.traceRay(ray, hitinfo);
		if(dist >= 0)
		{
			testAssert(hitinfo.sub_elem_index < raymesh.getNumTris());
			num_hit++;
		}
	}
	conPrint("Frac rays hit: " + toString((double)num_hit / N) + ", tracing speed: " + doubleToStringNSigFigs(N / timer.elapsed(), 4) + " rays/s");
}


static void testOnAllIGMeshes(bool comprehensive_tests)
{
	glare::TaskManager task_manager;
	StandardPrintOutput print_output;
	DummyShouldCancelCallback should_cancel_callback;

	Timer timer;
	std::vector<std::string> files = FileUtils::getSortedFilesInDirWithExtensionFullPathsRecursive(TestUtils::getTestReposDir(), "igmesh");

	const size_t num_to_test = comprehensive_tests ? files.size() : myMin(files.size(), (size_t)1000);
	for(size_t i=0; i<num_to_test; ++i)
	{
		try
		{
			Indigo::Mesh indigo_mesh;
			Indigo::Mesh::readFromFile(toIndigoString(files[i]), indigo_mesh);

			conPrint(toString(i) + "/" + toString(files.size()) + ": Building '" + files[i] + "'... (tris: " + toString(indigo_mesh.triangles.size()) + ", quads: " + toString(indigo_mesh.quads.size()) + ")");

			RayMesh raymesh("raymesh", /*enable_shading_normals=*/false);
			raymesh.fromIndigoMesh(indigo_mesh);
			raymesh.buildTrisFromQuads();

			js::BVH bvh(&raymesh);
			bvh.build(print_output, should_cancel_callback, task_manager);
		
			testTracingRays(bvh, raymesh);
		}
		catch(Indigo::IndigoException& e)
		{
			conPrint("Skipping mesh failed to read (" + files[i] + "): " + toStdString(e.what())); // Error reading mesh file (maybe invalid)
		}
	}
	conPrint("Finished building all meshes.  Elapsed: " + timer.elapsedStringNSigFigs(3));
}


void js::BVH::test(bool comprehensive_tests)
{
	conPrint("js::BVH::test()");

	testOnAllIGMeshes(comprehensive_tests);

	if(false) // Perf test
	{
		glare::TaskManager task_manager;
		StandardPrintOutput print_output;
		DummyShouldCancelCallback should_cancel_callback;

		Indigo::Mesh indigo_mesh;
		try
		{
			const std::string path = TestUtils::getTestReposDir() + "/dist/benchmark_scenes/Arthur Liebnau - bedroom-benchmark-2016/mesh_4191131180918266302.igmesh";
			Indigo::Mesh::readFromFile(toIndigoString(path), indigo_mesh);

			conPrint("Building '" + path + "'... (tris: " + toString(indigo_mesh.triangles.size()) + ", quads: " + toString(indigo_mesh.quads.size()) + ")");

			RayMesh raymesh("raymesh", /*enable_shading_normals=*/false);
			raymesh.fromIndigoMesh(indigo_mesh);
			raymesh.buildTrisFromQuads();
			testAssert(raymesh.getNumTris() == 3202576);

			double sum_time = 0;
			double min_time = 1.0e100;
			const int trials = 10;
			for(int i=0; i<trials; ++i)
			{
				Timer timer;

				js::BVH bvh(&raymesh);
				bvh.build(print_output, should_cancel_callback, task_manager);

				const double elapsed = timer.elapsed();
				sum_time += elapsed;
				min_time = myMin(min_time, elapsed);


				testTracingRays(bvh, raymesh);
			}

			const double av_time = sum_time / trials;
			conPrint("av_time:  " + toString(av_time) + " s");
			conPrint("min_time: " + toString(min_time) + " s");
		}
		catch(Indigo::IndigoException& e)
		{
			failTest(toStdString(e.what()));
		}

	}

	conPrint("js::BVH::test() done.");
}


#endif // BUILD_TESTS
