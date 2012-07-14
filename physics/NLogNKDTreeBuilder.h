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
NLogNKDTreeBuilder
------------------

=====================================================================*/
class NLogNKDTreeBuilder
{
public:
	NLogNKDTreeBuilder();

	~NLogNKDTreeBuilder();


	void build(
		PrintOutput& print_output, 
		bool verbose, 
		KDTree& tree, 
		const AABBox& cur_aabb, 
		KDTree::NODE_VECTOR_TYPE& nodes_out, 
		KDTree::LEAF_GEOM_ARRAY_TYPE& leaf_tri_indices_out);

	class LowerBound
	{
	public:
		float lower;
		uint32 tri_index;
	};
	class UpperBound
	{
	public:
		float upper;
		uint32 tri_index;
	};

	class LayerInfo
	{
	public:
		js::Vector<LowerBound, 4> lower_bounds[3]; // One for each axis
		js::Vector<UpperBound, 4> upper_bounds[3];
	};

private:
	void doBuild(
		PrintOutput& print_output, 
		bool verbose, 
		KDTree& tree, 
		KDTree::NODE_INDEX cur,
		unsigned int depth, 
		unsigned int maxdepth, 
		const AABBox& cur_aabb, 
		KDTree::NODE_VECTOR_TYPE& nodes_out, 
		KDTree::LEAF_GEOM_ARRAY_TYPE& leaf_tri_indices_out
	);
	
	std::vector<LayerInfo> layers;
	js::Vector<js::AABBox, 16> tri_aabbs;

	//Mutex write_mutex;
};


} //end namespace js


#endif //NLogNKDTreeBuilder_H_666_
