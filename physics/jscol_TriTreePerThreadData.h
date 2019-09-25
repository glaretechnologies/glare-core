/*=====================================================================
TriTreePerThreadData.h
----------------------
Copyright Glare Technologies Limited 2019 -
=====================================================================*/
#pragma once


#include "jscol_Tree.h"
#include "../utils/Platform.h"


namespace js
{


/*=====================================================================
TriTreePerThreadData
--------------------
Per-thread data structures needed for traversing BVH.
=====================================================================*/
class TriTreePerThreadData
{
public:
	TriTreePerThreadData();
	~TriTreePerThreadData();

	uint32 bvh_stack[js::Tree::MAX_TREE_DEPTH + 1];
};


} //end namespace js
