/*=====================================================================
TaskTests.cpp
-------------
Copyright Glare Technologies Limited 2020 -
Generated at 2011-10-05 22:32:02 +0100
=====================================================================*/
#include "TaskTests.h"


#if BUILD_TESTS


#include "Task.h"
#include "TaskManager.h"
#include "../utils/ConPrint.h"
#include "../utils/TestUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/StringUtils.h"
//#include "../utils/FixedSizeAllocator.h"
#include "../utils/Timer.h"


namespace Indigo
{


class TestTask : public Task
{
public:
	TestTask(IndigoAtomic& x_) : x(x_) {}

	virtual void run(size_t thread_index)
	{
		//std::cout << "x: " << x << std::endl;
		// conPrint("executing task x in thread: " + toString(thread_index));
		x++;
	}

	IndigoAtomic& x;
};


class LongRunningTestTask : public Task
{
public:
	LongRunningTestTask(IndigoAtomic& x_) : x(x_) {}

	virtual void run(size_t thread_index)
	{
		PlatformUtils::Sleep(50);
		x++;
	}

	IndigoAtomic& x;
};


class CancellableTestTask : public Task
{
public:
	CancellableTestTask(IndigoAtomic& x_) : x(x_) {}

	static int numSubTasks() { return 10; }

	virtual void run(size_t thread_index)
	{
		for(int i=0; i<numSubTasks(); ++i)
		{
			PlatformUtils::Sleep(1);
			if(should_cancel != 0)
				return;
			x++;
		}
	}

	virtual void cancelTask()
	{
		should_cancel = 1;
	}

	IndigoAtomic& x;

	IndigoAtomic should_cancel;
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


// Variant that has set() method.
class ForLoopTask2 : public Indigo::Task
{
public:
	void set(const ForLoopTaskClosure* closure_, size_t begin_, size_t end_)
	{
		closure = closure_;
		begin = begin_;
		end = end_;
	}

	virtual void run(size_t thread_index)
	{
		conPrint("ForLoopTask::run(), begin: " + toString(begin) + ", end: " + toString(end));

		for(size_t i = begin; i < end; ++i)
		{
			(*closure->touch_count)[i]++;
		}
	}

	const ForLoopTaskClosure* closure;
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

