/*=====================================================================
ObjectTree.h
------------
File created by ClassTemplate on Sat Apr 15 18:59:20 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __OBJECTTREE_H_666_
#define __OBJECTTREE_H_666_


#include "../maths/SSE.h"
#include "../indigo/object.h"
#include "jscol_Intersectable.h"
#include "ObjectTreeNode.h"
#include "jscol_aabbox.h"
#include "jscol_StackFrame.h"
namespace js{ class TriTreePerThreadData; }
namespace js{ class ObjectTreePerThreadData; }
class HitInfo;
class PrintOutput;


namespace js
{


class ObjectTreeStats;


/*=====================================================================
ObjectTree
----------

=====================================================================*/
SSE_CLASS_ALIGN ObjectTree
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	ObjectTree();
	~ObjectTree();

	typedef float Real;

	typedef Object INTERSECTABLE_TYPE;
	//typedef Intersectable INTERSECTABLE_TYPE;


	void insertObject(INTERSECTABLE_TYPE* intersectable);
	void build(PrintOutput& print_output, bool verbose);

	//js::ObjectTreePerThreadData* allocContext() const;

	// Returns distance untill hit or negative if missed
	Real traceRay(const Ray& ray, Real ray_length, ThreadContext& thread_context, double time, 
		const INTERSECTABLE_TYPE*& hitob_out, HitInfo& hitinfo_out) const;

	INDIGO_STRONG_INLINE const js::AABBox& getAABBoxWS() const { return root_aabb; }

	///-------- debugging methods ---------///
	Real traceRayAgainstAllObjects(const Ray& ray, ThreadContext& thread_context/*, js::ObjectTreePerThreadData& object_context*/, double time, const INTERSECTABLE_TYPE*& hitob_out, HitInfo& hitinfo_out) const;
	void writeTreeModel(std::ostream& stream);
	void printTree(int currentnode, int depth, std::ostream& out);
	void getTreeStats(ObjectTreeStats& stats_out, int cur = 0, int depth = 0);
	///------- end debugging methods --------///

	class SortedBoundInfo
	{
	public:
		float lower, upper;
	};

private:
	bool doesEnvSphereObjectIntersectAABB(INTERSECTABLE_TYPE* ob, const AABBox& aabb);
	void doBuild(int cur, const std::vector<INTERSECTABLE_TYPE*>& objs, 
					int depth, int maxdepth, const AABBox& cur_aabb,
					std::vector<SortedBoundInfo>& upper,
					std::vector<SortedBoundInfo>& lower);

	bool intersectableIntersectsAABB(INTERSECTABLE_TYPE* ob, const AABBox& aabb, int split_axis,
								bool is_neg_child);

	void doWriteModel(int currentnode, const AABBox& node_aabb, std::ostream& stream, int& num_verts) const;

	AABBox root_aabb; // AABB of whole thing

	std::vector<INTERSECTABLE_TYPE*> objects;
	std::vector<INTERSECTABLE_TYPE*> leafgeom;

	int max_depth;
	int nodestack_size;

	typedef std::vector<ObjectTreeNode> NODE_VECTOR_TYPE;
	NODE_VECTOR_TYPE nodes; // Nodes of the tree

	int num_inseparable_tri_leafs; // Num leaves formed when can't separate tris
	int num_maxdepth_leafs; // Num leaves formed because the max tree depth was hit
	int num_under_thresh_leafs; // Num leaves formed because the number of tris was less than leaf threshold

	int max_object_index;
};


class ObjectTreeStats
{
public:
	ObjectTreeStats();
	~ObjectTreeStats();

	int total_num_nodes; // total number of nodes, both interior and leaf
	int num_interior_nodes;
	int num_leaf_nodes;
	int num_objects; // number of triangles stored
	int num_leafgeom_objects; // number of references to tris held in leaf nodes
	double average_leafnode_depth;//average depth in tree of leaf nodes, where depth of 0 is root level.
	double average_objects_per_leafnode;//average num tri refs per leaf node
	int max_leaf_objects;
	int max_leafnode_depth;
	int max_depth;

	int total_node_mem;
	int leafgeom_indices_mem;
	int tri_mem;

	//build stats:
	int num_inseparable_tri_leafs;//num leafs formed when can't separate tris
	int num_maxdepth_leafs;//num leafs formed because the max tree depth was hit
	int num_under_thresh_leafs;//num leafs formed because the number of tris was less than leaf threshold
};


} //end namespace js


#endif //__OBJECTTREE_H_666_
