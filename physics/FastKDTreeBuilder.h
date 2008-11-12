/*=====================================================================
FastKDTreeBuilder.h
-------------------
File created by ClassTemplate on Sun Mar 23 23:33:20 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __FASTKDTREEBUILDER_H_666_
#define __FASTKDTREEBUILDER_H_666_


#include "KDTree.h"
#include <vector>

namespace js
{

class Boundary 
{
public:
	KDTree::TRI_INDEX tri_index;
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


	void build(KDTree& tree, const AABBox& cur_aabb, KDTree::NODE_VECTOR_TYPE& nodes_out, js::Vector<KDTree::TRI_INDEX, 4>& leaf_tri_indices_out);


private:
	

	void buildSubTree(
		KDTree& tree, 
		const AABBox& cur_aabb,
		const std::vector<std::vector<Boundary> >& min_bounds, 
		const std::vector<std::vector<Boundary> >& max_bounds, 
		unsigned int depth,
		unsigned int maxdepth,
		KDTree::NODE_VECTOR_TYPE& nodes_out, 
		js::Vector<KDTree::TRI_INDEX, 4>& leaf_tri_indices_out);

};

} // End namespace js


#endif //__FASTKDTREEBUILDER_H_666_




