/*=====================================================================
BVH.h
-----
File created by ClassTemplate on Sun Oct 26 17:19:14 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __BVH_H_666_
#define __BVH_H_666_


#include "BVHNode.h"
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
class BVH : public Tree
{
public:
	BVH(RayMesh* raymesh);
	~BVH();

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

	friend class BVHImpl;
	friend class DoesFiniteRayHitFunctions;
	friend class TraceRayFunctions;
	friend class GetAllHitsFunctions;

private:
	typedef uint32 TRI_INDEX;
	
	void doBuild(const AABBox& aabb, std::vector<std::vector<TRI_INDEX> >& tris, std::vector<std::vector<TRI_INDEX> >& temp, 
		const std::vector<Vec3f>& tri_centers, int left, int right, int depth, unsigned int parent_index, unsigned int child_index);

	void markLeafNode(BVHNode* nodes, unsigned int parent_index, unsigned int child_index, int left, int right, const std::vector<std::vector<TRI_INDEX> >& tris);

	const Vec3f& triVertPos(unsigned int tri_index, unsigned int vert_index_in_tri) const;
	unsigned int numTris() const;

	//typedef std::vector<BVHNode> NODE_VECTOR_TYPE;

	RayMesh* raymesh;
	//NODE_VECTOR_TYPE nodes; // Nodes of the tree.
	BVHNode* nodes;
	unsigned int num_nodes;
	unsigned int nodes_capacity;

	//std::vector<BVHBuildNode> build_nodes;

	AABBox* root_aabb;//aabb of whole thing

	AABBox* tri_aabbs;

	typedef js::BadouelTri INTERSECT_TRI_TYPE;
	INTERSECT_TRI_TYPE* intersect_tris;
	unsigned int num_intersect_tris;
	unsigned int intersect_tri_i;
	std::vector<TRI_INDEX> original_tri_index;
	std::vector<TRI_INDEX> new_tri_index;

	//std::vector<TRI_INDEX> leafgeom;//indices into the intersect_tris array

	//std::vector<Vec3f> tri_centers;

	//std::vector<float> centers;

	/// build stats ///
	int num_maxdepth_leaves;
	int num_under_thresh_leaves;
	int num_cheaper_nosplit_leaves;
	int num_leaves;
	int max_num_tris_per_leaf;
	int leaf_depth_sum;
	int max_leaf_depth;
};


} //end namespace js


#endif //__BVH_H_666_
