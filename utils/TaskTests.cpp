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
#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/StringUtils.h"
//#include "../utils/FixedSizeAllocator.h"
#include "../utils/Timer.h"


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


class ForLoopTaskClosure
{
public:
	ForLoopTaskClosure() {}

	std::vector<int>* touch_count;
};


class ForLoopTask : public Indigo::Task
{
public:
	ForLoopTask(const ForLoopTaskClosure& closure_, size_t begin_, size_t end_) : closure(closure_), begin(begin_), end(end_) {}

	virtual void run(size_t thread_index)
	{
		conPrint("ForLoopTask::run(), begin: " + toString(begin) + ", end: " + toString(end));

		for(size_t i = begin; i < end; ++i)
		{
			(*closure.touch_count)[i]++;
		}
	}

	const ForLoopTaskClosure& closure;
	size_t begin, end;
};


class ForLoopTaskInterleaved : public Indigo::Task
{
public:
	ForLoopTaskInterleaved(const ForLoopTaskClosure& closure_, size_t begin_, size_t end_, size_t stride_) : closure(closure_), begin(begin_), end(end_), stride(stride_) {}

	virtual void run(size_t thread_index)
	{
		conPrint("ForLoopTask::run(), begin: " + toString(begin) + ", end: " + toString(end) + ", stride: + " + toString(stride));

		for(size_t i = begin; i < end; i += stride)
		{
			(*closure.touch_count)[i]++;
		}
	}

	const ForLoopTaskClosure& closure;
	size_t begin, end, stride;
};


void testForLoopTaskRun(TaskManager& task_manager, size_t N)
{
	{
		testAssert(task_manager.areAllTasksComplete());

		std::vector<int> touch_count(N, 0);

		ForLoopTaskClosure closure;
		closure.touch_count = &touch_count;

		task_manager.runParallelForTasks<ForLoopTask, ForLoopTaskClosure>(closure, 0, (int)N);

		testAssert(task_manager.areAllTasksComplete());

		for(size_t i=0; i<N; ++i)
			testAssert(touch_count[i] == 1);
	}

	// Try runParallelForTasks() with begin != 0
	{
		testAssert(task_manager.areAllTasksComplete());

		// We will add a border of elements that should be zero afterwards.
		const int border = 10;
		std::vector<int> touch_count(2*border + N, 0);

		ForLoopTaskClosure closure;
		closure.touch_count = &touch_count;

		task_manager.runParallelForTasks<ForLoopTask, ForLoopTaskClosure>(closure, border, (int)N + border);

		testAssert(task_manager.areAllTasksComplete());

		for(size_t i=0; i<border; ++i)
			testAssert(touch_count[i] == 0);

		for(size_t i=border; i<N+border; ++i)
			testAssert(touch_count[i] == 1);

		for(size_t i=N+border; i<N+border*2; ++i)
			testAssert(touch_count[i] == 0);
	}

	// Try runParallelForTasksInterleaved() with begin != 0
	{
		testAssert(task_manager.areAllTasksComplete());

		// We will add a border of elements that should be zero afterwards.
		const int border = 10;
		std::vector<int> touch_count(2*border + N, 0);

		ForLoopTaskClosure closure;
		closure.touch_count = &touch_count;

		task_manager.runParallelForTasksInterleaved<ForLoopTaskInterleaved, ForLoopTaskClosure>(closure, border, (int)N + border);

		testAssert(task_manager.areAllTasksComplete());

		for(size_t i=0; i<border; ++i)
			testAssert(touch_count[i] == 0);

		for(size_t i=border; i<N+border; ++i)
			testAssert(touch_count[i] == 1);

		for(size_t i=N+border; i<N+border*2; ++i)
			testAssert(touch_count[i] == 0);
	}
}


void TaskTests::test()
{
	conPrint("TaskTests");

	{
		TaskManager m;
	}

	{
		TaskManager m(1);
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

	// Test mem allocator
	/*{
		::FixedSizeAllocator allocator(sizeof(TestTask), 16, 8);
		TaskManager m(1, &allocator);

		for(int i=0; i<1000; ++i)
			m.addTask(new (allocator.allocate(sizeof(TestTask), 16)) TestTask(i));

		m.waitForTasksToComplete();
	}*/


	// Test for loop stuff
	{
		TaskManager m(1);

		testForLoopTaskRun(m, 0);
		testForLoopTaskRun(m, 1);
		testForLoopTaskRun(m, 2);
		testForLoopTaskRun(m, 3);
		testForLoopTaskRun(m, 4);
		testForLoopTaskRun(m, 7);
		testForLoopTaskRun(m, 8);
		testForLoopTaskRun(m, 9);
		testForLoopTaskRun(m, 16);
		testForLoopTaskRun(m, 1000000);
	}

	{
		TaskManager m; // auto-pick num threads

		testForLoopTaskRun(m, 0);
		testForLoopTaskRun(m, 1);
		testForLoopTaskRun(m, 2);
		testForLoopTaskRun(m, 3);
		testForLoopTaskRun(m, 4);
		testForLoopTaskRun(m, 7);
		testForLoopTaskRun(m, 8);
		testForLoopTaskRun(m, 9);
		testForLoopTaskRun(m, 16);
		testForLoopTaskRun(m, 1000000);
	}



	// Perf test - fixed size allocator vs global allocator
	/*{

		const int N = 100000;
		{
			::FixedSizeAllocator allocator(sizeof(TestTask), 16, 8);
			TaskManager m(8, &allocator);
			Timer timer;
			for(int test=0; test<N; ++test)
			{
				for(int i=0; i<8; ++i)
					m.addTask(new (allocator.allocate(sizeof(TestTask), 16)) TestTask(i));

				m.waitForTasksToComplete();
			}

			conPrint("FixedSizeAllocator elapsed: " + timer.elapsedStringNPlaces(3));
		}

		{
			TaskManager m(8, NULL);
			Timer timer;
			for(int test=0; test<N; ++test)
			{
				for(int i=0; i<8; ++i)
					m.addTask(new TestTask(i));

				m.waitForTasksToComplete();
			}

			conPrint("global new/delete elapsed: " + timer.elapsedStringNPlaces(3));
		}
	}*/

	conPrint("TaskTests done.");
}


} // end namespace Indigo 


#endif

