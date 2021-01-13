/*=====================================================================
TaskManager.cpp
---------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "TaskManager.h"


#include "TaskRunnerThread.h"
#include "PlatformUtils.h"
#include "StringUtils.h"
#include "ConPrint.h"


namespace glare
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


void TaskManager::setThreadPriorities(MyThread::Priority priority)
{
#if defined(_WIN32)
	for(size_t i=0; i<threads.size(); ++i)
		threads[i]->setPriority(priority);
#endif
}


void TaskManager::addTask(const TaskRef& t)
{
	{
		Lock lock(num_unfinished_tasks_mutex);
		num_unfinished_tasks++;
	}

	tasks.enqueue(t);
}


void TaskManager::addTasks(ArrayRef<TaskRef> new_tasks)
{
	{
		Lock lock(num_unfinished_tasks_mutex);
		num_unfinished_tasks += (int)new_tasks.size();
	}

	tasks.enqueueItems(new_tasks.data(), new_tasks.size());
}


void TaskManager::runTasks(ArrayRef<TaskRef> new_tasks) // Add tasks, then wait for tasks to complete.
{
	addTasks(new_tasks);
	waitForTasksToComplete();
}


size_t TaskManager::getNumUnfinishedTasks() const
{
	Lock lock(num_unfinished_tasks_mutex);
	return num_unfinished_tasks;
}


bool TaskManager::areAllTasksComplete() const
{
	Lock lock(num_unfinished_tasks_mutex);
	return num_unfinished_tasks == 0;
}


void TaskManager::removeQueuedTasks()
{
	Lock lock(num_unfinished_tasks_mutex);

	const size_t num_queued_tasks = tasks.size();

	tasks.clear();

	num_unfinished_tasks -= (int)num_queued_tasks;

	assert(num_unfinished_tasks >= 0);
}


void TaskManager::cancelAndWaitForTasksToComplete()
{
	removeQueuedTasks();

	for(size_t i=0; i<threads.size(); ++i)
		threads[i]->cancelTasks();

	waitForTasksToComplete();
}


void TaskManager::waitForTasksToComplete()
{
	Lock lock(num_unfinished_tasks_mutex);

	if(threads.empty()) // If there are zero worker threads:
	{
		// Do the work in this thread!
		while(!tasks.empty())
		{
			TaskRef task;
			tasks.dequeue(task);
			task->run(0);
			num_unfinished_tasks--;
		}
	}
	else
	{
		while(num_unfinished_tasks != 0)
			num_unfinished_tasks_cond.wait(num_unfinished_tasks_mutex);
	}
}


bool TaskManager::areAllThreadsBusy()
{
	Lock lock(num_unfinished_tasks_mutex);

	return num_unfinished_tasks >= (int)threads.size();
}


TaskRef TaskManager::dequeueTask() // called by TaskRunnerThread
{
	TaskRef task;
	tasks.dequeue(task);
	return task;
}


void TaskManager::taskFinished() // called by TaskRunnerThread
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


const std::string& TaskManager::getName() // called by TaskRunnerThread
{
	return name;
}


} // end namespace glare 
