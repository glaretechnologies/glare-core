/*=====================================================================
TaskTests.cpp
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2011-10-05 22:32:02 +0100
=====================================================================*/
#include "TaskTests.h"


#include "Task.h"
#include "TaskManager.h"
#include <iostream>


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


void TaskTests::test()
{
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
}


} // end namespace Indigo 


