/*=====================================================================
TaskManager.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at 2011-10-05 21:56:22 +0100
=====================================================================*/
#include "TaskManager.h"


#include "TaskRunnerThread.h"
#include "PlatformUtils.h"
#include "StringUtils.h"
#include "../utils/ConPrint.h"


namespace Indigo
{


TaskManager::TaskManager(size_t num_threads)
:	num_unfinished_tasks(0)
	///num_threads(0)
{
	if(num_threads == std::numeric_limits<size_t>::max()) // If auto-choosing num threads
		threads.resize(PlatformUtils::getNumLogicalProcessors());
	else
		threads.resize(num_threads);

	//num_threads = threads.size();

	for(size_t i=0; i<threads.size(); ++i)
	{
		threads[i] = new TaskRunnerThread(this, i);
		threads[i]->launch(/*false*/); // Don't autodelete, as we will be joining with the threads and then deleting them manually.
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

	// Delete the threads.
	//for(size_t i=0; i<threads.size(); ++i)
	//	delete threads[i];

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


void TaskManager::addTask(const Reference<Task>& t)
{
	{
		Lock lock(num_unfinished_tasks_mutex);
		num_unfinished_tasks++;
	}

	tasks.enqueue(t);
}


void TaskManager::addTasks(Reference<Task>* t, size_t num_tasks)
{
	{
		Lock lock(num_unfinished_tasks_mutex);
		num_unfinished_tasks += (int)num_tasks;
	}

	tasks.enqueueItems(t, num_tasks);
}


bool TaskManager::areAllTasksComplete()
{
	Lock lock(num_unfinished_tasks_mutex);
	return num_unfinished_tasks == 0;
}


void TaskManager::waitForTasksToComplete()
{
	Lock lock(num_unfinished_tasks_mutex);

	while(1)
	{
		if(num_unfinished_tasks == 0)
			return;
		
		num_unfinished_tasks_cond.wait(
			num_unfinished_tasks_mutex, 
			true, // infinite wait time
			0 // wait time (s) (not used)
		);
	}
}


bool TaskManager::areAllThreadsBusy()
{
	Lock lock(num_unfinished_tasks_mutex);

	return num_unfinished_tasks >= threads.size();
}


Reference<Task> TaskManager::dequeueTask() // called by Tasks
{
	Reference<Task> task;
	tasks.dequeue(task);
	return task;
}


void TaskManager::taskFinished() // called by Tasks
{
	//conPrint("taskFinished()");
	//conPrint("num_unfinished_tasks: " + toString(num_unfinished_tasks));

	int new_num_unfinished_tasks;
	{
		Lock lock(num_unfinished_tasks_mutex);
		num_unfinished_tasks--;
		new_num_unfinished_tasks = num_unfinished_tasks;
	}

	if(new_num_unfinished_tasks == 0)
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


