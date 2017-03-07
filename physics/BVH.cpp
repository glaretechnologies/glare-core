/*=====================================================================
BVH.cpp
-------
Copyright Glare Technologies Limited 2015 -
File created by ClassTemplate on Sun Oct 26 17:19:14 2008
=====================================================================*/
#include "BVH.h"


#include "BVHImpl.h"
#include "BVHBuilder.h"
#include "jscol_aabbox.h"
#include "../indigo/ThreadContext.h"
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


static inline void convertPos(const Vec3f& p, Vec4f& pos_out)
{
	pos_out.x[0] = p.x;
	pos_out.x[1] = p.y;
	pos_out.x[2] = p.z;
	pos_out.x[3] = 1.0f;
}


void BVH::build(PrintOutput& print_output, bool verbose, Indigo::TaskManager& task_manager)
{
	if(verbose) print_output.print("\tBVH::build()");
	Timer timer;

	const RayMesh::TriangleVectorType& raymesh_tris = raymesh->getTriangles();
	const size_t raymesh_tris_size = raymesh_tris.size();
	const RayMesh::VertexVectorType& raymesh_verts = raymesh->getVertices();

	// Build tri AABBs, root_aabb
	root_aabb = js::AABBox::emptyAABBox();
	tri_aabbs.resize(raymesh_tris_size);
	for(size_t i=0; i<raymesh_tris_size; ++i)
	{
		const RayMeshTriangle& tri = raymesh_tris[i];

		Vec4f p;
		convertPos(raymesh_verts[tri.vertex_indices[0]].pos, p);
		tri_aabbs[i].min_ = p;
		tri_aabbs[i].max_ = p;

		convertPos(raymesh_verts[tri.vertex_indices[1]].pos, p);
		tri_aabbs[i].enlargeToHoldPoint(p);

		convertPos(raymesh_verts[tri.vertex_indices[2]].pos, p);
		tri_aabbs[i].enlargeToHoldPoint(p);

		root_aabb.enlargeToHoldAABBox(tri_aabbs[i]);
	}

	BVHBuilder builder(
		4, // leaf_num_object_threshold.  Since we are intersecting against 4 tris at once, as soon as we get down to 4 tris, make a leaf.
		BVHNode::maxNumGeom(), // max_num_objects_per_leaf
		4.f // intersection_cost.
	);

	js::Vector<ResultNode, 64> result_nodes;
	builder.build(
		task_manager,
		tri_aabbs.data(),
		(int)tri_aabbs.size(),
		print_output,
		verbose,
		result_nodes
	);
	const BVHBuilder::ResultObIndicesVec& result_ob_indices = builder.getResultObjectIndices();

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
		const int num_4tris = ((num_tris % 4) == 0) ? (num_tris / 4) : (num_tris / 4) + 1;
		const int num_tris_rounded_up = num_4tris * 4;
		const int num_padding = num_tris_rounded_up - num_tris;

		assert(num_tris + num_padding == num_tris_rounded_up);
		assert(num_tris_rounded_up % 4 == 0);
		assert(num_4tris <= (int)BVHNode::maxNumGeom());

		nodes[0].setLeftToLeaf();
		nodes[0].setLeftGeomIndex((unsigned int)leafgeom.size());
		nodes[0].setLeftNumGeom(num_4tris);

		for(int i=begin; i<end; ++i)
		{
			const uint32 source_tri_i = result_ob_indices[i];
			leafgeom.push_back(source_tri_i);
		}

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
		this->nodes.resizeUninitialised(new_num_nodes);

		new_index = 0;
		for(size_t i=0; i<result_nodes_size; ++i)
		{
			const ResultNode& result_node = result_nodes[i];
			if(result_node.interior)
			{
				BVHNode& node = this->nodes[new_index++];
				node.setToInterior();

				node.setLeftAABB(result_node.left_aabb);
				node.setRightAABB(result_node.right_aabb);

				const ResultNode& left_child  = result_nodes[result_node.left];
				const ResultNode& right_child = result_nodes[result_node.right];

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
					assert(num_tris >= 0);
					const int num_4tris = ((num_tris % 4) == 0) ? (num_tris / 4) : (num_tris / 4) + 1;
					const int num_tris_rounded_up = num_4tris * 4;
					const int num_padding = num_tris_rounded_up - num_tris;

					assert(num_tris + num_padding == num_tris_rounded_up);
					assert(num_tris_rounded_up % 4 == 0);
					assert(num_4tris <= (int)BVHNode::maxNumGeom());

					node.setLeftToLeaf();
					node.setLeftGeomIndex((unsigned int)leafgeom.size());
					node.setLeftNumGeom(num_4tris);

					for(int i=begin; i<end; ++i)
					{
						const uint32 source_tri_i = result_ob_indices[i];
						leafgeom.push_back(source_tri_i);
					}

					// Add padding if needed
					for(int i=0; i<num_padding; ++i)
					{
						const uint32 back_tri_i = leafgeom.back();
						leafgeom.push_back(back_tri_i);
					}
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
					assert(num_tris >= 0);
					const int num_4tris = ((num_tris % 4) == 0) ? (num_tris / 4) : (num_tris / 4) + 1;
					const int num_tris_rounded_up = num_4tris * 4;
					const int num_padding = num_tris_rounded_up - num_tris;

					assert(num_tris + num_padding == num_tris_rounded_up);
					assert(num_tris_rounded_up % 4 == 0);
					assert(num_4tris <= (int)BVHNode::maxNumGeom());

					node.setRightToLeaf();
					node.setRightGeomIndex((unsigned int)leafgeom.size());
					node.setRightNumGeom(num_4tris);

					for(int i=begin; i<end; ++i)
					{
						const uint32 source_tri_i = result_ob_indices[i];
						leafgeom.push_back(source_tri_i);
					}

					// Add padding if needed
					for(int i=0; i<num_padding; ++i)
					{
						const uint32 back_tri_i = leafgeom.back();
						leafgeom.push_back(back_tri_i);
					}
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



	this->tri_aabbs.clearAndFreeMem(); // only needed during build.

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
		float& best_t,
		ThreadContext& thread_context
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


BVH::DistType BVH::traceRay(const Ray& ray, DistType ray_max_t, ThreadContext& thread_context, HitInfo& hitinfo_out) const
{
	return BVHImpl::traceRay<TraceRayFunctions>(*this, ray, ray_max_t, 
		thread_context, 
		thread_context.getTreeContext(),
		hitinfo_out
	);
}


BVH::DistType BVH::traceSphere(const Ray& ray, float radius, DistType max_t, ThreadContext& thread_context, Vec4f& hit_normal_out) const
{
	const Vec3f sourcePoint3(ray.startPos());
	const Vec3f unitdir3(ray.unitDir());
	const js::BoundingSphere sphere_os(ray.startPos(), radius);

	// Build AABB of sphere path in object space.  NOTE: Assuming ray.minT() == 0.
	Vec4f endpos = ray.startPos() + ray.unitDir() * (float)max_t;
	js::AABBox spherepath_aabb(min(ray.startPos(), endpos) - Vec4f(radius, radius, radius, 0), max(ray.startPos(), endpos) + Vec4f(radius, radius, radius, 0));

	const SSE_ALIGN unsigned int mask[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0 };
	const __m128 maskWToZero = _mm_load_ps((const float*)mask);

	float closest_dist = (float)max_t; // This is the distance along the ray to the minimum of the closest hit so far and the maximum length of the ray

	std::vector<uint32>& bvh_stack = thread_context.getTreeContext().bvh_stack;
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
				__m128 lower_separation = _mm_cmplt_ps(spherepath_aabb.max_.v, box_min); // [spherepath.max.x < leftchild.min.x, spherepath.max.y < leftchild.min.y, ...]
				__m128 upper_separation = _mm_cmplt_ps(box_max, spherepath_aabb.min_.v);// [leftchild.max.x < spherepath.min.x, leftchild.max.y < spherepath.min.y, ...]
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
				__m128 lower_separation = _mm_cmplt_ps(spherepath_aabb.max_.v, box_min); 
				__m128 upper_separation = _mm_cmplt_ps(box_max, spherepath_aabb.min_.v);
				__m128 either = _mm_and_ps(_mm_or_ps(lower_separation, upper_separation), maskWToZero); // Will have a bit set if there is a separation on any axis
				right_disjoint = _mm_movemask_ps(either); // Creates a 4-bit mask from the most significant bits
			}

			int left_geom_index;
			int left_num_geom = 0;
			int right_geom_index;
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
						assert(stacktop < (int)bvh_stack.size());
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

			intersectSphereAgainstLeafTris(sphere_os, ray, left_num_geom,  left_geom_index,  closest_dist, hit_normal_out);
			intersectSphereAgainstLeafTris(sphere_os, ray, right_num_geom, right_geom_index, closest_dist, hit_normal_out);

			// We may have hit a triangle, which will have decreased closest_dist.  So recompute the sphere path AABB.
			endpos = ray.startPos() + ray.unitDir() * closest_dist;
			spherepath_aabb = js::AABBox(min(ray.startPos(), endpos) - Vec4f(radius, radius, radius, 0), max(ray.startPos(), endpos) + Vec4f(radius, radius, radius, 0));
		}
	}


	if(closest_dist < (float)max_t)
		return closest_dist;
	else
		return -1.0f; // Missed all tris
}


