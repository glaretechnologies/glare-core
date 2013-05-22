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

	virtual void build(PrintOutput& print_output); // throws TreeExcep
	virtual bool diskCachable() { return false; }
	virtual void buildFromStream(std::istream& stream) {} // throws TreeExcep
	virtual void saveTree(std::ostream& stream) {}
	virtual uint32 checksum() { return 0; }


	virtual double traceRay(const Ray& ray, double max_t, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object, HitInfo& hitinfo_out) const;
	virtual const js::AABBox& getAABBoxWS() const;
	virtual const std::string debugName() const { return "BVH"; }

	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, js::TriTreePerThreadData& context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const;

	virtual const Vec3f triGeometricNormal(unsigned int tri_index) const;

	virtual void printStats() const {}
	virtual void printTraceStats() const {}
	
	//float triCenter(unsigned int tri_index, unsigned int axis);
	//std::vector<Vec3f> tri_centers;

private:
	typedef uint32 TRI_INDEX;

	void triAABB(unsigned int tri_index, AABBox& aabb_out);

	void markLeafNode(const std::vector<std::vector<TRI_INDEX> >& tris, unsigned int node_index, int left, int right);

	void doBuild(const AABBox& aabb, std::vector<std::vector<TRI_INDEX> >& tris, std::vector<std::vector<TRI_INDEX> >& temp, 
		const std::vector<Vec3f>& tri_centers, int left, int right, unsigned int node_index_to_use, int depth);

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
	unsigned int intersect_tri_i;
	std::vector<TRI_INDEX> original_tri_index;
	std::vector<TRI_INDEX> new_tri_index;

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


#endif //__SIMPLEBVH_H_666_
