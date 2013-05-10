/*=====================================================================
BVHObjectTree.cpp
-------------------
Copyright Glare Technologies Limited 2012 -
Generated at 2012-11-10 19:47:32 +0000
=====================================================================*/
#include "BVHObjectTree.h"


#include "BVHBuilder.h"
#include "../indigo/object.h"
#include "../simpleraytracer/ray.h"
#include "../maths/Vec4i.h"
#include "../utils/stringutils.h"
#include <limits>



BVHObjectTree::BVHObjectTree()
:	root_node_index(0)
{
}


BVHObjectTree::~BVHObjectTree()
{
}


void BVHObjectTree::insertObject(const Object* ob)
{
	objects.push_back(ob);
}


// NOTE: Uses SEE3 instruction _mm_shuffle_epi8.
static INDIGO_STRONG_INLINE const Vec4f shuffle8(const Vec4f& a, const Vec4i& shuf) { return _mm_castsi128_ps(_mm_shuffle_epi8(_mm_castps_si128(a.v), shuf.v)); }
static INDIGO_STRONG_INLINE const Vec4f vec4XOR(const Vec4f& a, const Vec4i& b ) { return _mm_castsi128_ps(_mm_xor_si128(_mm_castps_si128(a.v),b.v)); }


BVHObjectTree::Real BVHObjectTree::traceRay(const Ray& ray, ThreadContext& thread_context, double time, 
		const Object* last_object_hit,
		unsigned int last_triangle_hit,
		const Object*& hitob_out, HitInfo& hitinfo_out) const
{
	hitob_out = NULL;
	HitInfo ob_hit_info;
	float closest_dist = std::numeric_limits<float>::infinity();

	int stack[64];
	float dist_stack[64];
	int stack_top = 0;
	stack[0] = root_node_index; // Push root node onto stack
	dist_stack[0] = std::numeric_limits<float>::infinity(); // Near distances to nodes on the stack.

	const Vec4f r_x(ray.startPos().x[0]); // (r_x, r_x, r_x, r_x)
	const Vec4f r_y(ray.startPos().x[1]); // (r_y, r_y, r_y, r_y)
	const Vec4f r_z(ray.startPos().x[2]); // (r_z, r_z, r_z, r_z)

	const Vec4i negate(0x00000000, 0x00000000, 0x80000000, 0x80000000); // To flip sign bits on floats 2 and 3.
	const Vec4f rdir_x = vec4XOR(Vec4f(ray.getRecipRayDirF().x[0]), negate); // (1/d_x, 1/d_x, -1/d_x, -1/d_x)
	const Vec4f rdir_y = vec4XOR(Vec4f(ray.getRecipRayDirF().x[1]), negate); // (1/d_y, 1/d_y, -1/d_y, -1/d_y)
	const Vec4f rdir_z = vec4XOR(Vec4f(ray.getRecipRayDirF().x[2]), negate); // (1/d_z, 1/d_z, -1/d_z, -1/d_z)

	// Near_far will store current ray segment as (near, near, -far, -far)
	Vec4f near_far(ray.minT(), ray.minT(), -std::numeric_limits<float>::infinity(), -std::numeric_limits<float>::infinity());

	const Vec4i identity(	_mm_set_epi8(15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1, 0));
	const Vec4i swap(		_mm_set_epi8( 7,  6,  5,  4,  3,  2,  1,  0, 15, 14, 13, 12, 11, 10,  9, 8));

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
			const BVHObjectTreeNode& node = nodes[cur];
			
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

			// If hit both children:
			/*if(near_le_far.x[0] && near_le_far.x[1])
			{
				if(new_near_far.x[0] < new_near_far.x[1]) // If left child is closer
				{
					// Push right child onto stack
					assert(stack_top < 64);
					stack_top++;
					stack[stack_top] = node.child[1];
					dist_stack[stack_top] = new_near_far.x[1]; // push near_right

					cur = node.child[0];
					//near_far = _mm_shuffle_ps(near_far.v, new_near_far.v, _MM_SHUFFLE(2, 2, 0, 0)); // (near_left, near_left, -far_left, -far_left)
				}
				else
				{
					// Push left child onto stack
					stack_top++;
					stack[stack_top] = node.child[0];
					dist_stack[stack_top] = new_near_far.x[0]; // push near_left

					cur = node.child[1];
					//near_far = _mm_shuffle_ps(near_far.v, new_near_far.v, _MM_SHUFFLE(3, 3, 1, 1));  // (near_left, near_left, -far_left, -far_left)
				}
			}
			else // Else hit one or zero children
			{
				if(near_le_far.x[0]) // If hit left child:
				{
					cur = node.child[0];
					//near_far = _mm_shuffle_ps(near_far.v, new_near_far.v, _MM_SHUFFLE(2, 2, 0, 0)); // (near_left, near_left, -far_left, -far_left)
				}
				else if(near_le_far.x[1]) // If hit right child:
				{
					cur = node.child[1];
					//near_far = _mm_shuffle_ps(near_far.v, new_near_far.v, _MM_SHUFFLE(3, 3, 1, 1));  // (near_left, near_left, -far_left, -far_left)
				}
				else
					goto stack_pop; // Hit zero children, pop node off stack
			}*/
		}

		// current node is a leaf.  Intersect objects.
		cur ^= 0x80000000; // Zero sign bit
        const size_t ofs = size_t(cur) >> 5;
        const size_t num = size_t(cur) & 0x1F;
        for (size_t i=ofs; i<ofs+num; i++)
		{
			const Object* ob = leaf_objects[i];

			const Real dist = ob->traceRay(
				ray,
				closest_dist,
				time,
				thread_context,
				ob_hit_info
			);

			if(dist >= 0 && dist < closest_dist)
			{
				closest_dist = dist;
				hitob_out = ob;
				hitinfo_out = ob_hit_info;

				// Update far to min(dist, far)
				const Vec4f new_neg_far = Vec4f(_mm_max_ps(Vec4f(-dist).v, near_far.v)); // (., ., -min(far, dist), -min(far, dist))
				near_far = _mm_shuffle_ps(near_far.v, new_neg_far.v, _MM_SHUFFLE(3, 2, 1, 0)); // (near, near, -far, -far)
			}
		}
	}

	return hitob_out != NULL ? closest_dist : -1;
}


