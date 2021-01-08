/*=====================================================================
ThreadManager.cpp
-----------------
Copyright Glare Technologies Limited 2019 -
File created by ClassTemplate on Sat Nov 03 08:25:38 2007
=====================================================================*/
#include "ThreadManager.h"


#include "KillThreadMessage.h"
#include "PlatformUtils.h"
#include "StringUtils.h"
#include "ConPrint.h"


ThreadManager::ThreadManager()
{
}


ThreadManager::~ThreadManager()
{
	killThreadsBlocking();
}


void ThreadManager::enqueueMessage(const Reference<ThreadMessage>& m)
{
	Lock lock(mutex);

	for(THREAD_SET_TYPE::iterator i=threads.begin(); i!=threads.end(); ++i)
		(*i)->getMessageQueue().enqueue(m);
}


void ThreadManager::killThreadsBlocking()
{
	// Send kill messages to all threads
	killThreadsNonBlocking();

	// Get list of threads to wait on.
	std::vector<Reference<MessageableThread> > threads_to_wait_on;

	// Build the list of threads.  Only hold the 'threads' mutex while building this list.
	// We don't want to hold the mutex while joining threads, as threads will be getting removed from the threads set during the joining process.
	{
		Lock lock(mutex);
		for(THREAD_SET_TYPE::iterator i=threads.begin(); i!=threads.end(); ++i)
			threads_to_wait_on.push_back(*i);
	}
	
	// Wait for each thread to terminate.  NOTE: Could use WaitForMulitpleObjects on Windows.
	for(size_t i=0; i<threads_to_wait_on.size(); ++i)
		threads_to_wait_on[i]->join();
}


void ThreadManager::killThreadsNonBlocking()
{
	// Send kill messages to all threads
	Lock lock(mutex);

	Reference<KillThreadMessage> kill_msg = new KillThreadMessage();
	for(THREAD_SET_TYPE::iterator i=threads.begin(); i!=threads.end(); ++i)
	{
		(*i)->getMessageQueue().enqueue(kill_msg);
		(*i)->kill();
	}
}


void ThreadManager::threadFinished(MessageableThread* t)
{
	assert(t);

	// Remove the thread from our set of threads.
	Lock lock(mutex);

	threads.erase(Reference<MessageableThread>(t));
}


void ThreadManager::addThread(const Reference<MessageableThread>& t)
{
	Lock lock(mutex);

	// Set the pointer back to this.
	t->setThreadManager(this);

	// Add to set of threads.
	threads.insert(t);

	try
	{
		// Launch the thread
		t->launch();
	}
	catch(MyThreadExcep& e)
	{
		// Thread could not be created for some reason.

		// Remove from list of threads
		threads.erase(t);

		// Re-throw exception
		throw e;
	}	
}


size_t ThreadManager::getNumThreads()
{
	Lock lock(mutex);

	return threads.size();
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/PlatformUtils.h"


class SimpleMessageableThread : public MessageableThread
{
public:
	SimpleMessageableThread() {}
	~SimpleMessageableThread() {}
	virtual void doRun() {}
};


class SimpleMessageableThreadWithNumRuns : public MessageableThread
{
public:
	SimpleMessageableThreadWithNumRuns(int* num_runs_, Mutex* num_runs_mutex_) : num_runs(num_runs_), num_runs_mutex(num_runs_mutex_) {}
	~SimpleMessageableThreadWithNumRuns() {}
	virtual void doRun()
	{
		Lock lock(*num_runs_mutex);
		(*num_runs)++;
	}

	int* num_runs;
	Mutex* num_runs_mutex;
};


class SimpleMessageableThreadWithPause : public MessageableThread
{
public:
	SimpleMessageableThreadWithPause() {}
	~SimpleMessageableThreadWithPause() {}
	virtual void doRun()
	{
		PlatformUtils::Sleep(50);
	}
};


class MessageHandlingTestThread : public MessageableThread
{
public:
	MessageHandlingTestThread() {}
	~MessageHandlingTestThread() {}
	virtual void doRun()
	{
		bool keep_running = true;
		while(keep_running)
		{
			// Dequeue message from message queue
			ThreadMessageRef m;
			this->getMessageQueue().dequeue(m);

			// If it's a kill message, break
			if(dynamic_cast<KillThreadMessage*>(m.getPointer()))
				keep_running = false;

			PlatformUtils::Sleep(0);
		}
	}
};


void ThreadManager::test()
{
	
	conPrint("------------ThreadManager::test()---------------");
	conPrint("test 1:");

	{
		ThreadManager m;
		testAssert(m.getNumThreads() == 0);
	}

	conPrint("test 2:");

	{
		ThreadManager m;
		m.addThread(new SimpleMessageableThread());

		testAssert(m.getNumThreads() >= 0 && m.getNumThreads() <= 1); // The thread may have terminated and been removed from the manager by the time we get here.
	}

	conPrint("test 3:");

	{
		ThreadManager m;
		m.addThread(new SimpleMessageableThread());
		m.addThread(new SimpleMessageableThread());

		testAssert(m.getNumThreads() >= 0 && m.getNumThreads() <= 2);
	}

	conPrint("test 4:");

	try
	{
		int num_runs = 0;
		Mutex num_runs_mutex;

		// NOTE: If we try to create more theads, e.g. 1000, thread creation starts failing in our Linux VMs.
		ThreadManager m;
		for(int i=0; i<100; ++i)
			m.addThread(new SimpleMessageableThreadWithNumRuns(&num_runs, &num_runs_mutex));

		testAssert(m.getNumThreads() >= 0 && m.getNumThreads() <= 10000);

		PlatformUtils::Sleep(50);
		
		{
			Lock lock(num_runs_mutex);
			conPrint("num_runs: " + toString(num_runs));
		}
	}
	catch(MyThreadExcep& e)
	{
		failTest(e.what());
	}

	conPrint("test 5:");

	// Test with threads that pause a while, so we can check that killThreadsBlocking() works for live threads.
	{
		ThreadManager m;
		for(int i=0; i<100; ++i)
			m.addThread(new SimpleMessageableThreadWithPause());

		testAssert(m.getNumThreads() >= 0 && m.getNumThreads() <= 100);

		m.killThreadsBlocking();
	}

	conPrint("test 6:");

	// Test with threads that wait for a kill message before terminating.
	{
		ThreadManager m;
		for(int i=0; i<100; ++i)
			m.addThread(new MessageHandlingTestThread());

		// All the threads won't finish until a kill message is sent to them
		testAssert(m.getNumThreads() == 100);
	}

	conPrint("test 7:");

	// Test killThreadsNonBlocking
	{
		ThreadManager m;
		m.addThread(new SimpleMessageableThreadWithPause());
		m.killThreadsNonBlocking();
		m.addThread(new SimpleMessageableThreadWithPause());
		m.killThreadsNonBlocking();
	}

	conPrint("test 8:");

	// Test killThreadsBlocking
	{
		ThreadManager m;
		m.addThread(new SimpleMessageableThreadWithPause());
		m.killThreadsBlocking();
		testAssert(m.getNumThreads() == 0);
		m.addThread(new SimpleMessageableThreadWithPause());
		m.killThreadsBlocking();
		testAssert(m.getNumThreads() == 0);
	}

	conPrint("ThreadManager::test() done.");
}


#endif
