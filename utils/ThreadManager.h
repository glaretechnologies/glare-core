/*=====================================================================
ThreadManager.h
---------------
File created by ClassTemplate on Sat Nov 03 08:25:38 2007
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "ThreadSafeQueue.h"
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
	void enqueueMessage(const Reference<ThreadMessage>& m);

	// Send a kill message to all the managed threads, then blocks until all the threads have completed.
	void killThreadsBlocking();

	// Enqueues kill message to all threads, then returns immediately.
	void killThreadsNonBlocking(); 
	
	
	// Called by threads when they are finished running
	void threadFinished(MessageableThread* t);

	// Get number of threads that are being managed.
	unsigned int getNumThreads();

	Mutex& getMutex() { return mutex; }

	typedef std::set<Reference<MessageableThread> > THREAD_SET_TYPE;
	THREAD_SET_TYPE& getThreads() { return threads; }

	// Run unit tests
	static void test();

private:
	THREAD_SET_TYPE threads;

	Mutex mutex; // Protects 'threads' set.
};
