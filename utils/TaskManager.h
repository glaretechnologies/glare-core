/*=====================================================================
TaskManager.h
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2011-10-05 21:56:22 +0100
=====================================================================*/
#pragma once


#include "threadsafequeue.h"
#include "mutex.h"
#include "Condition.h"
#include <vector>
#include <limits>


namespace Indigo
{


class Task;
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

	void addTask(Task* t);

	bool areAllTasksComplete();

	void waitForTasksToComplete();

	size_t getNumThreads() const { return threads.size(); }


	ThreadSafeQueue<Task*>& getTaskQueue() { return tasks; }
	const ThreadSafeQueue<Task*>& getTaskQueue() const { return tasks; }

	Task* dequeueTask(); // called by TestRunnerThread
	void taskFinished(); // called by TestRunnerThread
	// void threadDead(); // called by TestRunnerThread
private:
	Condition num_unfinished_tasks_cond;
	::Mutex num_unfinished_tasks_mutex;
	int num_unfinished_tasks;

	//Condition num_threads_cond;
	//Mutex num_threads_mutex;
	//int num_threads;

	ThreadSafeQueue<Task*> tasks;

	std::vector<TaskRunnerThread*> threads;
};


} // end namespace Indigo 