	// Test runParallelForTasks() that takes an array
	{
		testAssert(task_manager.areAllTasksComplete());

		std::vector<int> touch_count(N, 0);

		ForLoopTaskClosure closure;
		closure.touch_count = &touch_count;

		std::vector<Reference<Task> > tasks;

		task_manager.runParallelForTasks<ForLoopTask2, ForLoopTaskClosure>(&closure, 0, (int)N, tasks);
		testAssert(task_manager.areAllTasksComplete());

		for(size_t i=0; i<N; ++i)
			testAssert(touch_count[i] == 1);

		// Run again
		task_manager.runParallelForTasks<ForLoopTask2, ForLoopTaskClosure>(&closure, 0, (int)N, tasks);
		testAssert(task_manager.areAllTasksComplete());

		for(size_t i=0; i<N; ++i)
			testAssert(touch_count[i] == 2);
	}
}


void TaskTests::test()
{
	conPrint("TaskTests");

	{
		TaskManager m;
	}

	{
		TaskManager m(0);
	}

	{
		TaskManager m(1);
	}

	{
		TaskManager m;
		m.waitForTasksToComplete();
	}

	{
		TaskManager m(0);
		m.waitForTasksToComplete();
	}

	{
		TaskManager m;
		IndigoAtomic exec_counter = 0;
		m.addTask(new TestTask(exec_counter));
		m.waitForTasksToComplete();
		testAssert(exec_counter == 1);
	}

	// Run a quick task, but wait a bit.  This should catch any problems with the thread finishing early.
	{
		TaskManager m;
		IndigoAtomic exec_counter = 0;
		m.addTask(new TestTask(exec_counter));

		PlatformUtils::Sleep(50);

		m.waitForTasksToComplete();
		testAssert(exec_counter == 1);
	}

	// Run a Long task, but wait quickly.  This should catch any problems with the thread finishing late.
	{
		TaskManager m;
		IndigoAtomic exec_counter = 0;
		m.addTask(new LongRunningTestTask(exec_counter));

		m.waitForTasksToComplete();
		testAssert(exec_counter == 1);
	}

	// Test with multiple tasks
	{
		TaskManager m;
		IndigoAtomic exec_counter = 0;

		for(int i=0; i<1000; ++i)
			m.addTask(new TestTask(exec_counter));

		m.waitForTasksToComplete();
		testAssert(exec_counter == 1000);
	}

	{
		TaskManager m;
		IndigoAtomic exec_counter = 0;

		for(int i=0; i<1000; ++i)
			m.addTask(new TestTask(exec_counter));

		m.waitForTasksToComplete();
		testAssert(exec_counter == 1000);

		for(int i=0; i<1000; ++i)
			m.addTask(new TestTask(exec_counter));

		m.waitForTasksToComplete();
		testAssert(exec_counter == 2000);
	}

	// Test with zero worker threads (tasks should be executed in this thread)
	{
		TaskManager m(0);
		IndigoAtomic exec_counter = 0;

		for(int i=0; i<1000; ++i)
			m.addTask(new TestTask(exec_counter));

		m.waitForTasksToComplete();
		testAssert(exec_counter == 1000);

		for(int i=0; i<1000; ++i)
			m.addTask(new TestTask(exec_counter));

		m.waitForTasksToComplete();
		testAssert(exec_counter == 2000);
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
		TaskManager m(0); // zero threads

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

	// Test runTasks()
	{
		TaskManager m; // auto-pick num threads
		IndigoAtomic exec_counter = 0;

		std::vector<Reference<Indigo::Task> > tasks;
		for(int i=0; i<10; ++i)
			tasks.push_back(new TestTask(exec_counter));

		m.runTasks(tasks);
		testAssert(exec_counter == 10);
	}
	
	// Test addTasks()
	{
		TaskManager m; // auto-pick num threads
		IndigoAtomic exec_counter = 0;

		std::vector<Reference<Indigo::Task> > tasks;
		for(int i=0; i<10; ++i)
			tasks.push_back(new TestTask(exec_counter));

		m.addTasks(tasks);
		m.waitForTasksToComplete();
		testAssert(exec_counter == 10);
	}

	//-------------------- Test cancelAndWaitForTasksToComplete -----------------------------

	// Test with no enqueued tasks
	{
		TaskManager m; // auto-pick num threads
		m.cancelAndWaitForTasksToComplete();
	}

	// Test with a task that can't be interrupted.
	{
		TaskManager m; // auto-pick num threads
		IndigoAtomic exec_counter = 0;
		m.addTask(new LongRunningTestTask(exec_counter));
		PlatformUtils::Sleep(10);
		m.cancelAndWaitForTasksToComplete();
		testAssert(exec_counter == 0 || exec_counter == 1);
	}

	// Execute a single task, which does 1ms 'subtasks', then intterupt after 5ms.  We expected to see ~5 subtasks completed.
	{
		TaskManager m; // auto-pick num threads
		IndigoAtomic sub_exec_counter = 0;

		m.addTask(new CancellableTestTask(sub_exec_counter));

		PlatformUtils::Sleep(10);
		m.cancelAndWaitForTasksToComplete();
		conPrint("Cancelling task test: num completed sub-tasks: " + toString(sub_exec_counter) + " / " + toString(CancellableTestTask::numSubTasks()));
		testAssert(sub_exec_counter >= 0 && sub_exec_counter <= CancellableTestTask::numSubTasks());
	}

	// Try with lots of tasks
	{
		TaskManager m; // auto-pick num threads
		IndigoAtomic sub_exec_counter = 0;

		const int NUM_TASKS = 10000;
		for(int i=0; i<NUM_TASKS; ++i)
			m.addTask(new CancellableTestTask(sub_exec_counter));

		PlatformUtils::Sleep(10);
		m.cancelAndWaitForTasksToComplete();
		conPrint("Cancelling task test: num completed sub-tasks: " + toString(sub_exec_counter) + " / " + toString(NUM_TASKS));
		testAssert(sub_exec_counter >= 0 && sub_exec_counter <= (int64)NUM_TASKS * CancellableTestTask::numSubTasks());
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


#endif // BUILD_TESTS
