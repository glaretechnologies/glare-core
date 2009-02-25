/*=====================================================================
MessageableThread.cpp
---------------------
File created by ClassTemplate on Sat Nov 03 09:15:49 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "MessageableThread.h"


#include "ThreadManager.h"
#include "../utils/KillThreadMessage.h"


MessageableThread::MessageableThread()
:	mesthread_message_queue(NULL),
	mesthread_thread_manager(NULL)
{
	
}


MessageableThread::~MessageableThread()
{
	assert(mesthread_thread_manager);
	mesthread_thread_manager->threadTerminating(this);
}



void MessageableThread::set(ThreadManager* thread_manager, ThreadSafeQueue<ThreadMessage*>* message_queue)
{
	assert(!mesthread_message_queue && !mesthread_thread_manager);
	mesthread_thread_manager = thread_manager;
	mesthread_message_queue = message_queue;
}


bool MessageableThread::deleteQueuedMessages() // Returns true if a KillThreadMessage was in the queue.
{
	bool found_kill_thread_message = false;
	
	Lock lock(getMessageQueue().getMutex());
		
	while(!getMessageQueue().unlockedEmpty())
	{
		ThreadMessage* message;
		getMessageQueue().unlockedDequeue(message);
		found_kill_thread_message = found_kill_thread_message || dynamic_cast<KillThreadMessage*>(message) != NULL;
		delete message;
	}

	return found_kill_thread_message;
}
