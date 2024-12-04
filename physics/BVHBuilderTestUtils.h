/*=====================================================================
BVHBuilderTestUtils.h
---------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "BVHBuilder.h"
#include "EmbreeBVHBuilder.h"
#include "../utils/Vector.h"
struct ResultInteriorNode;


/*=====================================================================
BVHBuilderTestUtils
-------------------

=====================================================================*/
namespace BVHBuilderTestUtils
{

void testResultsValid(const BVHBuilder::ResultObIndicesVec& result_ob_indices, const js::Vector<ResultNode, 64>& result_nodes, size_t num_obs/*const js::Vector<js::AABBox, 16>& aabbs*/, bool duplicate_prims_allowed);

void testResultsValid(const BVHBuilder::ResultObIndicesVec& result_ob_indices, const js::Vector<ResultInteriorNode, 64>& result_nodes, const js::Vector<js::AABBox, 16>& aabbs, bool duplicate_prims_allowed);

};



