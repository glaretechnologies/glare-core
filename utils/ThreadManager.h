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
#include <set>


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
	typedef std::set<Reference<MessageableThread> > THREAD_SET_TYPE;
	THREAD_SET_TYPE threads;

	Mutex mutex; // Protects 'threads' set.
};
