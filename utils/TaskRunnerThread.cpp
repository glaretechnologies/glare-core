/*=====================================================================
TaskRunnerThread.cpp
--------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "TaskRunnerThread.h"


#include "TaskManager.h"
#include "Task.h"
#include "ConPrint.h"
#include "StringUtils.h"
#include "Timer.h"
#include "Clock.h"
#include "PlatformUtils.h"
#include "Lock.h"


//#define TASK_STATS 1
#if TASK_STATS
#define TASK_STATS_DO(expr) (expr)
#else
#define TASK_STATS_DO(expr)
#endif


namespace glare
{


TaskRunnerThread::TaskRunnerThread(TaskManager* manager_, size_t thread_index_)
:	manager(manager_),
	thread_index(thread_index_)
//	task_times(NULL)
{
#if TASK_STATS
	task_times = new std::vector<TaskTimes>();
#endif
}


TaskRunnerThread::~TaskRunnerThread()
{
#if TASK_STATS
	for(size_t i=0; i<task_times->size(); ++i)
	{
		for(size_t z=0; z<i; ++z)
			conPrintStr("\t");

		conPrint("thread " + toString(thread_index) + ": task " + toString(i) + " start=" + doubleToStringNSigFigs((*task_times)[i].dequeue_time, 12) + " s, exec took " +
			::doubleToStringNSigFigs((*task_times)[i].finish_time - (*task_times)[i].dequeue_time, 5) + " s");
	}

	delete task_times;
#endif
}


void TaskRunnerThread::run()
{
	PlatformUtils::setCurrentThreadName(manager->getName() + " thread " + toString(thread_index));

	while(1)
	{
		Reference<Task> task = manager->dequeueTask();
		
		if(task->is_quit_runner_task)
			break; // All tasks are finished, terminate thread by returning from run().

		{
			Lock lock(current_task_mutex);
			this->current_task = task;
		}

#if TASK_STATS
		task_times->push_back(TaskTimes());
		task_times->back().dequeue_time = Clock::getCurTimeRealSec();
#endif
		
		// Execute the task
		task->run(thread_index);

		TASK_STATS_DO(task_times->back().finish_time = Clock::getCurTimeRealSec());

		{
			Lock lock(current_task_mutex);
			this->current_task = nullptr;
		}

		// Inform the task manager that we have executed the task.
		manager->decrementUnfinishedTaskCountAndNotify();

		if(task->task_group)
		{
			int old_num_unfinished_group_tasks;
			TaskGroupRef task_group = task->task_group; // Hold a reference to the task group here until we execute task_group->num_unfinished_tasks_cond.notify(); below.
			// This makes sure the task group is not destroyed by some other thread after we decrement task_group->num_unfinished_group_tasks.
			{
				Lock lock(task_group->num_unfinished_group_tasks_mutex);
				old_num_unfinished_group_tasks = task_group->num_unfinished_group_tasks--;
			}

			if(old_num_unfinished_group_tasks == 1)
				task_group->num_unfinished_group_tasks_cond.notify();
		}
	}
}


void TaskRunnerThread::cancelTasks()
{
	TaskRef cur_task;
	{
		Lock lock(current_task_mutex);
		cur_task = this->current_task;
	}
	if(cur_task)
		cur_task->cancelTask();
}


} // end namespace glare 
