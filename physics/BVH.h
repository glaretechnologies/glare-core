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
#include "../maths/SSE.h"
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
	INDIGO_ALIGNED_NEW_DELETE

	BVH(const RayMesh* const raymesh);
	virtual ~BVH();

	virtual void build(PrintOutput& print_output, bool verbose, Indigo::TaskManager& task_manager); // throws Indigo::Exception


	virtual DistType traceRay(const Ray& ray, ThreadContext& thread_context, HitInfo& hitinfo_out) const;
	virtual DistType traceSphere(const Ray& ray, float radius, DistType max_t, ThreadContext& thread_context, Vec4f& hit_normal_out) const;
	virtual void appendCollPoints(const Vec4f& sphere_pos, float radius, ThreadContext& thread_context, std::vector<Vec4f>& points_ws_in_out) const;
	virtual const js::AABBox& getAABBoxWS() const;

	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, std::vector<DistanceHitInfo>& hitinfos_out) const;

	//virtual const Vec3f triGeometricNormal(unsigned int tri_index) const;

	virtual void printStats() const {}
	virtual void printTraceStats() const {}
	virtual size_t getTotalMemUsage() const;

	friend class BVHImpl;
	friend class TraceRayFunctions;
	friend class GetAllHitsFunctions;

	typedef uint32 TRI_INDEX;
private:
	inline void intersectSphereAgainstLeafTris(js::BoundingSphere sphere_os, const Ray& ray,
		int num_geom, int geom_index, float& closest_dist, Vec4f& hit_normal_out) const;

	typedef js::Vector<BVHNode, 64> NODE_VECTOR_TYPE;
	typedef MollerTrumboreTri INTERSECT_TRI_TYPE;

	AABBox root_aabb; // AABB of whole thing
	NODE_VECTOR_TYPE nodes; // Nodes of the tree.
	js::Vector<TRI_INDEX, 64> leafgeom; // Indices into the intersect_tris array.
	std::vector<INTERSECT_TRI_TYPE> intersect_tris;
	
	const RayMesh* const raymesh;
};


} //end namespace js
