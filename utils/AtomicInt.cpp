/*=====================================================================
AtomicInt.cpp
-------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "AtomicInt.h"


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../utils/Task.h"
#include "../utils/TaskManager.h"
#include "../utils/Mutex.h"


class IncrementTask : public glare::Task
{
public:
	IncrementTask(glare::AtomicInt* atomic_i_) : atomic_i(atomic_i_) {}

	void run(size_t thread_index)
	{
		const int num_iters = 1000000;
		for(int i=0; i<num_iters; ++i)
			atomic_i->increment();
	}

	glare::AtomicInt* atomic_i;
};


class IncrementWIthMutexTask : public glare::Task
{
public:
	IncrementWIthMutexTask(Mutex* mutex_, int* i_ptr_) : mutex(mutex_), i_ptr(i_ptr_) {}

	void run(size_t thread_index)
	{
		const int num_iters = 1000000;
		for(int i=0; i<num_iters; ++i)
		{
			Lock lock(*mutex);
			(*i_ptr)++;
		}
	}

	Mutex* mutex;
	int* i_ptr;
};



void glare::AtomicInt::test()
{
	conPrint("glare::AtomicInt::test()");

	// Test default constructor
	{
		AtomicInt i;
		testAssert(i == 0);
	}

	// Test constructor
	{
		AtomicInt i(3);
		testAssert(i == 3);
	}

	// Test operator = 
	{
		AtomicInt i(3);
		i = 4;
		testAssert(i == 4);
	}

	// Test operator++
	{
		AtomicInt i(3);
		i++;
		testAssert(i == 4);
		testAssert(i++ == 4);
		testAssert(i == 5);
	}

	// Test operator--
	{
		AtomicInt i(3);
		i--;
		testAssert(i == 2);
		testAssert(i-- == 2);
		testAssert(i == 1);
	}


	// Test operator+=
	{
		AtomicInt i(3);
		i += 10;
		testAssert(i == 13);
		i += 20;
		testAssert(i == 33);
	}


	// Test operator-=
	{
		AtomicInt i(30);
		i -= 10;
		testAssert(i == 20);
		i -= 40;
		testAssert(i == -20);
	}


	// Test increment()
	{
		AtomicInt i(3);
		i.increment();
		testAssert(i == 4);
		testAssert(i.increment() == 4);
		testAssert(i == 5);
	}

	// Test decrement()
	{
		AtomicInt i(3);
		i.decrement();
		testAssert(i == 2);
		testAssert(i.decrement() == 2);
		testAssert(i == 1);
	}

	// Test concurrent incrementing by multiple threads.
	{
		AtomicInt atomic_i;
		glare::TaskManager m;
		for(int i=0; i<8; ++i)
			m.addTask(new IncrementTask(&atomic_i));
		m.waitForTasksToComplete();

		testAssert(atomic_i == 8 * 1000000);
	}

	// Do a performance test: Atomics vs mutexes.
	if(false)
	{
		const int num_threads = 8;
		{
			AtomicInt atomic_i;
			glare::TaskManager m(num_threads);

			Timer timer;
			for(int i=0; i<num_threads; ++i)
				m.addTask(new IncrementTask(&atomic_i));
			m.waitForTasksToComplete();

			const double elapsed = timer.elapsed();
			conPrint("incrementing using atomics took " + doubleToStringNSigFigs(elapsed, 4) + " s (" + doubleToStringNSigFigs(1.0e9 * elapsed / (8 * 1000000), 4) + " ns / increment)");


			testAssert(atomic_i == num_threads * 1000000);
		}
	
		{
			Mutex mutex;
			int shared_i = 0;
			glare::TaskManager m(num_threads);

			Timer timer;
			for(int i=0; i<num_threads; ++i)
				m.addTask(new IncrementWIthMutexTask(&mutex, &shared_i));
			m.waitForTasksToComplete();

			const double elapsed = timer.elapsed();
			conPrint("incrementing using mutex took " + doubleToStringNSigFigs(elapsed, 4) + " s (" + doubleToStringNSigFigs(1.0e9 * elapsed / (8 * 1000000), 4) + " ns / increment)");

			testAssert(shared_i == num_threads * 1000000);
		}

		/*
		With 32 threads:
		incrementing using atomics took 0.4556 s (56.95 ns / increment)
		incrementing using mutex took 2.501 s (312.6 ns / increment)

		With 8 threads:
		incrementing using atomics took 0.1073 s (13.42 ns / increment)
		incrementing using mutex took 1.927 s (240.9 ns / increment)
		
		With 1 thread:
		incrementing using atomics took 0.001734 s (0.2168 ns / increment)
		incrementing using mutex took 0.005396 s (0.6745 ns / increment)
		*/
	}

	conPrint("glare::AtomicInt::test() done.");
}


#endif // BUILD_TESTS
