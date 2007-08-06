/*=====================================================================
treenode.h
----------
File created by ClassTemplate on Fri Nov 05 01:54:56 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TREENODE_H_666_
#define __TREENODE_H_666_



#include "jscol_triangle.h"
//#include "jscol_ray.h"
#include <vector>
class Ray;

namespace js
{




/*=====================================================================
TreeNode
--------

=====================================================================*/
class TreeNode
{
public:
	/*=====================================================================
	TreeNode
	--------
	
	=====================================================================*/
	TreeNode(int depth);

	~TreeNode();

	//void addTri(const js::Triangle& tri);
	void build(const std::vector<js::Triangle*>& tris, int depth);

	//int getNumTris() const { return tris.size(); }

	float traceRay(const ::Ray& ray, float tmin, float tmax, js::Triangle*& hittri_out);
	float traceRayIterative(const ::Ray& ray, js::Triangle*& hittri_out);
	
	Vec3 min;
	Vec3 max;

	void calcAABB(const std::vector<js::Triangle*>& tris);

	void draw() const;

	//debugging/diagnosis stuff
	int getNumLeafTrisInSubtree() const;

	static int nodes_touched;

private:
	//std::vector<js::Triangle> tris;

	std::vector<js::Triangle*> leaftris;

	TreeNode* positive_child;
	TreeNode* negative_child;

	int dividing_axis;
	float dividing_val;

	//int depth;

};



} //end namespace js


#endif //__TREENODE_H_666_




