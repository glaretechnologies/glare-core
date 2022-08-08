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


class IncrementTask : public glare::Task
{
public:
	IncrementTask(glare::AtomicInt* atomic_i_) : atomic_i(atomic_i_) {}

	void run(size_t thread_index)
	{
		const int num_iters = 100000;
		for(int i=0; i<num_iters; ++i)
			atomic_i->increment();
	}

	glare::AtomicInt* atomic_i;
};


void glare::AtomicInt::test()
{
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
		AtomicInt i = AtomicInt(3);
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

		testAssert(atomic_i == 8 * 100000);
	}
}


#endif // BUILD_TESTS
