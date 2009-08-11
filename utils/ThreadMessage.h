/*=====================================================================
ThreadMessage.h
---------------
File created by ClassTemplate on Sat Nov 03 08:25:44 2007
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __THREADMESSAGE_H_666_
#define __THREADMESSAGE_H_666_


#include <string>


/*=====================================================================
ThreadMessage
-------------

=====================================================================*/
class ThreadMessage
{
public:
	/*=====================================================================
	ThreadMessage
	-------------
	
	=====================================================================*/
	ThreadMessage();

	virtual ~ThreadMessage();


	virtual ThreadMessage* clone() const = 0;

	virtual const std::string debugName() const { return "ThreadMessage"; }

};


#endif //__THREADMESSAGE_H_666_
