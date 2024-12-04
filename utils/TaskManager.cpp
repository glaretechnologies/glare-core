/*=====================================================================
TaskManager.cpp
---------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "TaskManager.h"


#include "TaskRunnerThread.h"
#include "PlatformUtils.h"
#include "StringUtils.h"
#include "ConPrint.h"
#include "Lock.h"


namespace glare
{


TaskManager::TaskManager(size_t num_threads)
:	num_unfinished_tasks(0),
	name("task manager")
{
	init(num_threads);
}


TaskManager::TaskManager(const std::string& name_, size_t num_threads)
:	num_unfinished_tasks(0),
	name(name_)
{
	init(num_threads);
}


void TaskManager::init(size_t num_threads)
{
	{
		Lock lock(queue_mutex);
		task_queue_head = NULL;
		task_queue_tail = NULL;
	}

	if(num_threads == std::numeric_limits<size_t>::max()) // If auto-choosing num threads
		threads.resize(myMax<unsigned int>(1, PlatformUtils::getNumLogicalProcessors()) - 1); // Since we will process work on the calling thread of runTaskGroup, we only need getNumLogicalProcessors() - 1 worker threads.
	else
		threads.resize(num_threads);

	for(size_t i=0; i<threads.size(); ++i)
	{
		threads[i] = new TaskRunnerThread(this, i);
		threads[i]->launch();
	}
}


class QuitRunnerTask : public Task
{
public:
	QuitRunnerTask() { is_quit_runner_task = true; }
	virtual void run(size_t /*thread_index*/) {}
};


TaskManager::~TaskManager()
{
	// Send a QuitRunnerTask task to all threads, to tell them to quit.
	// NOTE: can't use the same QuitRunnerTask object for all threads as each object can only be inserted in queue once due to linked list stuff.
	for(size_t i=0; i<threads.size(); ++i)
		enqueueTaskInternal(new QuitRunnerTask());

	// Wait for threads to quit.
	for(size_t i=0; i<threads.size(); ++i)
		threads[i]->join();
}


void TaskManager::setThreadPriorities(MyThread::Priority priority)
{
#if defined(_WIN32)
	for(size_t i=0; i<threads.size(); ++i)
		threads[i]->setPriority(priority);
#endif
}


void TaskManager::enqueueTaskInternal(Task* task)
{
	task->incRefCount();

	{
		Lock lock(queue_mutex);
		// Insert at tail of queue linked list
		if(task_queue_tail)
		{
			task_queue_tail->next = task;
			task->prev = task_queue_tail;
			task->next = NULL;
			task_queue_tail = task;
		}
		else // else if queue was empty:
		{
			task->prev = NULL;
			task->next = NULL;
			task_queue_head = task_queue_tail = task;
		}
		task->in_queue = true;
	}

	queue_nonempty.notify();
}


void TaskManager::enqueueTasksInternal(const TaskRef* tasks, size_t num_tasks)
{
	{
		Lock lock(queue_mutex);

		for(size_t i=0; i<num_tasks; ++i)
		{
			Task* task = tasks[i].ptr();
			task->incRefCount();

			// Insert at tail of queue linked list
			if(task_queue_tail)
			{
				task_queue_tail->next = task;
				task->prev = task_queue_tail;
				task->next = NULL;
				task_queue_tail = task;
			}
			else // else if queue was empty:
			{
				task->prev = NULL;
				task->next = NULL;
				task_queue_head = task_queue_tail = task;
			}
			task->in_queue = true;
		}
	}

	 // Notify all suspended threads that there are items in the queue.  NOTE: the use of notifyAll here is potentially slow if the number of threads is >> num_tasks.
	queue_nonempty.notifyAll();
}


// Blocks until there is something to remove from queue
TaskRef TaskManager::dequeueTaskInternal()
{
	Lock lock(queue_mutex);

	while(1)
	{
		if(task_queue_head)
		{
			TaskRef removed_task = task_queue_head;
			assert(removed_task->getRefCount() >= 2); // There should be removed_task reference and queue reference.
			removed_task->decRefCount(); // Remove queue reference to task

			// Update queue head, tail and next task pointers.
			Task* next_task = task_queue_head->next;
			if(next_task)
			{
				task_queue_head = next_task;
				next_task->prev = NULL;
			}
			else // else we removed the only task in the queue:
			{
				task_queue_head = task_queue_tail = NULL;
			}

			removed_task->in_queue = false;
			return removed_task;
		}

		queue_nonempty.wait(queue_mutex);
	}
}


void TaskManager::addTask(const TaskRef& t)
{
	{
		Lock lock(num_unfinished_tasks_mutex);
		num_unfinished_tasks++;
	}

	enqueueTaskInternal(t.ptr());
}


void TaskManager::addTasks(ArrayRef<TaskRef> new_tasks)
{
	{
		Lock lock(num_unfinished_tasks_mutex);
		num_unfinished_tasks += (int)new_tasks.size();
	}

	enqueueTasksInternal(new_tasks.data(), new_tasks.size());
}


