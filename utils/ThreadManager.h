/*=====================================================================
ThreadManager.h
---------------
Copyright Glare Technologies Limited 2019 -
File created by ClassTemplate on Sat Nov 03 08:25:38 2007
=====================================================================*/
#pragma once


#include "MessageableThread.h"
#include "ThreadMessage.h"
#include <set>


/*=====================================================================
ThreadManager
-------------
Manages one or more MessageableThreads.
Provides some basic services such as sending all managed threads a message,
or sending kill messages to the threads and waiting for them to finish.
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
	size_t getNumThreads();

	Mutex& getMutex() { return mutex; }

	typedef std::set<Reference<MessageableThread> > THREAD_SET_TYPE;
	THREAD_SET_TYPE& getThreads() { return threads; }

	// Run unit tests
	static void test();

private:
	THREAD_SET_TYPE threads			GUARDED_BY(mutex);

	Mutex mutex; // Protects 'threads' set.
};
