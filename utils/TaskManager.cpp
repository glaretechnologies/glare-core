/*=====================================================================
TaskManager.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-10-05 21:56:22 +0100
=====================================================================*/
#include "TaskManager.h"


#include "TaskRunnerThread.h"
#include "platformutils.h"
#include "stringutils.h"
#include "../indigo/globals.h"


namespace Indigo
{


TaskManager::TaskManager()
:	num_unfinished_tasks(0)
	///num_threads(0)
{
	threads.resize(PlatformUtils::getNumLogicalProcessors());

	//num_threads = threads.size();

	for(size_t i=0; i<threads.size(); ++i)
	{
		threads[i] = new TaskRunnerThread(this, i);
		threads[i]->launch(true);
	}
}


TaskManager::~TaskManager()
{
	// Send null tasks to all threads, to tell them to quit.

	for(size_t i=0; i<threads.size(); ++i)
		tasks.enqueue(NULL);

	// Wait for threads to quit.
	for(size_t i=0; i<threads.size(); ++i)
		threads[i]->join();

	/*while(1)
	{
		Lock lock(num_threads_mutex);
		if(num_threads == 0)
			break;

		num_threads_cond.wait(num_threads_mutex, true, 0);

		if(num_threads == 0)
			break;
	}*/
}


void TaskManager::addTask(Task* t)
{
	{
		Lock lock(num_unfinished_tasks_mutex);
		num_unfinished_tasks++;
	}

	tasks.enqueue(t);
}


bool TaskManager::areAllTasksComplete()
{
	Lock lock(num_unfinished_tasks_mutex);
	return num_unfinished_tasks == 0;
}


void TaskManager::waitForTasksToComplete()
{
	while(1)
	{
		Lock lock(num_unfinished_tasks_mutex);

		if(num_unfinished_tasks == 0)
			return;
		
		num_unfinished_tasks_cond.wait(
			num_unfinished_tasks_mutex, 
			true, // infinite wait time
			0 // wait time (s) (not used)
		);

		if(num_unfinished_tasks == 0)
			return;
	}
}


Task* TaskManager::dequeueTask() // called by Tasks
{
	Task* task = NULL;
	tasks.dequeue(task);
	return task;
}


void TaskManager::taskFinished() // called by Tasks
{
	//conPrint("taskFinished()");
	//conPrint("num_unfinished_tasks: " + toString(num_unfinished_tasks));

	{
		Lock lock(num_unfinished_tasks_mutex);
		num_unfinished_tasks--;
	}

	num_unfinished_tasks_cond.notify();
}


/*void TaskManager::threadDead() // called by Tasks
{
	conPrint("threadDead()");
	

	{
		Lock lock(num_threads_mutex);
		conPrint("num_threads: " + toString(num_threads));
		assert(num_threads >= 1);
		num_threads--;
	}

	num_threads_cond.notify();
}*/


} // end namespace Indigo 


