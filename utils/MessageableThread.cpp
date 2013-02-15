/*=====================================================================
MessageableThread.cpp
---------------------
File created by ClassTemplate on Sat Nov 03 09:15:49 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "MessageableThread.h"


#include "ThreadManager.h"
#include "../utils/KillThreadMessage.h"
#include "../utils/timer.h"
#include "../maths/mathstypes.h"


MessageableThread::MessageableThread()
:	mesthread_thread_manager(NULL)
{
}


MessageableThread::~MessageableThread()
{
}


void MessageableThread::set(ThreadManager* thread_manager)
{
	assert(!mesthread_thread_manager);
	assert(thread_manager);
	mesthread_thread_manager = thread_manager;
}


void MessageableThread::run()
{
	this->doRun();
	assert(mesthread_thread_manager);
	if(mesthread_thread_manager)
		mesthread_thread_manager->threadFinished(this);
}


/*
	Suspend thread for wait_period_s, while waiting on the message queue.
	Will Consume all messages on the thread message queue, and break the wait if a kill message is received,
	or if the wait period is finished.
	Has a small minimum wait time (e.g. 0.1 s)
	If the thread receives a kill message, keep_running_in_out will be set to false.
*/
void MessageableThread::waitForPeriod(double wait_period, bool& keep_running_in_out)
{
	// Wait for wait_period seconds, but we also want to be responsive to kill messages while waiting.
	Timer wait_timer;

	while(keep_running_in_out && wait_timer.elapsed() < wait_period)
	{
		const double wait_time = myMax(0.1, wait_period - wait_timer.elapsed());

		// Block until timeout or thread message is ready to dequeue
		ThreadMessageRef message;
		const bool got_message = getMessageQueue().dequeueWithTimeout(
			wait_time,
			message
			);
		if(got_message)
		{
			if(dynamic_cast<KillThreadMessage*>(message.getPointer()))
				keep_running_in_out = false;
		}
	}
}
