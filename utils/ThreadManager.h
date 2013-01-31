/*=====================================================================
ThreadManager.h
---------------
File created by ClassTemplate on Sat Nov 03 08:25:38 2007
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "threadsafequeue.h"
#include "MessageableThread.h"
#include "ThreadMessage.h"
#include <map>


/*=====================================================================
ThreadManager
-------------
Manages one or more MessageableThreads.
=====================================================================*/
class ThreadManager
{
public:
	ThreadManager();
	~ThreadManager();

	// Add a thread.  Launches the thread after adding it.
	void addThread(const Reference<MessageableThread>& t);
	
	// Enqueue a message for all the managed threads.
	void enqueueMessage(const ThreadMessage& m);

	// Send a kill message to all the managed threads, then blocks until all the threads have completed.
	void killThreadsBlocking();

	// Enqueues kill message to all threads, then returns immediately.
	void killThreadsNonBlocking(); 
	
	
	// Called by threads when they are finished running
	void threadFinished(MessageableThread* t);

	// Get number of threads that are being managed.
	unsigned int getNumThreads();


	// Run unit tests
	static void test();

private:
	typedef ThreadSafeQueue<Reference<ThreadMessage> > MESSAGE_QUEUE_TYPE;
	typedef std::map<Reference<MessageableThread>, MESSAGE_QUEUE_TYPE*> MESSAGE_QUEUE_MAP_TYPE;
	MESSAGE_QUEUE_MAP_TYPE message_queues;

	Mutex mutex;

	Condition thread_terminated_condition;
};