#if 0
bool BVHObjectTree::doesFiniteRayHit(const Ray& ray, Real max_t, ThreadContext& thread_context, double time, const Object* ignore_object, unsigned int ignore_tri) const
{
	int stack[64];
	int stack_top = 0;
	stack[0] = root_node_index; // Push root node onto stack

	const Vec4f r_x(ray.startPos().x[0]); // [r_x, r_x, r_x, r_x]
	const Vec4f r_y(ray.startPos().x[1]);
	const Vec4f r_z(ray.startPos().x[2]);

	const Vec4i negate(0x00000000, 0x00000000, 0x80000000, 0x80000000); // To flip sign bits on floats 2 and 3.
	const Vec4f rdir_x = vec4XOR(Vec4f(ray.getRecipRayDirF().x[0]), negate); // (1/d_x, 1/d_x, -1/d_x, -1/d_x)
	const Vec4f rdir_y = vec4XOR(Vec4f(ray.getRecipRayDirF().x[1]), negate); // (1/d_y, 1/d_y, -1/d_y, -1/d_y)
	const Vec4f rdir_z = vec4XOR(Vec4f(ray.getRecipRayDirF().x[2]), negate); // (1/d_z, 1/d_z, -1/d_z, -1/d_z)

	// Near_far will store current ray segment as (near, near, -far, -far)
	const Vec4f near_far(ray.minT(), ray.minT(), -max_t, -max_t);

	const Vec4i identity(	_mm_set_epi8(15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1, 0));
	const Vec4i swap(		_mm_set_epi8( 7,  6,  5,  4,  3,  2,  1,  0, 15, 14, 13, 12, 11, 10,  9, 8));

	const Vec4i x_shuffle = ray.getRecipRayDirF().x[0] > 0 ? identity : swap;
	const Vec4i y_shuffle = ray.getRecipRayDirF().x[1] > 0 ? identity : swap;
	const Vec4i z_shuffle = ray.getRecipRayDirF().x[2] > 0 ? identity : swap;

stack_pop:
	while(stack_top >= 0) // While still one or more nodes on the stack:
	{
		int cur = stack[stack_top--]; // Pop node off top of stack.

		while(cur >= 0) // While this is a proper interior node:
		{
			const BVHObjectTreeNode& node = nodes[cur];
			
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

			const Vec4f new_near_far(_mm_max_ps(_mm_max_ps(minmax_d_x, minmax_d_y), _mm_max_ps(near_far.v, minmax_d_z))); // (near_left, near_right, -far_left, -far_right)
			const Vec4f non_negated_near_far(vec4XOR(new_near_far, negate)); // (near_left, near_right, far_left, far_right)
			const Vec4f shuffled_near_far(shuffle8(non_negated_near_far, swap)); //  // (far_left, far_right, near_left, near_right)
			const Vec4i near_le_far(_mm_castps_si128(_mm_cmple_ps(new_near_far.v, shuffled_near_far.v))); // (near_left <= far_left, near_right <= far_right, ., .)

			// If hit both children:
			if(near_le_far.x[0] != 0 && near_le_far.x[1] != 0)
			{
				if(new_near_far.x[0] < new_near_far.x[1]) // If left child is closer
				{
					// Push right child onto stack
					assert(stack_top < 64);
					stack_top++;
					stack[stack_top] = node.child[1];

					cur = node.child[0];
				}
				else
				{
					// Push left child onto stack
					stack_top++;
					stack[stack_top] = node.child[0];

					cur = node.child[1];
				}
			}
			else // Else hit one or zero children
			{
				if(near_le_far.x[0] != 0) // If hit left child:
					cur = node.child[0];
				else if(near_le_far.x[1] != 0) // If hit right child:
					cur = node.child[1];
				else
					goto stack_pop; // Hit zero children, pop node off stack
			}
		}

		// current node is a leaf.  Intersect objects.
		cur ^= 0x80000000; // Zero sign bit
        const size_t ofs = size_t(cur) >> 5;
        const size_t num = size_t(cur) & 0x1F;
        for (size_t i=ofs; i<ofs+num; i++)
		{
			const Object* ob = leaf_objects[i];
			if(ob->doesFiniteRayHit(ray, max_t, time, thread_context,
					(ignore_object == ob ? ignore_tri : std::numeric_limits<unsigned int>::max())
			))
				return true;
		}
	}
	return false;
}
#endif


