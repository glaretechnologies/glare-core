/*=====================================================================
ThreadManager.cpp
-----------------
File created by ClassTemplate on Sat Nov 03 08:25:38 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "ThreadManager.h"


#include "KillThreadMessage.h"
#include "../utils/platformutils.h"
#include "../utils/stringutils.h"
#include "../indigo/globals.h"


ThreadManager::ThreadManager()
{
}


ThreadManager::~ThreadManager()
{
	killThreadsBlocking();
}


void ThreadManager::enqueueMessage(const ThreadMessage& m)
{
	Lock lock(mutex);

	for(MESSAGE_QUEUE_MAP_TYPE::iterator i=message_queues.begin(); i!=message_queues.end(); ++i)
	{
		ThreadMessage* cloned_message = m.clone();
		(*i).second->enqueue(cloned_message);
	}
}


void ThreadManager::killThreadsBlocking()
{
	// Send kill messages to all threads
	killThreadsNonBlocking();

	while(1)
	{
		{
			Lock lock(mutex);

			if(message_queues.size() == 0)
				break;

			// Suspend this thread until one of the worker threads terminates and calls thread_terminated_condition.notify().
			/*const bool signalled = */thread_terminated_condition.wait(
				mutex,
				true, // infinite wait time
				0.0 // wait time (s) (n/a)
				);

			if(message_queues.size() == 0)
				break;
		}
	}
}


void ThreadManager::killThreadsNonBlocking()
{
	// Send kill messages to all threads
	const KillThreadMessage m;
	enqueueMessage(m);
}


void ThreadManager::threadFinished(MessageableThread* t)
{
	assert(t);

	Lock lock(mutex);

	// Delete any messages in the queue for this thread
	MESSAGE_QUEUE_TYPE* queue = (*message_queues.find(t)).second;
	if(queue)
	{
		// Delete any messages still in the queue
		{
			Lock queue_lock(queue->getMutex());

			while(!queue->unlockedEmpty())
			{
				ThreadMessageRef m;
				queue->unlockedDequeue(m);
			}
		} // End of queue lock scope.

		// Delete the queue
		delete queue;
	}

	// Remove queue from list of queues
	message_queues.erase(t);

	thread_terminated_condition.notify();
}


void ThreadManager::addThread(const Reference<MessageableThread>& t)
{
	Lock lock(mutex);

	// Create a message queue for the thread
	MESSAGE_QUEUE_TYPE* msg_queue = new MESSAGE_QUEUE_TYPE();

	// Add it to list of message queues
	message_queues[t] = msg_queue;

	// Tell the thread the message queue and 'this'.
	t->set(this, msg_queue);

	// Launch the thread
	t->launch(/*true*/);
}


unsigned int ThreadManager::getNumThreads()
{
	Lock lock(mutex);

	return (unsigned int)message_queues.size();
}



#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"
#include "../utils/stringutils.h"
#include "../utils/platformutils.h"


class SimpleMessageableThread : public MessageableThread
{
public:
	SimpleMessageableThread() { /*conPrint("SimpleMessageableThread()"); */}
	~SimpleMessageableThread() { /*conPrint("~SimpleMessageableThread()");*/ }
	virtual void doRun()
	{
	}
};


class SimpleMessageableThreadWithPause : public MessageableThread
{
public:
	SimpleMessageableThreadWithPause() { /*conPrint("SimpleMessageableThreadWithPause()");*/ }
	~SimpleMessageableThreadWithPause() { /*conPrint("~SimpleMessageableThreadWithPause()");*/ }
	virtual void doRun()
	{
		PlatformUtils::Sleep(50);
	}
};


class MessageHandlingTestThread : public MessageableThread
{
public:
	MessageHandlingTestThread() { /*conPrint("MessageHandlingTestThread() {()");*/ }
	~MessageHandlingTestThread() { /*conPrint("~MessageHandlingTestThread() {()");*/ }
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
	conPrint("ThreadManager::test()");

	{
		ThreadManager m;
		testAssert(m.getNumThreads() == 0);
	}

	{
		ThreadManager m;
		m.addThread(new SimpleMessageableThread());

		testAssert(m.getNumThreads() >= 0 && m.getNumThreads() <= 1); // The thread may have terminated and been removed from the manager by the time we get here.
	}

	{
		ThreadManager m;
		m.addThread(new SimpleMessageableThread());
		m.addThread(new SimpleMessageableThread());

		testAssert(m.getNumThreads() >= 0 && m.getNumThreads() <= 2);
	}

	{
		ThreadManager m;
		for(int i=0; i<1000; ++i)
			m.addThread(new SimpleMessageableThread());

		testAssert(m.getNumThreads() >= 0 && m.getNumThreads() <= 1000);
	}


	// Test with threads that pause a while, so we can check that killThreadsBlocking() works for live threads.
	{
		ThreadManager m;
		for(int i=0; i<100; ++i)
			m.addThread(new SimpleMessageableThreadWithPause());

		testAssert(m.getNumThreads() >= 0 && m.getNumThreads() <= 100);

		m.killThreadsBlocking();
	}


	// Test with threads that wait for a kill message before terminating.
	{
		ThreadManager m;
		for(int i=0; i<100; ++i)
			m.addThread(new MessageHandlingTestThread());

		// All the threads won't finish until a kill message is sent to them
		testAssert(m.getNumThreads() == 100);
	}

	conPrint("ThreadManager::test() done.");
}


#endif
