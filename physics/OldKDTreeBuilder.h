/*=====================================================================
OldKDTreeBuilder.h
------------------
File created by ClassTemplate on Sun Mar 23 23:42:55 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __OLDKDTREEBUILDER_H_666_
#define __OLDKDTREEBUILDER_H_666_


#include "KDTree.h"
#include <vector>


namespace js
{


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


	void build(PrintOutput& print_output, KDTree& tree, const AABBox& cur_aabb, KDTree::NODE_VECTOR_TYPE& nodes_out, KDTree::LEAF_GEOM_ARRAY_TYPE& leaf_tri_indices_out);


	class SortedBoundInfo
	{
	public:
		float lower, upper;
	};

private:
	class TriInfo
	{
	public:
		KDTree::TRI_INDEX tri_index;
		Vec3f lower;
		Vec3f upper;
	};


	void doBuild(PrintOutput& print_output, KDTree& tree, KDTree::NODE_INDEX cur,
		std::vector<std::vector<TriInfo> >& node_tri_layers,
		unsigned int depth, unsigned int maxdepth, const AABBox& cur_aabb, std::vector<SortedBoundInfo>& upper, std::vector<SortedBoundInfo>& lower,
		KDTree::LEAF_GEOM_ARRAY_TYPE& leaf_tri_indices_out,
		std::vector<KDTreeNode>& nodes
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




