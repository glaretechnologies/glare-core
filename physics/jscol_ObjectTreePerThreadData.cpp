/*=====================================================================
ObjectTreePerThreadData.cpp
---------------------------
File created by ClassTemplate on Sun Jul 02 00:24:06 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "jscol_ObjectTreePerThreadData.h"


#include "jscol_TriHash.h"


namespace js
{




ObjectTreePerThreadData::ObjectTreePerThreadData(int numobjects, int stacksize)
{
	nodestack_size = stacksize;
	//printVar(nodestack_size);
	//printVar(numobjects);
	alignedSSEArrayMalloc(nodestack_size, nodestack);

	last_test_time.resize(numobjects);
	for(int i=0; i<numobjects; ++i)
		last_test_time[i] = -1;

	hits.reserve(20);

	time = 0;
}


ObjectTreePerThreadData::~ObjectTreePerThreadData()
{
	alignedSSEFree(nodestack);
}






} //end namespace js






