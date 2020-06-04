/*=====================================================================
TaskRunnerThread.h
-------------------
Copyright Glare Technologies Limited 2020 -
Generated at 2011-10-05 22:17:09 +0100
=====================================================================*/
#pragma once


#include "MyThread.h"


namespace Indigo
{


class TaskManager;
class Task;

struct TaskTimes
{
	double dequeue_time;
	double finish_time;
};


/*=====================================================================
TaskRunnerThread
-------------------

=====================================================================*/
class TaskRunnerThread : public MyThread
{
public:
	TaskRunnerThread(TaskManager* manager, size_t thread_index);
	virtual ~TaskRunnerThread();

	virtual void run();

	// Cancel the task the thread is currently executing, if any.
	virtual void cancelTasks();

private:
	TaskManager* manager;
	size_t thread_index;

	// std::vector<TaskTimes>* task_times;

	Reference<Task> current_task;
};


} // end namespace Indigo 
