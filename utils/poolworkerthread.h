/*=====================================================================
poolworkerthread.h
------------------
File created by ClassTemplate on Mon Sep 29 02:45:15 2003
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __POOLWORKERTHREAD_H_666_
#define __POOLWORKERTHREAD_H_666_



#include "threadpool.h"
#include "mythread.h"
#include <assert.h>
#include "../cyberspace/globals.h"//for threadsShutdown()

/*=====================================================================
PoolWorkerThread
----------------

=====================================================================*/
template <class Task>
class PoolWorkerThread : public MyThread
{
public:
	/*=====================================================================
	PoolWorkerThread
	----------------
	
	=====================================================================*/
	PoolWorkerThread(ThreadPool<Task>* pool, float poll_period);

	virtual ~PoolWorkerThread();


	virtual void doHandleTask(Task& task) = 0;


	//bool commit_suicide;//will be set by threadpool on its destruction
private:
	virtual void run();

	ThreadPool<Task>* pool; 
	float poll_period;
	Task task;
};


template <class Task>
PoolWorkerThread<Task>::PoolWorkerThread(ThreadPool<Task>* pool_, float poll_period_)
:	pool(pool_), 
	poll_period(poll_period_)
{
	assert(pool);
	assert(poll_period >= 0);
	commit_suicide = false;
}


template <class Task>
PoolWorkerThread<Task>::~PoolWorkerThread()
{
	
}



template <class Task>
void PoolWorkerThread<Task>::run()
{
	while(1)
	{
		if(::threadsShutDown())//if(commit_suicide)
			return;

		const bool got_task = pool->pollDequeueTask(task);

		if(::threadsShutDown())//if(commit_suicide)
			return;

		if(got_task)
		{
			doHandleTask(task);
		}
		else
		{
			Sleep(poll_period * 1000);
		}
	}
}


#endif //__POOLWORKERTHREAD_H_666_




