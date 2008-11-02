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

=====================================================================*/
class TriTreePerThreadData
{
public:
	/*=====================================================================
	TriTreePerThreadData
	--------------------
	
	=====================================================================*/
	TriTreePerThreadData();

	~TriTreePerThreadData();

	StackFrame* nodestack;
	int nodestack_size;

	js::TriHash* tri_hash;

};


} //end namespace js


#endif //__TRITREEPERTHREADDATA_H_666_
