/*=====================================================================
tritree.h
---------
File created by ClassTemplate on Fri Nov 05 01:09:27 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TRITREE_H_666_
#define __TRITREE_H_666_


#include "jscol_triangle.h"
#include "jscol_edgetri.h"
#include <vector>
#include "jscol_treenode.h"
#include "jscol_aabbox.h"
#include "../maths/vec3.h"
#include <ostream>
#include "../utils/bigarray.h"
class Ray;


namespace js
{

//class TreeNode;

class StackFrame
{
public:
	StackFrame(){}
	StackFrame(int node_, float tmin_, float tmax_)
		:	node(node_), tmin(tmin_), tmax(tmax_) {}

	int node;
	float tmin;
	float tmax;

	//int padding;//to get to 16 bytes
};

/*=====================================================================
TriTree
-------

=====================================================================*/
class TriTree
{
public:
	/*=====================================================================
	TriTree
	-------
	
	=====================================================================*/
	TriTree();

	~TriTree();


	void insertTri(const js::Triangle& tri);
	void reserveTriMem(int numtris);//not needed, but speeds things up

	void build();

	//returns dist till hit tri, neg number if missed.
	float traceRay(const ::Ray& ray, js::EdgeTri*& hittri_out);

	void draw() const;

	//debugging/diagnosis
	//TreeNode& getRootNode(){ return nodes[0]; }//rootnode; }

	const AABBox& getAABB() const { return root_aabb; }

	int getNumNodes() const { return nodes.size(); }
	int getNumLeafTris() const { return leafgeom.size(); }

	void printTree(int currentnode, int depth, std::ostream& out); 

private:
	typedef unsigned int TRI_INDEX;

	typedef std::vector<TreeNode> NODE_VECTOR_TYPE;
	typedef std::vector<js::Triangle> TRIANGLE_VECTOR_TYPE;
	typedef std::vector<js::EdgeTri> EDGETRI_VECTOR_TYPE;
	typedef std::vector<AABBox> TRIBOX_VECTOR_TYPE;
	/*typedef BigArray<TreeNode> NODE_VECTOR_TYPE;
	typedef BigArray<js::Triangle> TRIANGLE_VECTOR_TYPE;
	typedef BigArray<js::EdgeTri> EDGETRI_VECTOR_TYPE;
	typedef BigArray<AABBox> TRIBOX_VECTOR_TYPE;*/

	void doBuild(int cur, const std::vector<TRI_INDEX>& tris, 
					int depth, int maxdepth, const AABBox& cur_aabb);

	float getCostForSplit(int cur, const std::vector<TRI_INDEX>& nodetris,
					  int axis, float splitval, const AABBox& aabb);

	bool triIntersectsAABB(int triindex, const AABBox& aabb, int split_axis,
								bool is_neg_child);

	std::vector<StackFrame> nodestack;

	//TreeNode* rootnode;
	NODE_VECTOR_TYPE nodes;

	TRIANGLE_VECTOR_TYPE tris;

	EDGETRI_VECTOR_TYPE edgetris;


	//std::vector<js::EdgeTri*> leafgeom;
	std::vector<TRI_INDEX> leafgeom;

	TRIBOX_VECTOR_TYPE tri_boxes;

	AABBox root_aabb;

	int numnodesbuilt;
};



} //end namespace jscol


#endif //__TRITREE_H_666_




