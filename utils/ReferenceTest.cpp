/*=====================================================================
referencetest.cpp
-----------------
File created by ClassTemplate on Tue Aug 28 18:25:25 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "ReferenceTest.h"


#if BUILD_TESTS


#include "reference.h"
#include "refcounted.h"
#include "ThreadSafeRefCounted.h"
#include "../indigo/TestUtils.h"
#include "../maths/SSE.h"
#include "mythread.h"
#include "../indigo/globals.h"
#include "../utils/timer.h"
#include "../utils/stringutils.h"
#include <vector>


ReferenceTest::ReferenceTest()
{
	
}


ReferenceTest::~ReferenceTest()
{
	
}


class TestClass : public RefCounted
{
public:
	TestClass(int* i_)
	:	i(i_)
	{
		(*i)++;
	}
	virtual ~TestClass()
	{
		(*i)--;
	}

	int f() { return 5; }

private:
	int* i;
};

class DerivedTestClass : public TestClass
{
public:
	DerivedTestClass(int* i_) : TestClass(i_) {}

};


static Reference<DerivedTestClass> someFunc(int* i)
{
	return Reference<DerivedTestClass>(new DerivedTestClass(i));
}


void functionWithByValueRefParam(Reference<TestClass> ref)
{
	int b = ref->f();
}


void functionWithByRefRefParam(const Reference<TestClass>& ref)
{
	int b = ref->f();
}


SSE_CLASS_ALIGN AlignedTestClass : public RefCounted
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	AlignedTestClass(int* i_)
	:	i(i_)
	{
		(*i)++;
	}
	virtual ~AlignedTestClass()
	{
		(*i)--;
	}

	int f() { return 5; }

private:
	int* i;
};


///////////////////////////////////////////////////////////////////////


class ThreadSafeTestClass : public ThreadSafeRefCounted
{
public:
	ThreadSafeTestClass()
	{
	}
	virtual ~ThreadSafeTestClass()
	{
	}

	int f() { return 1; }

private:
};


class TestThread : public MyThread
{
public:
	TestThread(const Reference<ThreadSafeTestClass>& ref_) : ref(ref_), sum(0) {}
	
	virtual void run()
	{
		conPrint("Running ref counting test thread...");

		for(int i=0; i<1000000; ++i)
		{
			// Make a new reference
			Reference<ThreadSafeTestClass> ref2 = ref;

			sum += ref2->f();
		}
		conPrint("Thread done.");
	}

	Reference<ThreadSafeTestClass> ref;
	int sum;
};


///////////////////////////////////////////////////////////////////////////////


void ReferenceTest::run()
{
	{
		int i = 0;
		{
			Reference<TestClass> t(new TestClass(&i));
			testAssert(i == 1);
		}
		testAssert(i == 0);

		{
			Reference<TestClass> t(new TestClass(&i));
			testAssert(i == 1);
			Reference<TestClass> t2(new TestClass(&i));
			testAssert(i == 2);
			{
				Reference<TestClass> t3(new TestClass(&i));
				testAssert(i == 3);
			}
			testAssert(i == 2);
		}
		testAssert(i == 0);


		{
		std::vector<Reference<TestClass> > v;
		testAssert(i == 0);
		v.push_back(Reference<TestClass>(new TestClass(&i)));
		v.push_back(Reference<TestClass>(new TestClass(&i)));
		testAssert(i == 2);
		v.pop_back();
		testAssert(i == 1);
		}
		testAssert(i == 0);


		// Test automatic conversion from derived to base class.
		{
			Reference<DerivedTestClass> d(new DerivedTestClass(&i));

			Reference<TestClass> t = d;
		}

		testAssert(i == 0);

		{
			Reference<DerivedTestClass> d = someFunc(&i);
		}

		{
			Reference<TestClass> t = someFunc(&i);
		}

		testAssert(i == 0);
	

		// Test automatic conversion from non-const to const ref
		{
			Reference<TestClass> t(new TestClass(&i));
			Reference<const TestClass> t_const = t;
		}

		testAssert(i == 0);


		{
			Reference<TestClass> t(new TestClass(&i));
		
			functionWithByValueRefParam(t);
		}

		{
			Reference<TestClass> t(new TestClass(&i));
		
			functionWithByRefRefParam(t);
		}

		testAssert(i == 0);

		// Test AlignedTestClass references, make sure it's always aligned
		{
			std::vector<Reference<AlignedTestClass> > refs;
			for(int z=0; z<1000; ++z)
			{
				Reference<AlignedTestClass> ref(new AlignedTestClass(&i));

				// Check the object is aligned.
				testAssert(SSE::isAlignedTo(ref.getPointer(), 16));

				refs.push_back(ref);
			}
		}

		testAssert(i == 0);
	}


	//////////////////////// Test ThreadSafeRefCounted ///////////////////////
	{
		Timer timer;

		Reference<ThreadSafeTestClass> shared_ref(new ThreadSafeTestClass());

		std::vector<TestThread*> threads;
		for(int i=0; i<8; ++i)
		{
			threads.push_back(new TestThread(shared_ref));
			threads.back()->launch(false);
		}

		// Wait for completion, then delete threads
		int total = 0;
		for(int i=0; i<threads.size(); ++i)
		{
			threads[i]->join();

			total += threads[i]->sum;

			delete threads[i];
		}

		testAssert(shared_ref->getRefCount() == 1);

	
		double time_per_incr = timer.elapsed() / 8000000;

		conPrint("time elapsed: " + timer.elapsedString());
		conPrint("total: " + toString(total));
		conPrint("time per incr: " + doubleToStringScientific(time_per_incr) + " s");

	}
}


#endif // BUILD_TESTS
