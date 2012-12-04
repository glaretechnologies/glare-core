/*=====================================================================
referencetest.cpp
-----------------
File created by ClassTemplate on Tue Aug 28 18:25:25 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "ReferenceTest.h"


#if BUILD_TESTS


#include "reference.h"
#include "Maybe.h"
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


class BaseClass : public RefCounted
{
public:
	int base_x;

};

class DerivedClass : public BaseClass
{
public:
	int derived_x;
};


void ReferenceTest::run()
{
	{

		int i = 0;
		// Basic Reference test
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

		// Test references in a vector
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

		// Test returning refs from a function
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


		// Test passing reference to a function
		{
			Reference<TestClass> t(new TestClass(&i));
		
			functionWithByValueRefParam(t);
		}
		testAssert(i == 0);

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

		// Test assigning reference to itself
		{
			Reference<TestClass> t(new TestClass(&i));
		
			t = t;
		}
		testAssert(i == 0);

		// Test assigning reference to another when both point to same object.
		{
			Reference<TestClass> t(new TestClass(&i));
			Reference<TestClass> t2 = t;
		
			t = t2;
		}
		testAssert(i == 0);

		// Test assigning reference to another when both point to different objects
		{
			Reference<TestClass> t(new TestClass(&i));
			Reference<TestClass> t2(new TestClass(&i));
		
			t = t2;
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


	///////////// Test downcasting from a const Reference //////////////////////////////
	{

		const Reference<BaseClass> ref(new DerivedClass());

		int x = ref.downcast<DerivedClass>()->derived_x;
	}

	{

		const Reference<const BaseClass> ref(new DerivedClass());

		int x = ref.downcast<const DerivedClass>()->derived_x;

		// This gives a compile error, as it should:
		//ref.downcast<DerivedClass>()->derived_x++;
	}


	////////////// Test downcasting from std::vector of refs /////////////////////////
	{
		std::vector<Reference<BaseClass> > vec;

		vec.push_back(Reference<BaseClass>(new DerivedClass()));

		const std::vector<Reference<BaseClass> > const_vec = vec;

		int x = const_vec[0]->base_x;
		x = const_vec[0].downcast<DerivedClass>()->derived_x;

	}


	/////////////////// Test Maybe ///////////////////////////////////
	{
		int i = 0;
		{
			Maybe<TestClass> maybe(new TestClass(&i));
		}
		testAssert(i == 0);
	
		// Test auto-conversion to Reference
		{
			
			Reference<TestClass> ref(new TestClass(&i));

			Maybe<TestClass> maybe(ref);
		}
		testAssert(i == 0);

		// Test operator = from reference
		{
			// Test auto-conversion to Reference
			Reference<TestClass> ref(new TestClass(&i));

			Maybe<TestClass> maybe;
			maybe = ref;
		}
		testAssert(i == 0);

	}
}


#endif // BUILD_TESTS
