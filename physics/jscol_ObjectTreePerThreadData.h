/*=====================================================================
ObjectTreePerThreadData.h
-------------------------
File created by ClassTemplate on Sun Jul 02 00:24:06 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __OBJECTTREEPERTHREADDATA_H_666_
#define __OBJECTTREEPERTHREADDATA_H_666_

class StackFrame;
#include "jscol_tritree.h"
#include <vector>
#include "../indigo/DistanceFullHitInfo.h"

namespace js
{

/*=====================================================================
ObjectTreePerThreadData
-----------------------

=====================================================================*/
class ObjectTreePerThreadData
{
public:
	/*=====================================================================
	ObjectTreePerThreadData
	-----------------------
	
	=====================================================================*/
	ObjectTreePerThreadData(int numobjects, int stacksize);

	~ObjectTreePerThreadData();



	StackFrame* nodestack;
	int nodestack_size;

	std::vector<int> last_test_time;

	std::vector<DistanceFullHitInfo> hits;

	int time;
};



} //end namespace js


#endif //__OBJECTTREEPERTHREADDATA_H_666_