void TaskManager::runTaskGroup(TaskGroupRef task_group)
{
	if(task_group->tasks.empty())
		return;

	{
		Lock lock(task_group->num_unfinished_tasks_mutex);
		task_group->num_unfinished_tasks = (int)task_group->tasks.size();
	}

	for(size_t i=0; i<task_group->tasks.size(); ++i)
		task_group->tasks[i]->task_group = task_group.ptr();

	{
		Lock lock(num_unfinished_tasks_mutex);
		num_unfinished_tasks += (int)task_group->tasks.size();
	}

	// Add all but the first task to the queue.  We will start processing the first task directly below.
	assert(task_group->tasks.size() >= 1);
	enqueueTasksInternal(task_group->tasks.data() + 1, task_group->tasks.size() - 1);


	for(size_t i=0; i<task_group->tasks.size(); ++i)
	{
		Task* task = task_group->tasks[i].ptr();
		// Try and remove this task from the global task queue.  Note that the task may not be at the head of the queue.
		// It also may not be in the queue at all, if it has already been removed by a TaskRunnerThread.
		// We will detect this by checking task->in_queue.
		bool run_task;
		if(i == 0)
		{
			// Task 0 was never inserted into queue
			run_task = true;
		}
		else
		{
			Lock lock(queue_mutex);
			run_task = task->in_queue;
			if(task->in_queue)
			{
				// Remove from queue:
				if(task_queue_head == task)
					task_queue_head = task->next;

				if(task_queue_tail == task)
					task_queue_tail = task->prev;

				if(task->prev)
					task->prev->next = task->next;
				if(task->next)
					task->next->prev = task->prev;
				
				task->decRefCount(); // Remove queue reference
				assert(task->getRefCount() >= 1); // There should still be the reference from task_group->tasks.
				task->in_queue = false;
			}
		}

		if(run_task)
		{
			task_group->tasks[i]->run(/*thread index=*/threads.size());

			taskFinished();

			{
				Lock lock(task_group->num_unfinished_tasks_mutex);
				task_group->num_unfinished_tasks--;
			}
		}
	}

	// Block until all group tasks are done, i.e. task_group->num_unfinished_tasks == 0
	{
		Lock lock(task_group->num_unfinished_tasks_mutex);
		while(1)
		{	
			if(task_group->num_unfinished_tasks == 0)
				break;
			task_group->num_unfinished_tasks_cond.wait(task_group->num_unfinished_tasks_mutex);
		}
	}
}


size_t TaskManager::getNumUnfinishedTasks() const
{
	Lock lock(num_unfinished_tasks_mutex);
	return num_unfinished_tasks;
}


bool TaskManager::areAllTasksComplete() const
{
	Lock lock(num_unfinished_tasks_mutex);
	return num_unfinished_tasks == 0;
}


void TaskManager::removeQueuedTasks()
{
	Lock lock(queue_mutex);
	while(task_queue_head)
	{
		TaskRef task = task_queue_head; // Make a reference that can destroy the object if needed.
		task_queue_head->decRefCount(); // Remove queue reference
		task_queue_head = task_queue_head->next;

		{
			Lock lock2(num_unfinished_tasks_mutex);
			num_unfinished_tasks--;
			assert(num_unfinished_tasks >= 0);
		}
	}
	task_queue_tail = NULL;
}


void TaskManager::cancelAndWaitForTasksToComplete()
{
	removeQueuedTasks();

	for(size_t i=0; i<threads.size(); ++i)
		threads[i]->cancelTasks();

	waitForTasksToComplete();
}


void TaskManager::waitForTasksToComplete()
{
	if(threads.empty()) // If there are zero worker threads:
	{
		// Do the work in this thread!
		Lock lock(queue_mutex);

		while(task_queue_head)
		{
			TaskRef task = dequeueTaskInternal();
			task->run(0);

			{
				Lock lock2(num_unfinished_tasks_mutex);
				num_unfinished_tasks--;
				assert(num_unfinished_tasks >= 0);
			}
		}
	}
	else
	{
		Lock lock(num_unfinished_tasks_mutex);

		while(num_unfinished_tasks != 0)
			num_unfinished_tasks_cond.wait(num_unfinished_tasks_mutex);
	}
}


bool TaskManager::areAllThreadsBusy()
{
	Lock lock(num_unfinished_tasks_mutex);
	return num_unfinished_tasks >= (int)threads.size();
}


TaskRef TaskManager::dequeueTask() // called by TaskRunnerThread
{
	return dequeueTaskInternal();
}


void TaskManager::taskFinished() // called by TaskRunnerThread
{
	//conPrint("taskFinished()");
	//conPrint("num_unfinished_tasks: " + toString(num_unfinished_tasks));

	int new_num_unfinished_tasks;
	{
		Lock lock(num_unfinished_tasks_mutex);
		num_unfinished_tasks--;
		assert(num_unfinished_tasks >= 0);
		new_num_unfinished_tasks = num_unfinished_tasks;
	}
	
	if(new_num_unfinished_tasks == 0)
		num_unfinished_tasks_cond.notifyAll(); // There could be multiple threads waiting on this condition, so use notifyAll().
}


const std::string& TaskManager::getName() // called by TaskRunnerThread
{
	return name;
}


} // end namespace glare 
