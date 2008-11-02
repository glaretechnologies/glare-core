/*=====================================================================
ObjectTreePerThreadData.h
-------------------------
File created by ClassTemplate on Sun Jul 02 00:24:06 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __OBJECTTREEPERTHREADDATA_H_666_
#define __OBJECTTREEPERTHREADDATA_H_666_

#include "KDTree.h"
#include <vector>
#include <map>
#include "../indigo/DistanceHitInfo.h"
#include "jscol_TriTreePerThreadData.h"

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
	ObjectTreePerThreadData(bool root/*int numobjects, int stacksize*/);

	~ObjectTreePerThreadData();



	StackFrame* nodestack;
	int nodestack_size;

	std::vector<int> last_test_time;

	std::vector<DistanceHitInfo> hits;

	int time;



	TriTreePerThreadData tritree_context;
	
	ObjectTreePerThreadData* object_context;

	//std::map<int, ObjectTreePerThreadData> object_data;
};



} //end namespace js


#endif //__OBJECTTREEPERTHREADDATA_H_666_




