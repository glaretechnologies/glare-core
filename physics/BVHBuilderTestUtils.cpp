/*=====================================================================
BVHBuilderTestUtils.cpp
-----------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "BVHBuilderTestUtils.h"


#include "../utils/TestUtils.h"


#if BUILD_TESTS


namespace BVHBuilderTestUtils
{


// Recursively walk down tree, making sure that node AABBs are correct.
// Returns reference node AABB
#if 0
static js::AABBox checkNode(const js::Vector<ResultNode, 64>& result_nodes, int node_index, const BVHBuilder::ResultObIndicesVec& result_indices, const js::Vector<js::AABBox, 16>& aabbs)
{
	testAssert(node_index >= 0 && node_index < (int)result_nodes.size());
	const ResultNode& node = result_nodes[node_index];
	if(node.interior)
	{
		const js::AABBox left_aabb  = checkNode(result_nodes, node.left,  result_indices, aabbs);
		const js::AABBox right_aabb = checkNode(result_nodes, node.right, result_indices, aabbs);

		js::AABBox combined_aabb = left_aabb;
		combined_aabb.enlargeToHoldAABBox(right_aabb);

		testAssert(combined_aabb == node.aabb);

		return combined_aabb;
	}
	else // Else if leaf:
	{
		// Compute AABB of leaf from leaf object AABBs
		js::AABBox leaf_geom_aabb = js::AABBox::emptyAABBox();
		for(int i=node.left; i<node.right; ++i)
			leaf_geom_aabb.enlargeToHoldAABBox(aabbs[result_indices[i]]);

		//testAssert(aabb == node.aabb);
		testAssert(leaf_geom_aabb.containsAABBox(node.aabb));

		return leaf_geom_aabb;
	}
}
#endif


void testResultsValid(const BVHBuilder::ResultObIndicesVec& result_ob_indices, const js::Vector<ResultNode, 64>& result_nodes, size_t num_obs/*const js::Vector<js::AABBox, 16>& aabbs*/, bool duplicate_prims_allowed)
{
	//checkNode(result_nodes, /*node_index=*/0, result_ob_indices, aabbs);

	// Test that the resulting object indices are a permutation of the original indices.
	std::vector<bool> seen(num_obs, false);
	for(int z=0; z<(int)result_ob_indices.size(); ++z)
	{
		const uint32 ob_i = result_ob_indices[z];
		testAssert(ob_i < (uint32)num_obs);
		if(!duplicate_prims_allowed)
			testAssert(!seen[ob_i]);
		seen[ob_i] = true;
	}

	for(int z=0; z<(int)num_obs; ++z)
		testAssert(seen[z]);
	
	// Test that the result nodes object ranges cover all leaf objects, e.g. test that each object is in a leaf node.
	std::vector<bool> ob_in_leaf(result_ob_indices.size(), false);
	for(int n=0; n<(int)result_nodes.size(); ++n)
	{
		const ResultNode& node = result_nodes[n];
		if(node.interior) 
		{
			// TODO: check AABB
			testAssert(node.left >= 0  && node.left  < (int)result_nodes.size());
			testAssert(node.right >= 0 && node.right < (int)result_nodes.size());
		}
		else // Else if leaf node:
		{
			testAssert(node.left >= 0);
			testAssert(node.left <= node.right);
			testAssert(node.right <= (int)result_ob_indices.size());

			for(int z = node.left; z < node.right; ++z)
			{
				if(!duplicate_prims_allowed)
					testAssert(!ob_in_leaf[z]);
				ob_in_leaf[z] = true;
			}
		}
	}

	for(int z=0; z<(int)result_ob_indices.size(); ++z)
		testAssert(ob_in_leaf[z]);
}


#ifndef NO_EMBREE
void testResultsValid(const BVHBuilder::ResultObIndicesVec& result_ob_indices, const js::Vector<ResultInteriorNode, 64>& result_nodes, const js::Vector<js::AABBox, 16>& aabbs, bool duplicate_prims_allowed)
{
	// Test that the resulting object indices are a permutation of the original indices.
	std::vector<bool> seen(aabbs.size(), false);
	for(int z=0; z<(int)result_ob_indices.size(); ++z)
	{
		const uint32 ob_i = result_ob_indices[z];
		testAssert(ob_i < (uint32)aabbs.size());
		if(!duplicate_prims_allowed)
			testAssert(!seen[ob_i]);
		seen[ob_i] = true;
	}

	for(int z=0; z<(int)aabbs.size(); ++z)
		testAssert(seen[z]);

	// Test that the result nodes object ranges cover all leaf objects, e.g. test that each object is in a leaf node.
	std::vector<bool> ob_in_leaf(result_ob_indices.size(), false);
	for(int n=0; n<(int)result_nodes.size(); ++n)
	{
		const ResultInteriorNode& node = result_nodes[n];
		if(node.leftChildIsInterior())
		{
			// TODO: check AABB
			testAssert(node.left >= 0  && node.left  < (int)result_nodes.size());
		}
		else
		{
			testAssert(node.left >= 0);
			testAssert(node.left_num_prims > 0);
			
			for(int z = node.left; z < node.left + node.left_num_prims; ++z)
			{
				if(!duplicate_prims_allowed)
					testAssert(!ob_in_leaf[z]);
				ob_in_leaf[z] = true;
			}
		}

		if(node.rightChildIsInterior())
		{
			// TODO: check AABB
			testAssert(node.right >= 0  && node.right  < (int)result_nodes.size());
		}
		else
		{
			testAssert(node.right >= 0);
			testAssert(node.right_num_prims > 0);

			for(int z = node.right; z < node.right + node.right_num_prims; ++z)
			{
				if(!duplicate_prims_allowed)
					testAssert(!ob_in_leaf[z]);
				ob_in_leaf[z] = true;
			}
		}
	}

	for(int z=0; z<(int)result_ob_indices.size(); ++z)
		testAssert(ob_in_leaf[z]);
}
#endif // #ifndef NO_EMBREE


} // end namespace BVHBuilderTestUtils


#endif // BUILD_TESTS
