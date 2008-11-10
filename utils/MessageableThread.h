/*=====================================================================
MessageableThread.h
-------------------
File created by ClassTemplate on Sat Nov 03 09:15:49 2007
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MESSAGEABLETHREAD_H_666_
#define __MESSAGEABLETHREAD_H_666_


#include "mythread.h"
#include "threadsafequeue.h"
#include "ThreadMessage.h"
class ThreadManager;

/*=====================================================================
MessageableThread
-----------------

=====================================================================*/
class MessageableThread : public MyThread
{
public:
	/*=====================================================================
	MessageableThread
	-----------------
	
	=====================================================================*/
	MessageableThread();

	virtual ~MessageableThread();


	void set(ThreadManager* thread_manager, ThreadSafeQueue<ThreadMessage*>* message_queue);

protected:
	bool deleteQueuedMessages(); // Returns true if a KillThreadMessage was in the queue.

	ThreadSafeQueue<ThreadMessage*>& getMessageQueue() { return *mesthread_message_queue; }
	ThreadManager& getThreadManager() { return *mesthread_thread_manager; }

private:
	ThreadSafeQueue<ThreadMessage*>* mesthread_message_queue;
	ThreadManager* mesthread_thread_manager;
};



#endif //__MESSAGEABLETHREAD_H_666_




