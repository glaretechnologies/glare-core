/*=====================================================================
OldKDTreeBuilder.h
------------------
File created by ClassTemplate on Sun Mar 23 23:42:55 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __OLDKDTREEBUILDER_H_666_
#define __OLDKDTREEBUILDER_H_666_

#include "jscol_tritree.h"
#include <vector>


namespace js
{

class TriTree;

/*=====================================================================
OldKDTreeBuilder
----------------

=====================================================================*/
class OldKDTreeBuilder
{
public:
	/*=====================================================================
	OldKDTreeBuilder
	----------------
	
	=====================================================================*/
	OldKDTreeBuilder();

	~OldKDTreeBuilder();


	void build(TriTree& tree, const AABBox& cur_aabb, TriTree::NODE_VECTOR_TYPE& nodes_out, js::Vector<TriTree::TRI_INDEX>& leaf_tri_indices_out);


	class SortedBoundInfo
	{
	public:
		float lower, upper;
	};

private:
	class TriInfo
	{
	public:
		TriTree::TRI_INDEX tri_index;
		Vec3f lower;
		Vec3f upper;
	};

	
	void doBuild(TriTree& tree, TriTree::NODE_INDEX cur, 
		std::vector<std::vector<TriInfo> >& node_tri_layers,
		unsigned int depth, unsigned int maxdepth, const AABBox& cur_aabb, std::vector<SortedBoundInfo>& upper, std::vector<SortedBoundInfo>& lower,
		js::Vector<TriTree::TRI_INDEX>& leaf_tri_indices_out,
		std::vector<TreeNode>& nodes
		);


	int numnodesbuilt;
	int num_maxdepth_leafs;
	int num_under_thresh_leafs;
	int num_empty_space_cutoffs;
	int num_cheaper_no_split_leafs;
	int num_inseparable_tri_leafs;
};



} //end namespace js


#endif //__OLDKDTREEBUILDER_H_666_




