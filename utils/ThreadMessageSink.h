/*=====================================================================
ThreadMessageSink.h
-------------------
File created by ClassTemplate on Mon Dec 28 17:12:22 2009
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "../utils/reference.h"
class ThreadMessage;


/*=====================================================================
ThreadMessageSink
-----------------

=====================================================================*/
class ThreadMessageSink
{
public:
	ThreadMessageSink();

	virtual ~ThreadMessageSink();


	virtual void handleMessage(const Reference<ThreadMessage>& msg) = 0;
};
