/*=====================================================================
Task.h
------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "ThreadSafeRefCounted.h"
#include "Reference.h"
#include "Mutex.h"
#include "Condition.h"
#include <stdlib.h>
#include <vector>


namespace glare
{

class Task;
class TaskGroup;


class TaskAllocator
{
public:
	virtual ~TaskAllocator() = default;
	virtual void destroyAndFree(Task* task) = 0;
};


/*=====================================================================
Task
----
Tests are in TaskTests.
=====================================================================*/
class Task : public ::ThreadSafeRefCounted
{
public:
	Task();
	virtual ~Task();

	virtual void run(size_t thread_index) = 0;

	virtual void cancelTask() {} // Called to interrupt a task currently running.

	virtual void removedFromQueue() {} // Called when the task is removed from the task queue, before running.

	TaskAllocator* allocator; // If non-null, Reference calls destroyAndFreeOb which calls allocator->destroyAndFree to destroy this object.
	
	TaskGroup* task_group; // Task group that this task is part of, if non-null.

	// Task manager task queue linked list pointers:
	Task* prev;		// guarded by task manager queue mutex.
	Task* next;		// guarded by task manager queue mutex.
	bool in_queue;	// guarded by task manager queue mutex.

	bool is_quit_runner_task;
private:
};


typedef Reference<Task> TaskRef;


class TaskGroup : public ::ThreadSafeRefCounted
{
public:
	std::vector<TaskRef> tasks;

	Condition num_unfinished_tasks_cond;
	mutable ::Mutex num_unfinished_tasks_mutex;
	int num_unfinished_tasks	GUARDED_BY(num_unfinished_tasks_mutex);
};

typedef Reference<TaskGroup> TaskGroupRef;


} // end namespace glare 


// Template specialisation of destroyAndFreeOb for glare::Task, to call allocator->destroyAndFree if this task was allocated from a custom allocator.
template <>
inline void destroyAndFreeOb<glare::Task>(glare::Task* task)
{
	if(task->allocator)
		task->allocator->destroyAndFree(task);
	else
		delete task;
}
