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
	MessageableThread();

	virtual ~MessageableThread();


	// Deriving classes should implement this method.
	virtual void doRun() = 0;


	void set(ThreadManager* thread_manager, ThreadSafeQueue<Reference<ThreadMessage> >* message_queue);

	ThreadSafeQueue<Reference<ThreadMessage> >& getMessageQueue() { return *mesthread_message_queue; }
protected:
	//bool deleteQueuedMessages(); // Returns true if a KillThreadMessage was in the queue.

	/*
		Suspend thread for wait_period_s, while waiting on the message queue.
		Will Consume all messages on the thread message queue, and break the wait if a kill message is received,
		or if the wait period is finished.
		Has a small minimum wait time (e.g. 0.1 s)
		If the thread receives a kill message, keep_running_in_out will be set to false.
	*/
	void waitForPeriod(double wait_period_s, bool& keep_running_in_out);

	ThreadManager& getThreadManager() { return *mesthread_thread_manager; }

private:
	ThreadSafeQueue<Reference<ThreadMessage> >* mesthread_message_queue;
	ThreadManager* mesthread_thread_manager;

	virtual void run();
};


#endif //__MESSAGEABLETHREAD_H_666_