void BVH::intersectSphereAgainstLeafTris(js::BoundingSphere sphere_os, const Ray& ray, int num_geom, int geom_index, float& closest_dist, Vec4f& hit_normal_out) const
{
	const Vec3f sourcePoint3(ray.startPos());
	const Vec3f unitdir3(ray.unitDir());

	for(int i=0; i<num_geom; ++i)
	{
		for(int z=0; z<4; ++z)
		{
			const MollerTrumboreTri& tri = intersect_tris[leafgeom[geom_index + z]];

			const Vec3f v0(tri.data);
			const Vec3f e1(tri.data + 3);
			const Vec3f e2(tri.data + 6);

			js::Triangle js_tri(v0, v0 + e1, v0 + e2);

			const Vec3f normal = normalise(crossProduct(e1, e2));

			Planef tri_plane(v0, normal);

			// Determine the distance from the plane to the sphere center
			float pDist = tri_plane.signedDistToPoint(sourcePoint3);

			//-----------------------------------------------------------------
			//Invert normal if doing backface collision, so 'usenormal' is always facing
			//towards sphere center.
			//-----------------------------------------------------------------
			Vec3f usenormal = normal;
			if(pDist < 0)
			{
				usenormal *= -1;
				pDist *= -1;
			}

			assert(pDist >= 0);

			//-----------------------------------------------------------------
			//check if sphere is heading away from tri
			//-----------------------------------------------------------------
			const float approach_rate = -usenormal.dot(unitdir3);
			if(approach_rate <= 0)
				continue;

			assert(approach_rate > 0);

			// trans_len_needed = dist to approach / dist approached per unit translation len
			const float trans_len_needed = (pDist - sphere_os.getRadius()) / approach_rate;

			if(closest_dist < trans_len_needed)
				continue; // then sphere will never get to plane

						  //-----------------------------------------------------------------
						  //calc the point where the sphere intersects with the triangle plane (planeIntersectionPoint)
						  //-----------------------------------------------------------------
			Vec3f planeIntersectionPoint;

			// Is the plane embedded in the sphere?
			if(trans_len_needed <= 0)//pDist <= sphere.getRadius())//make == trans_len_needed < 0
			{
				// Calculate the plane intersection point
				planeIntersectionPoint = tri_plane.closestPointOnPlane(sourcePoint3);

			}
			else
			{
				assert(trans_len_needed >= 0);

				planeIntersectionPoint = sourcePoint3 + (unitdir3 * trans_len_needed) - (sphere_os.getRadius() * usenormal);

				//assert point is actually on plane
				//			assert(epsEqual(tri.getTriPlane().signedDistToPoint(planeIntersectionPoint), 0.0f, 0.0001f));
			}

			//-----------------------------------------------------------------
			//now restrict collision point on tri plane to inside tri if neccessary.
			//-----------------------------------------------------------------
			Vec3f triIntersectionPoint = planeIntersectionPoint;

			const bool point_in_tri = js_tri.pointInTri(triIntersectionPoint);
			if(!point_in_tri)
			{
				//-----------------------------------------------------------------
				//restrict to inside tri
				//-----------------------------------------------------------------
				triIntersectionPoint = js_tri.closestPointOnTriangle(triIntersectionPoint);
			}

			//-----------------------------------------------------------------
			//Using the triIntersectionPoint, we need to reverse-intersect
			//with the sphere
			//-----------------------------------------------------------------

			//returns dist till hit sphere or -1 if missed
			//inline float rayIntersect(const Vec3& raystart_os, const Vec3& rayunitdir) const;
			const float dist = sphere_os.rayIntersect(triIntersectionPoint.toVec4fPoint(), -ray.unitDir());

			if(dist >= 0 && dist < closest_dist)
			{
				closest_dist = dist;

				//-----------------------------------------------------------------
				//calc hit normal
				//-----------------------------------------------------------------
				if(point_in_tri)
					hit_normal_out = usenormal.toVec4fVector();
				else
				{
					//-----------------------------------------------------------------
					//calc point sphere will be when it hits edge of tri
					//-----------------------------------------------------------------
					const Vec3f hit_spherecenter = sourcePoint3 + unitdir3 * dist;

					hit_normal_out = ((hit_spherecenter - triIntersectionPoint) / sphere_os.getRadius()).toVec4fVector();
				}
			}
		}

		geom_index += 4;
	}
}


