/*=====================================================================
ThreadShouldAbortCallback.h
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Tue Mar 09 15:48:35 +1300 2010
=====================================================================*/
#pragma once


#include "../networking/MySocket.h"
#include "../utils/ThreadSafeQueue.h"
#include "../utils/StreamShouldAbortCallback.h"
class ThreadMessage;


/*=====================================================================
ThreadShouldAbortCallback
-------------------

=====================================================================*/
class ThreadShouldAbortCallback : public StreamShouldAbortCallback
{
public:
	ThreadShouldAbortCallback(ThreadSafeQueue<Reference<ThreadMessage> >* message_queue);
	virtual ~ThreadShouldAbortCallback();

	virtual bool shouldAbort();

private:
	ThreadSafeQueue<Reference<ThreadMessage> >* message_queue;
};
