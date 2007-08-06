/*=====================================================================
tritree.h
---------
File created by ClassTemplate on Fri Nov 05 01:09:27 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TRITREE_H_666_
#define __TRITREE_H_666_


#include "jscol_triangle.h"
#include <vector>
#include "jscol_treenode.h"
class Ray;


namespace js
{

//class TreeNode;


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

	void build();

	//returns dist till hit tri, neg number if missed.
	float traceRay(const ::Ray& ray, js::Triangle*& hittri_out);

	void draw() const;

	//debugging/diagnosis
	TreeNode* getRootNode(){ return rootnode; }

private:
	//TreeNode* rootnode;
	std::vector<TreeNode> nodes;

	std::vector<js::Triangle> tris;

};



} //end namespace jscol


#endif //__TRITREE_H_666_




