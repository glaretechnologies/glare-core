/*=====================================================================
BVHBuilderTests.h
-------------------
Copyright Glare Technologies Limited 2015 -
Generated at 2015-09-28 16:25:21 +0100
=====================================================================*/
#pragma once


#include "BVHBuilder.h"
#include "../utils/Vector.h"


/*=====================================================================
BVHBuilderTests
-------------------

=====================================================================*/
namespace BVHBuilderTests
{


void test();

void testResultsValid(const BVHBuilder::ResultObIndicesVec& result_ob_indices, const js::Vector<ResultNode, 64>& result_nodes, size_t num_obs/*const js::Vector<js::AABBox, 16>& aabbs*/, bool duplicate_prims_allowed);

};



