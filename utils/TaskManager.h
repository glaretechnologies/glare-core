/*=====================================================================
TaskManager.h
-------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "Task.h"
#include "Mutex.h"
#include "Reference.h"
#include "Condition.h"
#include "ArrayRef.h"
#include "MyThread.h"
#include "../maths/mathstypes.h"
#include <vector>
#include <limits>
#include <string>


namespace glare
{


class TaskRunnerThread;


/*=====================================================================
TaskManager
-----------
Manages and runs Tasks on multiple threads.

Tests in TaskTests.
=====================================================================*/
class TaskManager
{
public:
	TaskManager(size_t num_threads = std::numeric_limits<size_t>::max());
	TaskManager(const std::string& name, size_t num_threads = std::numeric_limits<size_t>::max()); // Name is used to name TaskRunnerThreads in the debugger.

	~TaskManager();

	const std::string& getName();

	// Only works on Windows.  Does nothing if called on a non-windows system.
	void setThreadPriorities(MyThread::Priority priority);

	// Add, and start executing a single task.
	void addTask(const TaskRef& t);

	// Add, and start executing multiple tasks.
	void addTasks(ArrayRef<TaskRef> tasks);
	
	// Blocks until all tasks in task group have finished being executed.
	void runTaskGroup(TaskGroupRef task_group);

	size_t getNumUnfinishedTasks() const;

	bool areAllTasksComplete() const;

	// Removes all tasks from the task queue.  Will not remove any tasks that have already been grabbed by a TaskRunnerThread.
	// Warning: be careful with this method, as some code may be waiting with waitForTasksToComplete(), and may expect the removed
	// tasks to have been completed.
	void removeQueuedTasks();

	// Blocks until all tasks have finished being executed.
	// NOTE: Prefer to use runTaskGroup, as waitForTasksToComplete waits for all tasks, from any (logical) task group.
	void waitForTasksToComplete();

	// Removes queued tasks, calls cancelTask() on all tasks being currently executed in TaskRunnerThreads, then blocks until all tasks have completed.
	void cancelAndWaitForTasksToComplete();

	size_t getNumThreads() const { return threads.size(); }
	size_t getConcurrency() const { return threads.size() + 1; } // Number of tasks to make in order to use all threads (including calling thread).

	bool areAllThreadsBusy();


	template <class Task, class TaskClosure> 
	void runParallelForTasks(const TaskClosure& closure, size_t begin, size_t end);

	/*
	Creates and runs some tasks in parallel.
	Stores references to the tasks in the tasks vector, and reuses them if the references are non-null.

	Task type must implement 
	set(const TaskClosure* closure, size_t begin, size_t end)
	*/
	//template <class Task, class TaskClosure>
	//void runParallelForTasks(const TaskClosure* closure, size_t begin, size_t end, std::vector<TaskRef>& tasks);

	/*
	In this method, each task has a particular offset and stride, where stride = num tasks.
	So each task's work indices are interleaved with each other.
	*/
	template <class Task, class TaskClosure> 
	void runParallelForTasksInterleaved(const TaskClosure& closure, size_t begin, size_t end);


	TaskRef dequeueTask(); // called by TaskRunnerThread
	void taskFinished(); // called by TaskRunnerThread
private:
	void init(size_t num_threads);
	void enqueueTaskInternal(Task* task);
	void enqueueTasksInternal(const TaskRef* tasks, size_t num_tasks);
	TaskRef dequeueTaskInternal();

	Condition num_unfinished_tasks_cond;
	mutable ::Mutex num_unfinished_tasks_mutex;
	int num_unfinished_tasks	GUARDED_BY(num_unfinished_tasks_mutex);
	
	std::string name;

	mutable Mutex queue_mutex;
	Task* task_queue_head;		GUARDED_BY(queue_mutex);
	Task* task_queue_tail;		GUARDED_BY(queue_mutex);
	Condition queue_nonempty;

	std::vector<Reference<TaskRunnerThread> > threads;
};


template <class TaskType, class TaskClosure> 
void TaskManager::runParallelForTasks(const TaskClosure& closure, size_t begin, size_t end)
{
	if(begin >= end)
		return;

	const size_t num_indices = end - begin;
	const size_t num_tasks = myMax<size_t>(1, myMin(num_indices, getNumThreads()));
	const size_t num_indices_per_task = Maths::roundedUpDivide(num_indices, num_tasks);

	glare::TaskGroupRef group = new glare::TaskGroup();
	group->tasks.reserve(num_tasks);

	for(size_t t=0; t<num_tasks; ++t)
	{
		const size_t task_begin = myMin(begin + t       * num_indices_per_task, end);
		const size_t task_end   = myMin(begin + (t + 1) * num_indices_per_task, end);
		assert(task_begin >= begin && task_end <= end);

		if(task_begin < task_end)
		{
			TaskType* task = new TaskType(closure, task_begin, task_end);
			group->tasks.push_back(task);
		}
	}

	runTaskGroup(group);
}


// TEMP disabled with taskgroup refactor:

//template <class TaskType, class TaskClosure>
//void TaskManager::runParallelForTasks(const TaskClosure* closure, size_t begin, size_t end, std::vector<TaskRef>& tasks)
//{
//	if(begin >= end)
//		return;
//
//	const size_t num_indices = end - begin;
//	const size_t num_tasks = myMax<size_t>(1, myMin(num_indices, getNumThreads()));
//	const size_t num_indices_per_task = Maths::roundedUpDivide(num_indices, num_tasks);
//
//	glare::TaskGroupRef group = new glare::TaskGroup();
//	group->tasks.resize(num_tasks);
//
//	//tasks.resize(num_tasks);
//
//	for(size_t t=0; t<num_tasks; ++t)
//	{
//		if(tasks[t].isNull())
//			tasks[t] = new TaskType();
//		const size_t task_begin = myMin(begin + t       * num_indices_per_task, end);
//		const size_t task_end   = myMin(begin + (t + 1) * num_indices_per_task, end);
//		assert(task_begin >= begin && task_end <= end);
//		tasks[t].downcastToPtr<TaskType>()->set(closure, task_begin, task_end);
//
//		//tasks[t]->processed = 0;
//	}
//
//	runTasks(tasks); // Blocks
//}


template <class TaskType, class TaskClosure> 
void TaskManager::runParallelForTasksInterleaved(const TaskClosure& closure, size_t begin, size_t end)
{
	if(begin >= end)
		return;

	const size_t num_indices = end - begin;
	const size_t num_tasks = myMax<size_t>(1, myMin(num_indices, getNumThreads()));
	
	glare::TaskGroupRef group = new glare::TaskGroup();
	group->tasks.reserve(num_tasks);

	for(size_t t=0; t<num_tasks; ++t)
	{
		const size_t task_begin = begin + t;
		const size_t stride = num_tasks;

		assert(task_begin >= begin);
		assert(task_begin < end);

		TaskType* task = new TaskType(closure, task_begin, end, stride);

		group->tasks.push_back(task);
	}

	// Wait for them to complete.  This blocks.
	runTaskGroup(group);
}


} // end namespace glare 
