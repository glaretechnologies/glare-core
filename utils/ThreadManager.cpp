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

	// Wait for threads to kill themselves
	/*while(1)
	{
		//conPrint("Waiting..." + toString((int)message_queues.size()));
		{
		Lock lock(mutex);
		if(message_queues.size() == 0)
			break;
		}
		PlatformUtils::Sleep(100);
	}*/

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


void ThreadManager::threadTerminating(MessageableThread* t)
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
				ThreadMessage* m;
				queue->unlockedDequeue(m);
				delete m;
			}
		} // End of queue lock scope.

		// Delete the queue
		delete queue;
	}

	// Remove queue from list of queues
	message_queues.erase(t);

	thread_terminated_condition.notify();
}


void ThreadManager::addThread(MessageableThread* t)
{
	Lock lock(mutex);

	// Create a message queue for the thread
	MESSAGE_QUEUE_TYPE* msg_queue = new MESSAGE_QUEUE_TYPE();

	// Add it to list of message queues
	message_queues[t] = msg_queue;

	// Tell the thread the message queue and 'this'.
	t->set(this, msg_queue);

	// Launch the thread
	t->launch(true);
}


/*unsigned int ThreadManager::getNumThreadsRunning()
{
	Lock lock(mutex);

	return message_queues.size();
}*/