void BVH::appendCollPoints(const Vec4f& sphere_pos, float radius, ThreadContext& thread_context, std::vector<Vec4f>& points_ws_in_out) const
{
	const float radius2 = radius*radius;
	const Vec3f sphere_pos3(sphere_pos);
	const js::AABBox sphere_aabb(sphere_pos - Vec4f(radius, radius, radius, 0), sphere_pos + Vec4f(radius, radius, radius, 0));

	const SSE_ALIGN unsigned int mask[4] = { 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x0 };
	const __m128 maskWToZero = _mm_load_ps((const float*)mask);

	std::vector<uint32>& bvh_stack = thread_context.getTreeContext().bvh_stack;

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
				__m128 lower_separation = _mm_cmplt_ps(sphere_aabb.max_.v, box_min); // [spherepath.max.x < leftchild.min.x, spherepath.max.y < leftchild.min.y, ...]
				__m128 upper_separation = _mm_cmplt_ps(box_max, sphere_aabb.min_.v);// [leftchild.max.x < spherepath.min.x, leftchild.max.y < spherepath.min.y, ...]
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
				__m128 lower_separation = _mm_cmplt_ps(sphere_aabb.max_.v, box_min);
				__m128 upper_separation = _mm_cmplt_ps(box_max, sphere_aabb.min_.v);
				__m128 either = _mm_and_ps(_mm_or_ps(lower_separation, upper_separation), maskWToZero); // Will have a bit set if there is a separation on any axis
				right_disjoint = _mm_movemask_ps(either); // Creates a 4-bit mask from the most significant bits
			}

			int left_geom_index;
			int left_num_geom = 0;
			int right_geom_index;
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
						assert(stacktop < (int)bvh_stack.size());
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
					const MollerTrumboreTri& moller_tri = intersect_tris[leafgeom[left_geom_index + z]];
					const Vec3f v0(moller_tri.data);
					const Vec3f e1(moller_tri.data + 3);
					const Vec3f e2(moller_tri.data + 6);
					js::Triangle tri(v0, v0 + e1, v0 + e2);

					// See if sphere is touching plane
					const Planef tri_plane = tri.getTriPlane();
					const float disttoplane = tri_plane.signedDistToPoint(sphere_pos3);
					if(fabs(disttoplane) > radius)
						continue;

					// Get closest point on plane to sphere center
					Vec3f planepoint = tri_plane.closestPointOnPlane(sphere_pos3);

					// Restrict point to inside tri
					if(!tri.pointInTri(planepoint))
						planepoint = tri.closestPointOnTriangle(planepoint);

					if(planepoint.getDist2(sphere_pos3) <= radius2)
					{
						if(points_ws_in_out.empty() || (planepoint.toVec4fPoint() != points_ws_in_out.back())) // HACK: Don't add if same as last point.  May happen due to packing of 4 tris together with possible duplicates.
							points_ws_in_out.push_back(planepoint.toVec4fPoint());
					}
				}

				left_geom_index += 4;
			}
			for(int i=0; i<right_num_geom; ++i)
			{
				for(int z=0; z<4; ++z)
				{
					const MollerTrumboreTri& moller_tri = intersect_tris[leafgeom[right_geom_index + z]];
					const Vec3f v0(moller_tri.data);
					const Vec3f e1(moller_tri.data + 3);
					const Vec3f e2(moller_tri.data + 6);
					const js::Triangle tri(v0, v0 + e1, v0 + e2);

					// See if sphere is touching plane
					const Planef tri_plane = tri.getTriPlane();
					const float disttoplane = tri_plane.signedDistToPoint(sphere_pos3);
					if(fabs(disttoplane) > radius)
						continue;

					// Get closest point on plane to sphere center
					Vec3f planepoint = tri_plane.closestPointOnPlane(sphere_pos3);

					// Restrict point to inside tri
					if(!tri.pointInTri(planepoint))
						planepoint = tri.closestPointOnTriangle(planepoint);

					if(planepoint.getDist2(sphere_pos3) <= radius2)
					{
						if(points_ws_in_out.empty() || (planepoint.toVec4fPoint() != points_ws_in_out.back())) // HACK: Don't add if same as last point.  May happen due to packing of 4 tris together with possible duplicates.
							points_ws_in_out.push_back(planepoint.toVec4fPoint());
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
		float& best_t,
		ThreadContext& thread_context
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


void BVH::getAllHits(const Ray& ray, ThreadContext& thread_context, std::vector<DistanceHitInfo>& hitinfos_out) const
{
	hitinfos_out.resize(0);

	BVHImpl::traceRay<GetAllHitsFunctions>(*this, ray, std::numeric_limits<float>::max(), thread_context, 
		thread_context.getTreeContext(), // context, 
		hitinfos_out
	);
}


const js::AABBox& BVH::getAABBoxWS() const
{
	return root_aabb;
}


size_t BVH::getTotalMemUsage() const
{
	return leafgeom.dataSizeBytes() + intersect_tris.size()*sizeof(INTERSECT_TRI_TYPE);
}


/*const Vec3f BVH::triGeometricNormal(unsigned int tri_index) const //slow
{
	//return intersect_tris[new_tri_index[tri_index]].getNormal();
	return intersect_tris[tri_index].getNormal();
}*/


} //end namespace js
