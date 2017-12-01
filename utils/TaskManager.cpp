/*=====================================================================
TaskManager.cpp
-------------------
Copyright Glare Technologies Limited 2016 -
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
:	num_unfinished_tasks(0),
	name("task manager")
{
	init(num_threads);
}


TaskManager::TaskManager(const std::string& name_, size_t num_threads)
:	num_unfinished_tasks(0),
	name(name_)
{
	init(num_threads);
}


void TaskManager::init(size_t num_threads)
{
	if(num_threads == std::numeric_limits<size_t>::max()) // If auto-choosing num threads
		threads.resize(PlatformUtils::getNumLogicalProcessors());
	else
		threads.resize(num_threads);

	for(size_t i=0; i<threads.size(); ++i)
	{
		threads[i] = new TaskRunnerThread(this, i);
		threads[i]->launch();
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


void TaskManager::runTasks(Reference<Task>* new_tasks, size_t num_tasks) // Add tasks, then wait for tasks to complete.
{
	addTasks(new_tasks, num_tasks);
	waitForTasksToComplete();
}


bool TaskManager::areAllTasksComplete()
{
	Lock lock(num_unfinished_tasks_mutex);
	return num_unfinished_tasks == 0;
}


void TaskManager::waitForTasksToComplete()
{
	Lock lock(num_unfinished_tasks_mutex);

	if(threads.empty()) // If there are zero worker threads:
	{
		// Do the work in this thread!
		while(!tasks.empty())
		{
			Reference<Task> task;
			tasks.dequeue(task);
			task->run(0);
			num_unfinished_tasks--;
		}
	}
	else
	{
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
		num_unfinished_tasks_cond.notifyAll(); // There could be multiple threads waiting on this condition, so use notifyAll().
}


const std::string& TaskManager::getName() // called by TestRunnerThread
{
	return name;
}


} // end namespace Indigo 
