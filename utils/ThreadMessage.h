/*=====================================================================
ThreadMessage.h
---------------
File created by ClassTemplate on Sat Nov 03 08:25:44 2007
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "ThreadSafeRefCounted.h"
#include "reference.h"
#include <string>


/*=====================================================================
ThreadMessage
-------------

=====================================================================*/
class ThreadMessage : public ThreadSafeRefCounted
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


typedef Reference<ThreadMessage> ThreadMessageRef;
