/*=====================================================================
TaskTests.cpp
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2011-10-05 22:32:02 +0100
=====================================================================*/
#include "TaskTests.h"


#if BUILD_TESTS


#include "Task.h"
#include "TaskManager.h"
#include "../utils/platformutils.h"
#include "../indigo/globals.h"


namespace Indigo
{


class TestTask : public Task
{
public:
	TestTask(int x_) : x(x_) {}

	virtual void run(size_t thread_index)
	{
		//std::cout << "x: " << x << std::endl;
	}

	int x;
};


class LongRunningTestTask : public Task
{
public:
	LongRunningTestTask(int x_) : x(x_) {}

	virtual void run(size_t thread_index)
	{
		//std::cout << "x: " << x << std::endl;
		PlatformUtils::Sleep(50);
	}

	int x;
};


void TaskTests::test()
{
	conPrint("TaskTests");

	{
		TaskManager m;
	}
	{
		TaskManager m;
		m.waitForTasksToComplete();
	}


	{
		TaskManager m;
		m.addTask(new TestTask(0));
		m.waitForTasksToComplete();
	}

	// Run a quick task, but wait a bit.  This should catch any problems with the thread finishing early.
	{
		TaskManager m;
		m.addTask(new TestTask(0));

		PlatformUtils::Sleep(50);

		m.waitForTasksToComplete();
	}

	// Run a Long task, but wait quickly.  This should catch any problems with the thread finishing late.
	{
		TaskManager m;
		m.addTask(new LongRunningTestTask(0));

		m.waitForTasksToComplete();
	}



	{
		TaskManager m;

		for(int i=0; i<1000; ++i)
			m.addTask(new TestTask(i));

		m.waitForTasksToComplete();
	}

	{
		TaskManager m;

		for(int i=0; i<1000; ++i)
			m.addTask(new TestTask(i));

		m.waitForTasksToComplete();

		for(int i=0; i<1000; ++i)
			m.addTask(new TestTask(i));

		m.waitForTasksToComplete();
	}


	conPrint("TaskTests done.");
}


} // end namespace Indigo 


#endif

