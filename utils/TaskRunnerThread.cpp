/*=====================================================================
TaskRunnerThread.cpp
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2011-10-05 22:17:09 +0100
=====================================================================*/
#include "TaskRunnerThread.h"


#include "TaskManager.h"
#include "Task.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"


namespace Indigo
{


TaskRunnerThread::TaskRunnerThread(TaskManager* manager_, size_t thread_index_)
:	manager(manager_),
	thread_index(thread_index_)
{

}


TaskRunnerThread::~TaskRunnerThread()
{

}


void TaskRunnerThread::run()
{
	while(1)
	{
		Reference<Task> task = manager->dequeueTask();
		if(task.isNull())
			break;

		// Execute the task
		task->run(thread_index);

		// Inform the task manager that we have executed the task.
		manager->taskFinished();
	}

	//manager->threadDead();
}


} // end namespace Indigo 


