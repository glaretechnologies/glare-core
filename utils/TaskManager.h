/*=====================================================================
TaskManager.h
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2011-10-05 21:56:22 +0100
=====================================================================*/
#pragma once


#include "Task.h"
#include "ThreadSafeQueue.h"
#include "Mutex.h"
#include "Reference.h"
#include "Condition.h"
#include "../maths/mathstypes.h"
#include <vector>
#include <limits>


namespace Indigo
{


class TaskRunnerThread;


/*=====================================================================
TaskManager
-------------------

=====================================================================*/
class TaskManager
{
public:
	/*enum
	{
		NumThreadsChoice_Auto,
		NumThreadsChoice_Explicit
	} NumThreadsChoice;*/

	TaskManager(size_t num_threads = std::numeric_limits<size_t>::max());

	~TaskManager();

	void addTask(const Reference<Task>& t);
	void addTasks(Reference<Task>* tasks, size_t num_tasks);

	bool areAllTasksComplete();

	void waitForTasksToComplete();

	size_t getNumThreads() const { return threads.size(); }

	bool areAllThreadsBusy();


	template <class Task, class TaskClosure> 
	void runParallelForTasks(const TaskClosure& closure, size_t begin, size_t end);

	/*
	In this method, each task has a particular offset and stride, where stride = num tasks.
	So each task's work indices are interleaved with each other.
	*/
	template <class Task, class TaskClosure> 
	void runParallelForTasksInterleaved(const TaskClosure& closure, size_t begin, size_t end);


	ThreadSafeQueue<Reference<Task> >& getTaskQueue() { return tasks; }
	const ThreadSafeQueue<Reference<Task> >& getTaskQueue() const { return tasks; }

	Reference<Task> dequeueTask(); // called by TestRunnerThread
	void taskFinished(); // called by TestRunnerThread
	// void threadDead(); // called by TestRunnerThread
private:
	Condition num_unfinished_tasks_cond;
	::Mutex num_unfinished_tasks_mutex;
	int num_unfinished_tasks;

	//Condition num_threads_cond;
	//Mutex num_threads_mutex;
	//int num_threads;

	ThreadSafeQueue<Reference<Task> > tasks;

	std::vector<Reference<TaskRunnerThread> > threads;
};


template <class TaskType, class TaskClosure> 
void TaskManager::runParallelForTasks(const TaskClosure& closure, size_t begin, size_t end)
{
	if(begin >= end)
		return;

	const size_t total = end - begin;
	const size_t num_tasks = myMin(total, getNumThreads());

	size_t num_indices_per_task = 0;
	if(total % num_tasks == 0)
	{
		// If num_tasks divides total perfectly
		num_indices_per_task = total / num_tasks;
	}
	else
	{
		num_indices_per_task = (total / num_tasks) + 1;
	}

	size_t i = begin;
	for(size_t t=0; t<num_tasks; ++t)
	{
		const size_t task_begin = i;
		const size_t task_end = myMin(i + num_indices_per_task, end);

		assert(task_begin >= begin);
		assert(task_end <= end);

		if(task_begin < task_end)
		{
			TaskType* task = new TaskType(closure, task_begin, task_end);

			addTask(task);
		}

		i += num_indices_per_task;
	}
	assert(i >= end);

	// The tasks should be running.  Wait for them to complete.  This blocks.
	waitForTasksToComplete();
}


template <class TaskType, class TaskClosure> 
void TaskManager::runParallelForTasksInterleaved(const TaskClosure& closure, size_t begin, size_t end)
{
	if(begin >= end)
		return;

	const size_t total = end - begin;
	const size_t num_tasks = myMin(total, getNumThreads());

	for(size_t t=0; t<num_tasks; ++t)
	{
		const size_t task_begin = begin + t;
		const size_t stride = num_tasks;

		assert(task_begin >= begin);
		assert(task_begin < end);

		TaskType* task = new TaskType(closure, task_begin, end, stride);

		addTask(task);
	}

	// The tasks should be running.  Wait for them to complete.  This blocks.
	waitForTasksToComplete();
}


} // end namespace Indigo 


