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
#include "../indigo/PrintOutput.h"
#include "../indigo/ThreadContext.h"
#include "../indigo/DistanceHitInfo.h"
#include "../simpleraytracer/raymesh.h"
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
