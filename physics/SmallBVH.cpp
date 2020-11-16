/*=====================================================================
SmallBVH.cpp
------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "SmallBVH.h"


#include "BinningBVHBuilder.h"
#include "MollerTrumboreTri.h"
#include "../indigo/DistanceHitInfo.h"
#include "../simpleraytracer/raymesh.h"
#include "../physics/jscol_boundingsphere.h"
#include "../utils/PrintOutput.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


namespace js
{


SmallBVH::SmallBVH(const RayMesh* const raymesh_)
:	raymesh(raymesh_)
{
	assert(raymesh);
	
	static_assert(sizeof(SmallBVHNode) == 64, "sizeof(SmallBVHNode) == 64");
}


SmallBVH::~SmallBVH()
{
}


void SmallBVH::build(PrintOutput& print_output, ShouldCancelCallback& should_cancel_callback, bool verbose, Indigo::TaskManager& task_manager)
{
	if(verbose) print_output.print("\tSmallBVH::build()");
	// Timer timer;

	const RayMesh::TriangleVectorType& raymesh_tris = raymesh->getTriangles();
	const size_t raymesh_tris_size = raymesh_tris.size();
	const RayMesh::VertexVectorType& raymesh_verts = raymesh->getVertices();

	Reference<BinningBVHBuilder> builder = new BinningBVHBuilder(
		63, // leaf_num_object_threshold. As soon as we get down to this number of tris, make a leaf.
		63, // max_num_objects_per_leaf (2^6 - 1)
		1.f, // intersection_cost
		(int)raymesh_tris_size
	);

	for(size_t i=0; i<raymesh_tris_size; ++i)
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
		verbose,
		result_nodes
	);
	const BVHBuilder::ResultObIndicesVec& result_ob_indices = builder->getResultObjectIndices();

	root_aabb = builder->getRootAABB();

	if(result_nodes.size() == 1)
	{
		// Special case where the root node is a leaf.  
		const int num = result_nodes[0].right - result_nodes[0].left;
		const int offset = result_nodes[0].left;
		assert(num < 64);
		int c = 0x80000000 | num | (offset << 6);
		this->root_node_index = c;
	}
	else
	{
		this->root_node_index = 0;

		// Convert result_nodes to BVHObjectTreeNodes.
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
				SmallBVHNode& node = this->nodes[new_index++];

				const ResultNode& result_left_child  = result_nodes[result_node.left];
				const ResultNode& result_right_child = result_nodes[result_node.right];

				node.x = Vec4f(result_left_child.aabb.min_.x[0], result_right_child.aabb.min_.x[0], result_left_child.aabb.max_.x[0], result_right_child.aabb.max_.x[0]);
				node.y = Vec4f(result_left_child.aabb.min_.x[1], result_right_child.aabb.min_.x[1], result_left_child.aabb.max_.x[1], result_right_child.aabb.max_.x[1]);
				node.z = Vec4f(result_left_child.aabb.min_.x[2], result_right_child.aabb.min_.x[2], result_left_child.aabb.max_.x[2], result_right_child.aabb.max_.x[2]);

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
					assert(num < 64);
					int c = 0x80000000 | num | (offset << 6);
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
					assert(num < 64);
					int c = 0x80000000 | num | (offset << 6);
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

	// conPrint("SmallBVH total build took " + timer.elapsedString());
}


// NOTE: Uses SEE3 instruction _mm_shuffle_epi8.
static INDIGO_STRONG_INLINE const Vec4f shuffle8(const Vec4f& a, const Vec4i& shuf) { return _mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128(a.v), shuf.v)); }
static INDIGO_STRONG_INLINE const Vec4f vec4XOR(const Vec4f& a, const Vec4i& b) { return _mm_castsi128_ps(_mm_xor_si128(_mm_castps_si128(a.v), b.v)); }


SmallBVH::DistType SmallBVH::traceRay(const Ray& ray_, HitInfo& hitinfo_out) const
{
	//hitinfo_out.hit_opaque_ob = false; // Just consider the hit as non-opaque.  This is the conservative option.

	Ray ray = ray_;
	HitInfo ob_hit_info;

	int stack[64];
	float dist_stack[64];
	int stack_top = 0;
	stack[0] = root_node_index; // Push root node onto stack
	dist_stack[0] = -std::numeric_limits<float>::infinity(); // Near distances to nodes on the stack.

	const Vec4f r_x(ray.startPos().x[0]); // (r_x, r_x, r_x, r_x)
	const Vec4f r_y(ray.startPos().x[1]); // (r_y, r_y, r_y, r_y)
	const Vec4f r_z(ray.startPos().x[2]); // (r_z, r_z, r_z, r_z)

	const Vec4i negate(0x00000000, 0x00000000, 0x80000000, 0x80000000); // To flip sign bits on floats 2 and 3.
	const Vec4f rdir_x = vec4XOR(Vec4f(ray.getRecipRayDirF().x[0]), negate); // (1/d_x, 1/d_x, -1/d_x, -1/d_x)
	const Vec4f rdir_y = vec4XOR(Vec4f(ray.getRecipRayDirF().x[1]), negate); // (1/d_y, 1/d_y, -1/d_y, -1/d_y)
	const Vec4f rdir_z = vec4XOR(Vec4f(ray.getRecipRayDirF().x[2]), negate); // (1/d_z, 1/d_z, -1/d_z, -1/d_z)

	// Near_far will store current ray segment as (near, near, -far, -far)
	Vec4f near_far(ray.minT(), ray.minT(), -ray.maxT(), -ray.maxT());

	const Vec4i identity(_mm_set_epi8(15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0));
	const Vec4i swap(_mm_set_epi8(7, 6, 5, 4, 3, 2, 1, 0, 15, 14, 13, 12, 11, 10, 9, 8));

	const Vec4i x_shuffle = ray.getRecipRayDirF().x[0] > 0 ? identity : swap;
	const Vec4i y_shuffle = ray.getRecipRayDirF().x[1] > 0 ? identity : swap;
	const Vec4i z_shuffle = ray.getRecipRayDirF().x[2] > 0 ? identity : swap;

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
			const SmallBVHNode& node = nodes[cur];

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
						assert(stack_top < 64);
						stack_top++;
						stack[stack_top] = node.child[1];
						dist_stack[stack_top] = new_near_far.x[1]; // push near_right

						cur = node.child[0];
					}
					else
					{
						// Push left child onto stack
						stack_top++;
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
		const size_t ofs = size_t(cur) >> 6;
		const size_t num = size_t(cur) & 0x3F;
		for(size_t i=ofs; i<ofs+num; i++)
		{
			const uint32 tri_index = leaf_tri_indices[i];
			
			MollerTrumboreTri tri;
			tri.set(raymesh->triVertPos(tri_index, 0), raymesh->triVertPos(tri_index, 1), raymesh->triVertPos(tri_index, 2));

			Real dist;
			if(tri.referenceIntersect(ray, &ob_hit_info.sub_elem_coords.x, &ob_hit_info.sub_elem_coords.y, &dist))
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


void SmallBVH::getAllHits(const Ray& ray, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	hitinfos_out.resize(0);
	assert(0);
}


const js::AABBox& SmallBVH::getAABBox() const
{
	return root_aabb;
}


size_t SmallBVH::getTotalMemUsage() const
{
	return nodes.dataSizeBytes() + leaf_tri_indices.dataSizeBytes();
}


} //end namespace js
