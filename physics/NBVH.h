/*=====================================================================
NBVH.h
------
File created by ClassTemplate on Sun Oct 26 01:59:14 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __NBVH_H_666_
#define __NBVH_H_666_


#include "NBVHNode.h"
#include "jscol_Tree.h"
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
class NBVH : public Tree
{
public:
	/*=====================================================================
	NBVH
	----
	
	=====================================================================*/
	NBVH(RayMesh* raymesh);

	~NBVH();


	virtual void build(); // throws TreeExcep
	virtual bool diskCachable() { return false; }
	virtual void buildFromStream(std::istream& stream) {} // throws TreeExcep
	virtual void saveTree(std::ostream& stream) {}
	virtual uint32 checksum() { return 0; }


	//intersectable interface
	virtual double traceRay(const Ray& ray, double max_t, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const;
	virtual const js::AABBox& getAABBoxWS() const;
	virtual const std::string debugName() const { return "N-BVH"; }
	//end

	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const;

	virtual const Vec3f triGeometricNormal(unsigned int tri_index) const;

	virtual void printStats() const {}
	virtual void printTraceStats() const {}

private:
	typedef uint32 TRI_INDEX;
	
	void doBuild(std::vector<TRI_INDEX>& tris, int left, int right, unsigned int node_index_to_use, int depth);

	const Vec3f& triVertPos(unsigned int tri_index, unsigned int vert_index_in_tri) const;
	unsigned int numTris() const;

	typedef std::vector<NBVHNode> NODE_VECTOR_TYPE;



	RayMesh* raymesh;
	NODE_VECTOR_TYPE nodes; // Nodes of the tree.

	std::vector<NBVHBuildNode> build_nodes;

	AABBox* root_aabb;//aabb of whole thing

	AABBox* tri_aabbs;

	typedef js::BadouelTri INTERSECT_TRI_TYPE;
	INTERSECT_TRI_TYPE* intersect_tris;
	unsigned int num_intersect_tris;

	std::vector<TRI_INDEX> leafgeom;//indices into the intersect_tris array
};


} //end namespace js


#endif //__NBVH_H_666_
