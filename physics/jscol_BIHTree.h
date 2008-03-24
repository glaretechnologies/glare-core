/*=====================================================================
BIHTree.h
---------
File created by ClassTemplate on Sun Nov 26 21:59:32 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __BIHTREE_H_666_
#define __BIHTREE_H_666_

#include "jscol_Tree.h"
#include "jscol_BIHTreeNode.h"
#include "jscol_Intersectable.h"
#include "jscol_BadouelTri.h"
#include "../maths/vec3.h"
#include "../maths/SSE.h"

class RayMesh;
class FullHitInfo;

namespace js
{

/*=====================================================================
BIHTree
-------

=====================================================================*/
class BIHTree : public Tree
{
public:
	/*=====================================================================
	BIHTree
	-------
	
	=====================================================================*/
	BIHTree(RayMesh* raymesh);

	~BIHTree();

	virtual void build();
	virtual bool diskCachable();
	virtual void buildFromStream(std::istream& stream);
	virtual void saveTree(std::ostream& stream);
	virtual uint32 checksum();

	virtual double traceRay(const Ray& ray, double max_t, js::TriTreePerThreadData& context, HitInfo& hitinfo_out) const;
	virtual const js::AABBox& getAABBoxWS() const;
	virtual void getAllHits(const Ray& ray, js::TriTreePerThreadData& context, std::vector<FullHitInfo>& hitinfos_out) const;
	virtual bool doesFiniteRayHit(const ::Ray& ray, double raylength, js::TriTreePerThreadData& context) const;
	virtual const std::string debugName() const { return "BIH"; }

	virtual const Vec3f& triGeometricNormal(unsigned int tri_index) const;

	virtual void printStats() const {};
	virtual void printTraceStats() const {};

	double traceRayAgainstAllTris(const Ray& ray, double tmax, HitInfo& hitinfo_out) const;

	typedef float REAL;

private:
	//typedefs
	typedef uint32 TRI_INDEX;
	typedef std::vector<BIHTreeNode> NODE_VECTOR_TYPE;

	
	void doBuild(const AABBox& aabb, const AABBox& tri_aabb, std::vector<TRI_INDEX>& tris, int left, int right, unsigned int node_index_to_use, int depth);
	float triMaxPos(unsigned int tri_index, unsigned int axis) const;
	float triMinPos(unsigned int tri_index, unsigned int axis) const;
	const Vec3f& triVertPos(unsigned int tri_index, unsigned int vert_index_in_tri) const;
	unsigned int numTris() const;
	unsigned int numInternalNodes() const;
	unsigned int numLeafNodes() const;
	double meanNumTrisPerLeaf() const;
	unsigned int maxNumTrisPerLeaf() const;
	//void AABBoxForTri(unsigned int tri_index, AABBox& aabbox_out);


	RayMesh* raymesh;

	NODE_VECTOR_TYPE nodes;//nodes of the tree

	SSE_ALIGN AABBox* root_aabb;//aabb of whole thing

	typedef js::BadouelTri INTERSECT_TRI_TYPE;
	INTERSECT_TRI_TYPE* intersect_tris;
	unsigned int num_intersect_tris;

	std::vector<TRI_INDEX> leafgeom;//indices into the intersect_tris array


	///build stats///
	int num_inseparable_tri_leafs;
	int num_maxdepth_leafs;
	int num_under_thresh_leafs;

	///tracing stats///
	mutable double num_traces;
	mutable double total_num_nodes_touched;
	mutable double total_num_leafs_touched;
	mutable double total_num_tris_intersected;
};




} //end namespace js


#endif //__BIHTREE_H_666_



















