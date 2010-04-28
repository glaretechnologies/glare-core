/*=====================================================================
TriTreePerThreadData.h
----------------------
File created by ClassTemplate on Sun Jul 02 00:23:46 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TRITREEPERTHREADDATA_H_666_
#define __TRITREEPERTHREADDATA_H_666_


#include "KDTree.h"


namespace js
{


/*=====================================================================
TriTreePerThreadData
--------------------
Per-thread data structures needed for traversing KD-tree or BVH.
=====================================================================*/
class TriTreePerThreadData
{
public:
	TriTreePerThreadData();
	~TriTreePerThreadData();

	StackFrame* nodestack;
	int nodestack_size;

	std::vector<uint32> bvh_stack;

	js::TriHash* tri_hash;
};


} //end namespace js


#endif //__TRITREEPERTHREADDATA_H_666_