class BVHObjectTreeCallBack : public BVHBuilderCallBacks
{
public:
	virtual uint32 createNode()
	{
		// Don't actually create the node until it is marked as an interior node.
		return 0;
	}

	virtual int markAsInteriorNode(int node_index, int left_child_index, int right_child_index, const js::AABBox& left_aabb, const js::AABBox& right_aabb, int parent_index, bool is_left_child)
	{
		// Push back new node
		int index = (int)bvh->nodes.size();
		bvh->nodes.push_back(BVHObjectTreeNode());

		//conPrint("Marking node " + toString(index) + " as interior node.");

		BVHObjectTreeNode& node = bvh->nodes.back();

		node.x = Vec4f(left_aabb.min_.x[0], right_aabb.min_.x[0], left_aabb.max_.x[0], right_aabb.max_.x[0]);
		node.y = Vec4f(left_aabb.min_.x[1], right_aabb.min_.x[1], left_aabb.max_.x[1], right_aabb.max_.x[1]);
		node.z = Vec4f(left_aabb.min_.x[2], right_aabb.min_.x[2], left_aabb.max_.x[2], right_aabb.max_.x[2]);

		node.child[0] = left_child_index;
		node.child[1] = right_child_index;

		// Now that we have properly created this node, fix up child link in parent.  Don't do this if this is the root node!
		if(index > 0) // If not root node
			bvh->nodes[parent_index].child[is_left_child ? 0 : 1] = index;

		return index;
	}


	virtual void markAsLeafNode(int node_index, const std::vector<uint32>& objects, int begin, int end, int parent_index, bool is_left_child)
	{
		//conPrint("Marking node as leaf node. (parent_index: " + toString(parent_index) + ", left child: " + boolToString(is_left_child) + ")");

		int offset = (int)bvh->leaf_objects.size();
		int num = end - begin;

		// Number of objects is in lower 5 bits.  Set sign bit to 1.
		assert(num < 32);
		int c = 0x80000000 | num | (offset << 5);

		if(bvh->nodes.empty())
			bvh->root_node_index = c;
		else
		{
			BVHObjectTreeNode& parent = bvh->nodes[parent_index];

			parent.child[is_left_child ? 0 : 1] = c;
		}

		// Copy objects in leaf to leaf_objects
		for(size_t i=begin; i!=end; ++i)
			bvh->leaf_objects.push_back(bvh->objects[objects[i]]);
	}

	BVHObjectTree* bvh;
};


void BVHObjectTree::build(PrintOutput& print_output, bool verbose)
{
	conPrint("BVHObjectTree::build");

	leaf_objects.reserve(objects.size());

	js::Vector<js::AABBox, 32> aabbs(objects.size());

	for(size_t i=0; i<objects.size(); ++i)
		aabbs[i] = objects[i]->getAABBoxWS();

	BVHBuilder builder(
		1, // leaf_num_object_threshold
		31, // max_num_objects_per_leaf (2^5 - 1)
		100 // intersection_cost
	);

	BVHObjectTreeCallBack callback;
	callback.bvh = this;

	builder.build(
		aabbs.empty() ? NULL : &aabbs[0],
		(int)objects.size(),
		print_output,
		verbose,
		callback
	);

	printVar(builder.num_maxdepth_leaves);
	printVar(builder.num_under_thresh_leaves);
	printVar(builder.num_cheaper_nosplit_leaves);
	printVar(builder.num_could_not_split_leaves);
	printVar(builder.num_leaves);
	printVar(builder.max_num_tris_per_leaf);
	printVar(builder.leaf_depth_sum);
	printVar(builder.max_leaf_depth);
	printVar(builder.num_interior_nodes);
	conPrint("av leaf depth: " + toString((float)builder.leaf_depth_sum / builder.num_leaves));

	// Don't need objects array any more.
	objects.clearAndFreeMem();

	conPrint("BVHObjectTree::build done");
}
