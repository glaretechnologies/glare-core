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
//#include "jscol_BadouelTri.h"
#include "KDTreeNode.h"
#include "jscol_aabbox.h"
#include "jscol_StackFrame.h"
#include "jscol_Intersectable.h"
#include "MollerTrumboreTri.h"
#include "../maths/SSE.h"
#include "../maths/vec3.h"
#include "../maths/PaddedVec3.h"
#include "../utils/Vector.h"
#include "../utils/platform.h"
#include <ostream>
#include <vector>
#include <map>
class RayMesh;
class Ray;
class HitInfo;
class FullHitInfo;
class ThreadContext;
namespace js
{
	class TriHash;
	class TriTreePerThreadData;
	class TreeStats;
//	class Threaded
}

namespace ThreadedBuilder { class ThreadedNLogNKDTreeBuilder; };


namespace js
{


/*=====================================================================
KDTree
-------
Kd-Tree
=====================================================================*/
SSE_CLASS_ALIGN KDTree : public Tree
{
	friend class FastKDTreeBuilder;
	friend class OldKDTreeBuilder;
	friend class ThreadedKDTreeBuilder;
	friend class NLogNKDTreeBuilder;
	friend class ThreadedBuilder::ThreadedNLogNKDTreeBuilder;
public:
	typedef uint32 NODE_INDEX;
	/*=====================================================================
	KDTree
	-------

	=====================================================================*/
	KDTree(RayMesh* raymesh);
	virtual ~KDTree();

	virtual void build(PrintOutput& print_output, bool verbose); // throws TreeExcep
	virtual bool diskCachable();
	virtual void buildFromStream(std::istream& stream, PrintOutput& print_output, bool verbose); // throws TreeExcep
	virtual void saveTree(std::ostream& stream);
	virtual uint32 checksum();

	virtual DistType traceRay(const Ray& ray, DistType max_t, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri, HitInfo& hitinfo_out) const;
	virtual const js::AABBox& getAABBoxWS() const;
	virtual void getAllHits(const Ray& ray, ThreadContext& thread_contex, const Object* object, std::vector<DistanceHitInfo>& hitinfos_out) const;
	virtual bool doesFiniteRayHit(const ::Ray& ray, Real raylength, ThreadContext& thread_context, const Object* object, unsigned int ignore_tri) const;

	//inline virtual const Vec3f triGeometricNormal(unsigned int tri_index) const; //slow

	virtual void printStats() const;
	virtual void printTraceStats() const;

	//For Debugging:
	Real traceRayAgainstAllTris(const Ray& ray, Real max_t, HitInfo& hitinfo_out) const;
	void getAllHitsAllTris(const Ray& ray, std::vector<DistanceHitInfo>& hitinfos_out) const;
	const std::vector<KDTreeNode>& getNodesDebug() const { return nodes; }
	static void test();


	static const unsigned int MAX_KDTREE_DEPTH = Tree::MAX_TREE_DEPTH;

	static const unsigned int DEFAULT_EMPTY_LEAF_NODE_INDEX = 0;
	static const unsigned int ROOT_NODE_INDEX = 1;

	typedef uint32 TRI_INDEX;
	typedef std::vector<KDTreeNode> NODE_VECTOR_TYPE;
	const Vec3f& triVertPos(TRI_INDEX tri_index, unsigned int vert_index_in_tri) const;


	AABBox root_aabb; // AABB of whole thing.


	///tracing stats///
	mutable uint64 num_traces;
	mutable uint64 num_root_aabb_hits;
	mutable uint64 total_num_nodes_touched;
	mutable uint64 total_num_leafs_touched;
	mutable uint64 total_num_tris_intersected;
	mutable uint64 total_num_tris_considered;

	class SortedBoundInfo
	{
	public:
		float lower, upper;
	};
	unsigned int calcMaxDepth() const;

	friend class KDTreeImpl;
	friend class DoesFiniteRayHitFunctions;
	friend class TraceRayFunctions;
	friend class GetAllHitsFunctions;

	typedef js::Vector<TRI_INDEX, 8> LEAF_GEOM_ARRAY_TYPE;

	TRI_INDEX numTris() const;
private:
	//-----------------typedefs------------------------
	//typedef uint32 TRI_INDEX;

	//typedef std::vector<AABBox> TRIBOX_VECTOR_TYPE;
	//typedef js::Vector<TreeNode> NODE_VECTOR_TYPE;
	//typedef js::Vector<AABBox> TRIBOX_VECTOR_TYPE;


	class TriInfo
	{
	public:
		TRI_INDEX tri_index;
		Vec3f lower;
		Vec3f upper;
	};


	void getTreeStats(TreeStats& stats_out, NODE_INDEX cur = ROOT_NODE_INDEX, unsigned int depth = 0) const;
	void printTree(NODE_INDEX currentnode, unsigned int depth, std::ostream& out);
	void debugPrintTree(NODE_INDEX cur, unsigned int depth);
	//void doWriteModel(unsigned int currentnode, const AABBox& node_aabb, std::ostream& stream, int& num_verts) const;
	void postBuild();
	void doBuild(NODE_INDEX cur,
		std::vector<std::vector<TriInfo> >& node_tri_layers,
		unsigned int depth, unsigned int maxdepth, const AABBox& cur_aabb, std::vector<SortedBoundInfo>& upper, std::vector<SortedBoundInfo>& lower);

	RayMesh* raymesh;

	NODE_VECTOR_TYPE nodes; // Nodes of the tree
	
	//typedef js::BadouelTri INTERSECT_TRI_TYPE;
	typedef MollerTrumboreTri INTERSECT_TRI_TYPE;
	INTERSECT_TRI_TYPE* intersect_tris;
	unsigned int num_intersect_tris;
	
	LEAF_GEOM_ARRAY_TYPE leafgeom;//indices into the 'edgetris' array

	unsigned int numnodesbuilt;
	unsigned int num_inseparable_tri_leafs;//num leafs formed when can't separate tris
	unsigned int num_cheaper_no_split_leafs;//num leafs formed when the cost function is cheaper to terminate splitting.
	unsigned int num_maxdepth_leafs;//num leafs formed because the max tree depth was hit
	unsigned int num_under_thresh_leafs;//num leafs formed because the number of tris was less than leaf threshold
	int num_empty_space_cutoffs;

	uint32 checksum_;
	bool calced_checksum;

	Real tree_specific_min_t;
};


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
	int max_num_leaf_tris;
	int max_depth;

	int total_node_mem;
	int leafgeom_indices_mem;
	int tri_mem;

	//build stats:
	int num_cheaper_no_split_leafs;
	int num_inseparable_tri_leafs;//num leafs formed when can't separate tris
	int num_maxdepth_leafs;//num leafs formed because the max tree depth was hit
	int num_under_thresh_leafs;//num leafs formed because the number of tris was less than leaf threshold
	int num_empty_space_cutoffs;

	std::map<int, int> leaf_geom_counts;

	void print();
};


/*const Vec3f KDTree::triGeometricNormal(unsigned int tri_index) const //slow
{
	return intersect_tris[tri_index].getNormal();
}*/


} //end namespace jscol


#endif //__TRITREE_H_666_
