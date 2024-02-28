/*=====================================================================
Task.h
------
Copyright Glare Technologies Limited 2021 -
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

	virtual void cancelTask() {} 

	TaskAllocator* allocator; // If non-null, Reference calls destroyAndFreeOb which calls allocator->destroyAndFree to destroy this object.
	
	//TaskGroup* task_group;
	//glare::AtomicInt processed;
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
