/*=====================================================================
ThreadShouldAbortCallback.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Tue Mar 09 15:48:35 +1300 2010
=====================================================================*/
#include "ThreadShouldAbortCallback.h"


#include "KillThreadMessage.h"


ThreadShouldAbortCallback::ThreadShouldAbortCallback(ThreadSafeQueue<Reference<ThreadMessage> >* message_queue_)
:	message_queue(message_queue_)
{
	assert(message_queue);
}


ThreadShouldAbortCallback::~ThreadShouldAbortCallback()
{

}


bool ThreadShouldAbortCallback::shouldAbort()
{
	bool suicide = false;

	Lock lock(message_queue->getMutex()); // Lock message queue.

	while(!message_queue->unlockedEmpty()) // While there are messages in the queue...
	{
		// Dequeue message.
		ThreadMessageRef message;
		message_queue->unlockedDequeue(message); 

		if(dynamic_cast<KillThreadMessage*>(message.getPointer()))
			suicide = true;
	}

	return suicide;
}
