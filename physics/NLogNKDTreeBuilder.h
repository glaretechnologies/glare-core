/*=====================================================================
OldKDTreeBuilder.h
------------------
File created by ClassTemplate on Sun Mar 23 23:42:55 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef NLogNKDTreeBuilder_H_666_
#define NLogNKDTreeBuilder_H_666_


#include "KDTree.h"
#include <vector>


namespace js
{


/*=====================================================================
OldKDTreeBuilder
----------------

=====================================================================*/
class NLogNKDTreeBuilder
{
public:
	/*=====================================================================
	OldKDTreeBuilder
	----------------

	=====================================================================*/
	NLogNKDTreeBuilder();

	~NLogNKDTreeBuilder();


	void build(PrintOutput& print_output, bool verbose, KDTree& tree, const AABBox& cur_aabb, KDTree::NODE_VECTOR_TYPE& nodes_out, KDTree::LEAF_GEOM_ARRAY_TYPE& leaf_tri_indices_out);


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


	void doBuild(PrintOutput& print_output, bool verbose, KDTree& tree, KDTree::NODE_INDEX cur,
		std::vector<std::vector<TriInfo> >& node_tri_layers,
		unsigned int depth, unsigned int maxdepth, const AABBox& cur_aabb, std::vector<SortedBoundInfo>& upper, std::vector<SortedBoundInfo>& lower,
		KDTree::LEAF_GEOM_ARRAY_TYPE& leaf_tri_indices_out,
		std::vector<KDTreeNode>& nodes
		);

private:
	std::vector<SortedBoundInfo> lower, upper; 
};



} //end namespace js


#endif //NLogNKDTreeBuilder_H_666_




