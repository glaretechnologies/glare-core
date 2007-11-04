/*=====================================================================
MessageableThread.cpp
---------------------
File created by ClassTemplate on Sat Nov 03 09:15:49 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "MessageableThread.h"

#include "ThreadManager.h"



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



