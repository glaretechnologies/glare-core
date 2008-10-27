/*=====================================================================
SimpleBVH.h
-----------
File created by ClassTemplate on Mon Oct 27 20:35:09 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __SIMPLEBVH_H_666_
#define __SIMPLEBVH_H_666_


#include "SimpleBVHNode.h"
#include "jscol_Tree.h"
#include "jscol_Intersectable.h"
#include "jscol_BadouelTri.h"
#include "../maths/vec3.h"
#include "../maths/SSE.h"

class RayMesh;
class FullHitInfo;


namespace js
{


/*=====================================================================
NBVH
----

=====================================================================*/
class SimpleBVH : public Tree
{
public:
	SimpleBVH(RayMesh* raymesh);
	~SimpleBVH();

	virtual void build(); // throws TreeExcep
	virtual bool diskCachable() { return false; }
	virtual void buildFromStream(std::istream& stream) {} // throws TreeExcep
	virtual void saveTree(std::ostream& stream) {}
	virtual uint32 checksum() { return 0; }


	//intersectable interface
	virtual double traceRay(const Ray& ray, double max_t, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const;
	virtual const js::AABBox& getAABBoxWS() const;
	virtual const std::string debugName() const { return "BVH"; }
	//end

	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const;
	virtual bool doesFiniteRayHit(const ::Ray& ray, double raylength, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object) const;

	virtual const Vec3f& triGeometricNormal(unsigned int tri_index) const;

	virtual void printStats() const {}
	virtual void printTraceStats() const {}

private:
	typedef uint32 TRI_INDEX;
	
	void doBuild(const AABBox& aabb, std::vector<TRI_INDEX>& tris, int left, int right, unsigned int node_index_to_use, int depth);

	const Vec3f& triVertPos(unsigned int tri_index, unsigned int vert_index_in_tri) const;
	unsigned int numTris() const;


	RayMesh* raymesh;
	SimpleBVHNode* nodes;
	unsigned int num_nodes;
	unsigned int nodes_capacity;


	AABBox* root_aabb;//aabb of whole thing

	AABBox* tri_aabbs;

	typedef js::BadouelTri INTERSECT_TRI_TYPE;
	INTERSECT_TRI_TYPE* intersect_tris;
	unsigned int num_intersect_tris;

	std::vector<TRI_INDEX> leafgeom;//indices into the intersect_tris array

	std::vector<Vec3f> tri_centers;

	std::vector<float> centers;
};


} //end namespace js

#endif //__SIMPLEBVH_H_666_




