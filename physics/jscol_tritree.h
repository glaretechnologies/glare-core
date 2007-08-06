/*=====================================================================
tritree.h
---------
File created by ClassTemplate on Fri Nov 05 01:09:27 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TRITREE_H_666_
#define __TRITREE_H_666_


#include "jscol_triangle.h"
#include "jscol_Tree.h"
#include "jscol_BadouelTri.h"
#include "jscol_treenode.h"
#include "jscol_aabbox.h"
#include "jscol_StackFrame.h"
#include "jscol_Intersectable.h"
#include "../maths/vec3.h"
#include "../maths/PaddedVec3.h"
#include "../utils/Vector.h"
#include <ostream>
#include <vector>
class RayMesh;
class Ray;
class RayBundle;
class HitInfo;
class FullHitInfo;
namespace js 
{ 
	class TriHash;
	class TriTreePerThreadData;
}




namespace js
{





class TreeStats
{
public:
	TreeStats();
	~TreeStats();

	int total_num_nodes;//total number of nodes, both interior and leaf
	int num_interior_nodes;
	int num_leaf_nodes;
	int num_tris;//number of triangles stored
	int num_leaf_geom_tris;//number of references to tris held in leaf nodes
	double average_leafnode_depth;//average depth in tree of leaf nodes, where depth of 0 is root level.
	double average_numgeom_per_leafnode;//average num tri refs per leaf node
	int max_depth;

	int total_node_mem;
	int leafgeom_indices_mem;
	int tri_mem;

	//build stats:
	int num_inseparable_tri_leafs;//num leafs formed when can't separate tris
	int num_maxdepth_leafs;//num leafs formed because the max tree depth was hit
	int num_under_thresh_leafs;//num leafs formed because the number of tris was less than leaf threshold

};



/*=====================================================================
TriTree
-------
Kd-Tree
=====================================================================*/
class TriTree : public Tree
{
public:
	/*=====================================================================
	TriTree
	-------
	
	=====================================================================*/
	TriTree(RayMesh* raymesh);
	~TriTree();

	typedef float REAL;

	virtual void build();
	virtual bool diskCachable();
	virtual void buildFromStream(std::istream& stream);
	virtual void saveTree(std::ostream& stream);
	virtual unsigned int checksum();

	virtual double traceRay(const Ray& ray, double max_t, js::TriTreePerThreadData& context, HitInfo& hitinfo_out) const;
	virtual const js::AABBox& getAABBoxWS() const;
	virtual void getAllHits(const Ray& ray, js::TriTreePerThreadData& context, std::vector<FullHitInfo>& hitinfos_out) const;
	virtual bool doesFiniteRayHit(const ::Ray& ray, double raylength, js::TriTreePerThreadData& context) const;

	inline virtual const Vec3f& triGeometricNormal(unsigned int tri_index) const; //slow

	//For Debugging:
	double traceRayAgainstAllTris(const Ray& ray, double max_t, HitInfo& hitinfo_out) const;
	void getAllHitsAllTris(const Ray& ray, std::vector<FullHitInfo>& hitinfos_out) const;
	const std::vector<TreeNode>& getNodesDebug() const { return nodes; }

	static const unsigned int MAX_KDTREE_DEPTH = 64;

private:
	void printTree(unsigned int currentnode, unsigned int depth, std::ostream& out);
	void debugPrintTree(unsigned int cur, unsigned int depth);
	void getTreeStats(TreeStats& stats_out, unsigned int cur = 0, unsigned int depth = 0);
	const Vec3f& triVertPos(unsigned int tri_index, unsigned int vert_index_in_tri) const;
	unsigned int numTris() const;
	void AABBoxForTri(unsigned int tri_index, AABBox& aabbox_out);
	void printTraceStats() const;
	//void doWriteModel(unsigned int currentnode, const AABBox& node_aabb, std::ostream& stream, int& num_verts) const;
	void postBuild() const;


	//-----------------typedefs------------------------
	typedef unsigned int TRI_INDEX;
	typedef std::vector<TreeNode> NODE_VECTOR_TYPE;
	//typedef std::vector<AABBox> TRIBOX_VECTOR_TYPE;
	//typedef js::Vector<TreeNode> NODE_VECTOR_TYPE;
	typedef js::Vector<AABBox> TRIBOX_VECTOR_TYPE;




	void doBuild(unsigned int cur, 
		//const std::vector<TRI_INDEX>& tris, 
		std::vector<std::vector<TRI_INDEX> >& node_tri_layers,
		unsigned int depth, unsigned int maxdepth, const AABBox& cur_aabb, std::vector<float>& upper, std::vector<float>& lower);

	//float getCostForSplit(unsigned int cur, const std::vector<TRI_INDEX>& nodetris,
	//				  unsigned int axis, float splitval, const AABBox& aabb);

	//bool triIntersectsAABB(int triindex, const AABBox& aabb, int split_axis,
	//							bool is_neg_child);



	RayMesh* raymesh;
	int nodestack_size;

	NODE_VECTOR_TYPE nodes;//nodes of the tree

	typedef js::BadouelTri INTERSECT_TRI_TYPE;
	INTERSECT_TRI_TYPE* intersect_tris;
	unsigned int num_intersect_tris;

	//std::vector<TRI_INDEX> leafgeom;//indices into the 'edgetris' array
	js::Vector<TRI_INDEX> leafgeom;//indices into the 'edgetris' array

	unsigned int numnodesbuilt;
	unsigned int max_depth;
	unsigned int num_inseparable_tri_leafs;//num leafs formed when can't separate tris
	unsigned int num_maxdepth_leafs;//num leafs formed because the max tree depth was hit
	unsigned int num_under_thresh_leafs;//num leafs formed because the number of tris was less than leaf threshold

	SSE_ALIGN AABBox* root_aabb;//aabb of whole thing

	unsigned int checksum_;
	bool calced_checksum;

	///tracing stats///
	mutable double num_traces;
	mutable double total_num_nodes_touched;
	mutable double total_num_leafs_touched;
	mutable double total_num_tris_intersected;
};


const Vec3f& TriTree::triGeometricNormal(unsigned int tri_index) const //slow
{
	return intersect_tris[tri_index].getNormal();
}


} //end namespace jscol


#endif //__TRITREE_H_666_




