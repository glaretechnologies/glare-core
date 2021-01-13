/*=====================================================================
IndigoAtomic.cpp
----------------
Copyright Glare Technologies Limited 2014 -
Generated at Wed Nov 17 12:56:10 +1300 2010
=====================================================================*/
#include "IndigoAtomic.h"


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/StringUtils.h"
#include "../utils/Task.h"
#include "../utils/TaskManager.h"


class IncrementTask : public Indigo::Task
{
public:
	IncrementTask(IndigoAtomic* atomic_i_) : atomic_i(atomic_i_) {}

	void run(size_t thread_index)
	{
		const int num_iters = 100000;
		for(int i=0; i<num_iters; ++i)
			atomic_i->increment();
	}

	IndigoAtomic* atomic_i;
};


void IndigoAtomic::test()
{
	// Test default constructor
	{
		IndigoAtomic i;
		testAssert(i == 0);
	}

	// Test constructor
	{
		IndigoAtomic i = 3;
		testAssert(i == 3);
	}

	// Test operator = 
	{
		IndigoAtomic i = 3;
		i = 4;
		testAssert(i == 4);
	}

	// Test operator++
	{
		IndigoAtomic i = 3;
		i++;
		testAssert(i == 4);
		testAssert(i++ == 4);
		testAssert(i == 5);
	}

	// Test operator--
	{
		IndigoAtomic i = 3;
		i--;
		testAssert(i == 2);
		testAssert(i-- == 2);
		testAssert(i == 1);
	}


	// Test operator+=
	{
		IndigoAtomic i = 3;
		i += 10;
		testAssert(i == 13);
		i += 20;
		testAssert(i == 33);
	}


	// Test operator-=
	{
		IndigoAtomic i = 30;
		i -= 10;
		testAssert(i == 20);
		i -= 40;
		testAssert(i == -20);
	}


	// Test increment()
	{
		IndigoAtomic i = 3;
		i.increment();
		testAssert(i == 4);
		testAssert(i.increment() == 4);
		testAssert(i == 5);
	}

	// Test decrement()
	{
		IndigoAtomic i = 3;
		i.decrement();
		testAssert(i == 2);
		testAssert(i.decrement() == 2);
		testAssert(i == 1);
	}

	// Test concurrent incrementing by multiple threads.
	{
		IndigoAtomic atomic_i;
		Indigo::TaskManager m;
		for(int i=0; i<8; ++i)
			m.addTask(new IncrementTask(&atomic_i));
		m.waitForTasksToComplete();

		testAssert(atomic_i == 8 * 100000);
	}
}


#endif // BUILD_TESTS
