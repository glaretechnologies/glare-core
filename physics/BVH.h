/*=====================================================================
BVH.h
-----
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "jscol_Tree.h"
#include "jscol_aabbox.h"
#include "MollerTrumboreTri.h"
#include "../maths/vec3.h"
#include "../utils/MemAlloc.h"
#include "../utils/Vector.h"


class RayMesh;


namespace js
{


class BoundingSphere;


class BVHNode
{
public:
	Vec4f x; // (left_min_x, right_min_x, left_max_x, right_max_x)
	Vec4f y; // (left_min_y, right_min_y, left_max_y, right_max_y)
	Vec4f z; // (left_min_z, right_min_z, left_max_z, right_max_z)

	int32 child[2];
	int32 padding[2]; // Pad to 64 bytes.
};


/*=====================================================================
BVH
----
Triangle mesh acceleration structure.
=====================================================================*/
class BVH : public Tree
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	BVH(const RayMesh* const raymesh);
	virtual ~BVH();

	// Throws Indigo::CancelledException if cancelled.
	virtual void build(PrintOutput& print_output, ShouldCancelCallback& should_cancel_callback, Indigo::TaskManager& task_manager); // throws Indigo::Exception

	virtual DistType traceRay(const Ray& ray, HitInfo& hitinfo_out) const;
	virtual DistType traceSphere(const Ray& ray_ws, const Matrix4f& to_object, const Matrix4f& to_world, float radius_ws, Vec4f& hit_normal_ws_out) const;
	virtual void appendCollPoints(const Vec4f& sphere_pos_ws, float radius_ws, const Matrix4f& to_object, const Matrix4f& to_world, std::vector<Vec4f>& points_ws_in_out) const;
	virtual const js::AABBox& getAABBox() const;

	virtual void printStats() const {}
	virtual void printTraceStats() const {}
	virtual size_t getTotalMemUsage() const;

	static void test(bool comprehensive_tests);

	typedef uint32 TRI_INDEX;
private:
	inline void intersectSphereAgainstLeafTri(Ray& ray_ws, const Matrix4f& to_world, float radius_ws,
		TRI_INDEX tri_index, Vec4f& hit_normal_ws_out) const;

	typedef js::Vector<BVHNode, 64> NODE_VECTOR_TYPE;
	typedef MollerTrumboreTri INTERSECT_TRI_TYPE;

	AABBox root_aabb; // AABB of whole thing
	NODE_VECTOR_TYPE nodes; // Nodes of the tree.
	js::Vector<TRI_INDEX, 64> leaf_tri_indices; // Indices into the intersect_tris array.
	const RayMesh* const raymesh;
	int32 root_node_index;
};


} //end namespace js
