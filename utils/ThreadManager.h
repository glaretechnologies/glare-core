/*=====================================================================
ThreadManager.h
---------------
File created by ClassTemplate on Sat Nov 03 08:25:38 2007
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __THREADMANAGER_H_666_
#define __THREADMANAGER_H_666_


#include "threadsafequeue.h"
#include "MessageableThread.h"
#include "ThreadMessage.h"
//#include "../utils/singleton.h"
#include <map>

/*=====================================================================
ThreadManager
-------------

=====================================================================*/
class ThreadManager// : public Singleton<ThreadManager>
{
public:
	/*=====================================================================
	ThreadManager
	-------------
	
	=====================================================================*/
	ThreadManager();

	~ThreadManager();




	void addThread(MessageableThread* t);
	
	void enqueueMessage(const ThreadMessage& m);

	void killThreadsBlocking();
	void killThreadsNonBlocking(); // Enqueues kill message to all threads, then returns immediately.
	
	
	// Called by threads when they are about to terminate
	void threadTerminating(MessageableThread* t);

	unsigned int getNumThreadsRunning();

private:
	typedef ThreadSafeQueue<ThreadMessage*> MESSAGE_QUEUE_TYPE;
	typedef std::map<MessageableThread*, MESSAGE_QUEUE_TYPE*> MESSAGE_QUEUE_MAP_TYPE;
	MESSAGE_QUEUE_MAP_TYPE message_queues;
	//MESSAGE_QUEUE_MAP_TYPE dead_message_queues;

	Mutex mutex;
};



#endif //__THREADMANAGER_H_666_




