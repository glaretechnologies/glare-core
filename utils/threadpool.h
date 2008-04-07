/*=====================================================================
threadpool.h
------------
File created by ClassTemplate on Mon Sep 29 02:36:24 2003
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __THREADPOOL_H_666_
#define __THREADPOOL_H_666_


#include "threadsafequeue.h"
//#include "tsset.h"
#include "mythread.h"
#include <vector>

/*=====================================================================
ThreadPool
----------

=====================================================================*/
//template <class ThreadType, class Task>
template <class Task>
class ThreadPool// : public TSObjectManager<MyThread>
{
public:
	/*=====================================================================
	ThreadPool
	----------
	
	=====================================================================*/
	ThreadPool();//int numthreads, float poll_period);

	~ThreadPool();


	//client API
	void enqueueTask(Task& task);

	int getNumQueuedTasks() const; //threadsafe

	//worker thread API
	bool pollDequeueTask(Task& task_out);
	void insertFinishedTasks(Task& finished_task);
	
	//void committingSuicide(MyThread* thread);

	void addThread(MyThread* thread);

	ThreadSafeQueue<Task> taskqueue;

	void waitForThreadTermination();


private:

	//TSSet<MyThread*> threads;
	Mutex mutex;
	std::vector<MyThread*> threads;

	
};



template <class Task>
ThreadPool<Task>::ThreadPool()//int numthreads, float poll_period)
{
	/*for(int i=0; i<numthreads; ++i)
	{
		ThreadType* thread = new ThreadType(this, poll_period);
		thread->launch(true);
	}*/
}


template <class Task>
ThreadPool<Task>::~ThreadPool()
{
	//-----------------------------------------------------------------
	//tell all the threads in this pool to commit suicide
	//-----------------------------------------------------------------
	/*Lock ob_lock(this->getMutex());//get a lock on the object set

	for(OBSET_ITER_TYPE i = this->getObjects().begin(); i != this->getObjects().end(); ++i)
	{
		(*i)->commit_suicide = true;
	}*/

	//-----------------------------------------------------------------
	//clear set of threads, but don't delete threads as they will 
	//hopefully have already autodeleted themselves.
	//-----------------------------------------------------------------
	//this->clearObjects();
}


//client API
template <class Task>
void ThreadPool<Task>::enqueueTask(Task& task)
{
	taskqueue.enqueue(task);
}

template <class Task>
int ThreadPool<Task>::getNumQueuedTasks() const //threadsafe
{
	return taskqueue.size();
}


//worker thread API
template <class Task>
bool ThreadPool<Task>::pollDequeueTask(Task& task_out)
{
	return taskqueue.pollDequeueLocked(task_out);
}

template <class Task>
void ThreadPool<Task>::insertFinishedTasks(Task& finished_task)
{
	for(unsigned int i=0; i<threads.size(); ++i)
		taskqueue.enqueue(finished_task);
}

template <class Task>
void ThreadPool<Task>::addThread(MyThread* thread)
{
	threads.push_back(thread);
}

template <class Task>
void ThreadPool<Task>::waitForThreadTermination()
{
	// TEMP: inefficient way
	for(unsigned int i=0; i<threads.size(); ++i)
	{
		threads[i]->waitForThread();
	}
}


template <class TheadType, class Task>
void spawnThreads(ThreadPool<Task>& threadpool, int numthreadstospawn)
{
	for(int i=0; i<numthreadstospawn; ++i)
	{
		TheadType* thread = new TheadType(&threadpool);

		//threadpool.insertObject(thread);
		threadpool.addThread(thread);

		thread->launch(true);
	}
}








#endif //__THREADPOOL_H_666_




