/*=====================================================================
StackFrame.h
------------
File created by ClassTemplate on Sun Apr 16 17:42:55 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __STACKFRAME_H_666_
#define __STACKFRAME_H_666_

#include "../utils/platform.h"


namespace js
{

//should be 16 bytes all up
class StackFrame
{
public:
	StackFrame(){}
	StackFrame(uint32 node_, float tmin_, float tmax_)
		:	node(node_), tmin(tmin_), tmax(tmax_) {}

	uint32 node;
	float tmin;
	float tmax;

	uint32 padding;//to get to 16 bytes
};

} //end namespace js


#endif //__STACKFRAME_H_666_




