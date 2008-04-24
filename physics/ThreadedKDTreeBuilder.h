/*=====================================================================
OldKDTreeBuilder.h
------------------
File created by ClassTemplate on Sun Mar 23 23:42:55 2008
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __OLDKDTREEBUILDER_H_666_
#define __OLDKDTREEBUILDER_H_666_

#include "jscol_tritree.h"
#include "../utils/threadpool.h"
#include <vector>


namespace js
{

class TriTree;

/*=====================================================================
OldKDTreeBuilder
----------------

=====================================================================*/
class ThreadedKDTreeBuilder
{
public:
	/*=====================================================================
	OldKDTreeBuilder
	----------------
	
	=====================================================================*/
	ThreadedKDTreeBuilder();

	~ThreadedKDTreeBuilder();


	void build(TriTree& tree, const AABBox& cur_aabb, TriTree::NODE_VECTOR_TYPE& nodes_out, js::Vector<TriTree::TRI_INDEX>& leaf_tri_indices_out);


	class TriInfo
	{
	public:
		TriTree::TRI_INDEX tri_index;
		Vec3f lower;
		Vec3f upper;
	};

	class SortedBoundInfo
	{
	public:
		float lower, upper;
	};

	class IntermediateNode
	{
	public:
		/*float split_val;
		unsigned int split_axis;
		IntermediateNode* left;
		IntermediateNode* right;
		TriTree::TRI_INDEX* tri_indices;*/
		TriTree* tree;
		unsigned int depth;
		unsigned int maxdepth;
		AABBox aabb;
		IntermediateNode* parent;
		IntermediateNode* left;
		IntermediateNode* right;

		std::vector<TriInfo>* tris;
		TriTree::NODE_VECTOR_TYPE nodes_out;
		js::Vector<TriTree::TRI_INDEX> leaf_tri_indices_out;
	};





	class BuildTask
	{
	public:
		//ThreadedKDTreeBuilder* builder;

		/*uint32 parent_node_index;
		std::vector<TriInfo> tris;
		TriTree::NODE_VECTOR_TYPE nodes_out;
		js::Vector<TriTree::TRI_INDEX> leaf_tri_indices_out;*/

		IntermediateNode* parent_node;
		bool left_child;
		ThreadedKDTreeBuilder* builder;
		IntermediateNode* node;
	};

	Condition build_threads_finished;
	Mutex mutex;
	int num_subtrees_left_to_build;

	// Deletes tris when done with them
	void buildSubtree(
		const TriTree& tree, 
		TriTree::NODE_INDEX cur, 
		std::vector<TriInfo>* tris,
		unsigned int depth, unsigned int maxdepth, const AABBox& cur_aabb, 
		js::Vector<TriTree::TRI_INDEX>& leaf_tri_indices_out,
		std::vector<TreeNode>& nodes
		);

	void doBuild(
		IntermediateNode* node,
		const TriTree& tree, 
		TriTree::NODE_INDEX cur, 
		const std::vector<TriInfo>& tris,
		unsigned int depth, unsigned int maxdepth, const AABBox& cur_aabb, 
		js::Vector<TriTree::TRI_INDEX>& leaf_tri_indices_out,
		std::vector<TreeNode>& nodes
		);

private:
	

	
	
	


	int numnodesbuilt;
	int num_maxdepth_leafs;
	int num_under_thresh_leafs;
	int num_empty_space_cutoffs;
	int num_cheaper_no_split_leafs;
	int num_inseparable_tri_leafs;


	IntermediateNode* root_intermediate_node;

	ThreadPool<BuildTask> thread_pool;
};



} //end namespace js


#endif //__OLDKDTREEBUILDER_H_666_




