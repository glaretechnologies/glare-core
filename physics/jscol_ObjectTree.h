/*=====================================================================
ObjectTree.h
------------
File created by ClassTemplate on Sat Apr 15 18:59:20 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __OBJECTTREE_H_666_
#define __OBJECTTREE_H_666_

#include "../indigo/object.h"
#include "jscol_Intersectable.h"
#include "ObjectTreeNode.h"
#include "jscol_aabbox.h"
#include "jscol_StackFrame.h"
namespace js{ class TriTreePerThreadData; }
namespace js{ class ObjectTreePerThreadData; }
class HitInfo;

namespace js
{





class ObjectTreeStats
{
public:
	ObjectTreeStats();
	~ObjectTreeStats();

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
ObjectTree
----------

=====================================================================*/
class ObjectTree
{
public:
	/*=====================================================================
	ObjectTree
	----------
	
	=====================================================================*/
	ObjectTree();
	~ObjectTree();

	typedef float REAL;

	//typedef Object INTERSECTABLE_TYPE;
	typedef Intersectable INTERSECTABLE_TYPE;


	void insertObject(INTERSECTABLE_TYPE* intersectable);
	void build();

	//js::ObjectTreePerThreadData* allocContext() const;

	//returns dist till hit or negative if missed
	double traceRay(const Ray& ray, /*js::TriTreePerThreadData& tritree_context, */js::ObjectTreePerThreadData& object_context, const INTERSECTABLE_TYPE*& hitob_out, HitInfo& hitinfo_out) const;

	bool doesFiniteRayHit(const Ray& ray, double length, /*js::TriTreePerThreadData& tritree_context, */js::ObjectTreePerThreadData& object_context) const;

	const js::AABBox& getAABBoxWS() const { return *root_aabb; }

	///-------- debugging methods ---------///
	double traceRayAgainstAllObjects(const Ray& ray, /*js::TriTreePerThreadData& tritree_context, */js::ObjectTreePerThreadData& object_context, const INTERSECTABLE_TYPE*& hitob_out, HitInfo& hitinfo_out) const;
	bool allObjectsDoesFiniteRayHitAnything(const Ray& ray, double length, /*js::TriTreePerThreadData& tritree_context, */js::ObjectTreePerThreadData& object_context) const;
	void writeTreeModel(std::ostream& stream);
	void printTree(int currentnode, int depth, std::ostream& out);
	void getTreeStats(ObjectTreeStats& stats_out, int cur = 0, int depth = 0);
	///------- end debugging methods --------///

private:
	void doBuild(int cur, const std::vector<INTERSECTABLE_TYPE*>& objs, 
					int depth, int maxdepth, const AABBox& cur_aabb);

	bool intersectableIntersectsAABB(INTERSECTABLE_TYPE* ob, const AABBox& aabb, int split_axis,
								bool is_neg_child);

	void doWriteModel(int currentnode, const AABBox& node_aabb, std::ostream& stream, int& num_verts) const;

	SSE_ALIGN AABBox* root_aabb;//aabb of whole thing

	std::vector<INTERSECTABLE_TYPE*> objects;
	std::vector<INTERSECTABLE_TYPE*> leafgeom;

	int max_depth;
	int nodestack_size;

	typedef std::vector<ObjectTreeNode> NODE_VECTOR_TYPE;
	NODE_VECTOR_TYPE nodes;//nodes of the tree

	int num_inseparable_tri_leafs;//num leafs formed when can't separate tris
	int num_maxdepth_leafs;//num leafs formed because the max tree depth was hit
	int num_under_thresh_leafs;//num leafs formed because the number of tris was less than leaf threshold
};



} //end namespace js


#endif //__OBJECTTREE_H_666_




