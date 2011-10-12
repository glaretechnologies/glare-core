/*=====================================================================
TaskRunnerThread.h
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2011-10-05 22:17:09 +0100
=====================================================================*/
#pragma once


#include "MessageableThread.h"


namespace Indigo
{


class TaskManager;


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
};


} // end namespace Indigo 


