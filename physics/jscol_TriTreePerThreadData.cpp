/*=====================================================================
TriTreePerThreadData.cpp
------------------------
File created by ClassTemplate on Sun Jul 02 00:23:46 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "jscol_TriTreePerThreadData.h"


#include "jscol_TriHash.h"


namespace js
{


TriTreePerThreadData::TriTreePerThreadData()
{
	nodestack = NULL;
	nodestack_size = 0;

	nodestack_size = js::Tree::MAX_TREE_DEPTH + 1; //js::TriTree::MAX_KDTREE_DEPTH;//1024;
	SSE::alignedSSEArrayMalloc(nodestack_size, nodestack);


	tri_hash = (js::TriHash*)SSE::alignedMalloc(sizeof(js::TriHash), 64);//align to 64 bytes
	new(tri_hash) js::TriHash();
}


TriTreePerThreadData::~TriTreePerThreadData()
{
	SSE::alignedSSEFree(tri_hash);

	SSE::alignedSSEFree(nodestack);
	nodestack = NULL;

}


} //end namespace js
