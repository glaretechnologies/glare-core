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


/*=====================================================================
BVH
----
Triangle mesh acceleration structure.
=====================================================================*/
SSE_CLASS_ALIGN BVH : public Tree
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	BVH(const RayMesh* const raymesh);
	virtual ~BVH();

	virtual void build(PrintOutput& print_output, bool verbose, Indigo::TaskManager& task_manager); // throws TreeExcep
	virtual bool diskCachable() { return false; }
	virtual void buildFromStream(std::istream& stream, PrintOutput& print_output, bool verbose) {} // throws TreeExcep
	virtual void saveTree(std::ostream& stream) {}
	virtual uint32 checksum() { return 0; }


	virtual DistType traceRay(const Ray& ray, DistType max_t, ThreadContext& thread_context, HitInfo& hitinfo_out) const;
	virtual const js::AABBox& getAABBoxWS() const;
	virtual const std::string debugName() const { return "BVH"; }

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
	typedef js::Vector<BVHNode, 64> NODE_VECTOR_TYPE;
	typedef MollerTrumboreTri INTERSECT_TRI_TYPE;

	AABBox root_aabb; // AABB of whole thing
	NODE_VECTOR_TYPE nodes; // Nodes of the tree.
	js::Vector<TRI_INDEX, 64> leafgeom; // Indices into the intersect_tris array.
	std::vector<INTERSECT_TRI_TYPE> intersect_tris;
	
	js::Vector<js::AABBox, 64> tri_aabbs; // Triangle AABBs, used only during build process.
	const RayMesh* const raymesh;
};


} //end namespace js
