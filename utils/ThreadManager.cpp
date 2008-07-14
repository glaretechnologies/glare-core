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
	//conPrint("ThreadManager::~ThreadManager()");
	
	killThreadsBlocking();

	/*Lock lock(mutex);
	for(MESSAGE_QUEUE_MAP_TYPE::iterator i=dead_message_queues.begin(); i!=dead_message_queues.end(); ++i)
	{
		MESSAGE_QUEUE_TYPE* queue = (*i).second;
		while(!queue->empty())
		{
			ThreadMessage* m;
			queue->dequeue(m);
			delete m;
		}
		delete queue;
	}*/
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
	while(1)
	{
		//conPrint("Waiting..." + toString((int)message_queues.size()));
		{
		Lock lock(mutex);
		if(message_queues.size() == 0)
			break;
		}
		PlatformUtils::Sleep(500);
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

	// Delete any messages in the queue
	MESSAGE_QUEUE_TYPE* queue = (*message_queues.find(t)).second;
	if(queue)
	{
		while(!queue->empty())
		{
			ThreadMessage* m;
			queue->dequeue(m);
			delete m;
		}
		// Delete the queue
		delete queue;
	}
	// Remove queue from list of queues
	message_queues.erase(t);


	/*MESSAGE_QUEUE_MAP_TYPE::iterator r = message_queues.find(t);
	
	if(r != message_queues.end())
	{
		dead_message_queues.insert(*r);
		message_queues.erase(r);
	}*/
}


void ThreadManager::addThread(MessageableThread* t)
{
	Lock lock(mutex);

	// Create a message queue for the thread
	MESSAGE_QUEUE_TYPE* msg_queue = new MESSAGE_QUEUE_TYPE();

	// Add it to list of message queues
	message_queues[t] = msg_queue;

	// Tell the thread the message queue and this.
	t->set(this, msg_queue);

	// Launch the thread
	t->launch(true);

}



unsigned int ThreadManager::getNumThreadsRunning()
{
	Lock lock(mutex);

	return message_queues.size();
}