/*=====================================================================
BVH.h
-----
Copyright Glare Technologies Limited 2015 -
File created by ClassTemplate on Sun Oct 26 17:19:14 2008
=====================================================================*/
#pragma once


#include "BVHNode.h"
#include "jscol_Tree.h"
#include "MollerTrumboreTri.h"
#include "../maths/vec3.h"
#include "../utils/MemAlloc.h"
#include "../utils/Vector.h"


class RayMesh;


namespace js
{


class BoundingSphere;


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
	virtual void build(PrintOutput& print_output, ShouldCancelCallback& should_cancel_callback, bool verbose, Indigo::TaskManager& task_manager); // throws Indigo::Exception


	virtual DistType traceRay(const Ray& ray, HitInfo& hitinfo_out) const;
	virtual DistType traceSphere(const Ray& ray_ws, const Matrix4f& to_object, const Matrix4f& to_world, float radius_ws, Vec4f& hit_normal_ws_out) const;
	virtual void appendCollPoints(const Vec4f& sphere_pos_ws, float radius_ws, const Matrix4f& to_object, const Matrix4f& to_world, std::vector<Vec4f>& points_ws_in_out) const;
	virtual const js::AABBox& getAABBox() const;

	virtual void getAllHits(const Ray& ray, std::vector<DistanceHitInfo>& hitinfos_out) const;

	//virtual const Vec3f triGeometricNormal(unsigned int tri_index) const;

	// For Debugging:
	Real traceRayAgainstAllTris(const Ray& ray, Real max_t, HitInfo& hitinfo_out) const;
	void getAllHitsAllTris(const Ray& ray, std::vector<DistanceHitInfo>& hitinfos_out) const;

	virtual void printStats() const {}
	virtual void printTraceStats() const {}
	virtual size_t getTotalMemUsage() const;

	friend class BVHImpl;
	friend class TraceRayFunctions;
	friend class GetAllHitsFunctions;

	typedef uint32 TRI_INDEX;
private:
	inline void intersectSphereAgainstLeafTris(const Ray& ray_dir_ws, const Matrix4f& to_world, float radius_ws,
		int num_geom, int geom_index, float& closest_dist, Vec4f& hit_normal_ws_out) const;

	typedef js::Vector<BVHNode, 64> NODE_VECTOR_TYPE;
	typedef MollerTrumboreTri INTERSECT_TRI_TYPE;

	AABBox root_aabb; // AABB of whole thing
	NODE_VECTOR_TYPE nodes; // Nodes of the tree.
	js::Vector<TRI_INDEX, 64> leafgeom; // Indices into the intersect_tris array.
	std::vector<INTERSECT_TRI_TYPE> intersect_tris;
	
	const RayMesh* const raymesh;
};


} //end namespace js
