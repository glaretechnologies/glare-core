/*=====================================================================
TaskRunnerThread.h
-------------------
Copyright Glare Technologies Limited 2015 -
Generated at 2011-10-05 22:17:09 +0100
=====================================================================*/
#pragma once


#include "MessageableThread.h"


namespace Indigo
{


class TaskManager;


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
private:
	TaskManager* manager;
	size_t thread_index;

	std::vector<TaskTimes>* task_times;
};


} // end namespace Indigo 
