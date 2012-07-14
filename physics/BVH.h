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
//#include "jscol_BadouelTri.h"
#include "MollerTrumboreTri.h"
#include "../maths/vec3.h"
#include "../maths/SSE.h"
#include "../utils/Vector.h"


class RayMesh;
class FullHitInfo;


namespace js
{


/*=====================================================================
BVH
----

=====================================================================*/
SSE_CLASS_ALIGN BVH : public Tree
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	BVH(const RayMesh* const raymesh);
	~BVH();

	virtual void build(PrintOutput& print_output, bool verbose); // throws TreeExcep
	virtual bool diskCachable() { return false; }
	virtual void buildFromStream(std::istream& stream, PrintOutput& print_output, bool verbose) {} // throws TreeExcep
	virtual void saveTree(std::ostream& stream) {}
	virtual uint32 checksum() { return 0; }


	//intersectable interface
	virtual DistType traceRay(const Ray& ray, DistType max_t, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri, HitInfo& hitinfo_out) const;
	virtual const js::AABBox& getAABBoxWS() const;
	virtual const std::string debugName() const { return "BVH"; }
	//end

	virtual void getAllHits(const Ray& ray, ThreadContext& thread_context, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const;
	virtual bool doesFiniteRayHit(const ::Ray& ray, Real raylength, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri) const;

	//virtual const Vec3f triGeometricNormal(unsigned int tri_index) const;

	virtual void printStats() const {}
	virtual void printTraceStats() const {}

	friend class BVHImpl;
	friend class DoesFiniteRayHitFunctions;
	friend class TraceRayFunctions;
	friend class GetAllHitsFunctions;

	typedef uint32 TRI_INDEX;
private:
	
	
	void doBuild(const AABBox& aabb, std::vector<std::vector<TRI_INDEX> >& tris, std::vector<std::vector<TRI_INDEX> >& temp, 
		const std::vector<Vec3f>& tri_centers, int left, int right, int depth, unsigned int parent_index, unsigned int child_index);

	void markLeafNode(unsigned int parent_index, unsigned int child_index, int left, int right, const std::vector<std::vector<TRI_INDEX> >& tris);

	const Vec3f& triVertPos(unsigned int tri_index, unsigned int vert_index_in_tri) const;
	unsigned int numTris() const;

	AABBox root_aabb; // AABB of whole thing


	const RayMesh* const raymesh;

	typedef js::Vector<BVHNode, BVHNode::REQUIRED_ALIGNMENT> NODE_VECTOR_TYPE;
	NODE_VECTOR_TYPE nodes; // Nodes of the tree.


	js::Vector<js::AABBox, 16> tri_aabbs;
	//AABBox* tri_aabbs; // Triangle AABBs, used only during build process.

	typedef MollerTrumboreTri INTERSECT_TRI_TYPE;
	//js::Vector<INTERSECT_TRI_TYPE, 16> intersect_tris;
	std::vector<INTERSECT_TRI_TYPE> intersect_tris;
	
	//std::vector<TRI_INDEX> original_tri_index;
	//std::vector<TRI_INDEX> new_tri_index;

	std::vector<TRI_INDEX> leafgeom; // Indices into the intersect_tris array.

	js::Vector<float, 4> tri_max; // Used only during build process.

	//std::vector<Vec3f> tri_centers;
	//std::vector<float> centers;

	Real tree_specific_min_t;

	/// build stats ///
	int num_maxdepth_leaves;
	int num_under_thresh_leaves;
	int num_cheaper_nosplit_leaves;
	int num_could_not_split_leaves;
	int num_leaves;
	int max_num_tris_per_leaf;
	int leaf_depth_sum;
	int max_leaf_depth;
};


} //end namespace js


#endif //__BVH_H_666_
