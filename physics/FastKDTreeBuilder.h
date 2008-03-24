/*=====================================================================
FastKDTreeBuilder.h
-------------------
File created by ClassTemplate on Sun Mar 23 23:33:20 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __FASTKDTREEBUILDER_H_666_
#define __FASTKDTREEBUILDER_H_666_


#include "jscol_tritree.h"
#include <vector>

namespace js
{

class Boundary 
{
public:
	TriTree::TRI_INDEX tri_index;
	float lower, upper;
};

/*=====================================================================
FastKDTreeBuilder
-----------------

=====================================================================*/
class FastKDTreeBuilder
{
public:
	/*=====================================================================
	FastKDTreeBuilder
	-----------------
	
	=====================================================================*/
	FastKDTreeBuilder();

	~FastKDTreeBuilder();


	void build(TriTree& tree, const AABBox& cur_aabb, TriTree::NODE_VECTOR_TYPE& nodes_out, js::Vector<TriTree::TRI_INDEX>& leaf_tri_indices_out);


private:
	

	void buildSubTree(
		TriTree& tree, 
		const AABBox& cur_aabb,
		const std::vector<std::vector<Boundary> >& min_bounds, 
		const std::vector<std::vector<Boundary> >& max_bounds, 
		unsigned int depth,
		unsigned int maxdepth,
		TriTree::NODE_VECTOR_TYPE& nodes_out, 
		js::Vector<TriTree::TRI_INDEX>& leaf_tri_indices_out);

};

} // End namespace js


#endif //__FASTKDTREEBUILDER_H_666_




