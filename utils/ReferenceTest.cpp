/*=====================================================================
ReferenceTest.cpp
-----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "ReferenceTest.h"


#if BUILD_TESTS


#include "Reference.h"
#include "Maybe.h"
#include "VRef.h"
#include "RefCounted.h"
#include "ThreadSafeRefCounted.h"
#include "WeakRefCounted.h"
#include "WeakReference.h"
#include "MyThread.h"
#include "ConPrint.h"
#include "Timer.h"
#include "StringUtils.h"
#include "MemAlloc.h"
#include "TestUtils.h"
#include <vector>


namespace ReferenceTest
{


class TestClass : public RefCounted
{
public:
	TestClass(int* i_)
	:	i(i_)
	{
		(*i)++;
	}
	~TestClass()
	{
		(*i)--;
	}

	int f() { return 5; }

private:
	int* i;
};

// The RefCounted reference count should add 8 bytes, and then the int* pointer will be 4 or 8 bytes, resulting in a total size of 16 bytes.
//static_assert(sizeof(TestClass) == 16, "sizeof(TestClass) == 16");


class DerivedTestClass : public TestClass
{
public:
	DerivedTestClass(int* i_) : TestClass(i_) {}

};


static Reference<DerivedTestClass> someFunc(int* i)
{
	return Reference<DerivedTestClass>(new DerivedTestClass(i));
}


static void functionWithByValueRefParam(Reference<TestClass> ref)
{
	//int b = ref->f();
	//b;
}


static void functionWithByRefRefParam(const Reference<TestClass>& ref)
{
	//int b = ref->f();
	//b;
}


#ifdef _WIN32
#pragma warning(push)
#pragma warning(disable:4324) // Disable 'structure was padded due to __declspec(align())' warning.
#endif
SSE_CLASS_ALIGN AlignedTestClass : public RefCounted
{
public:
	GLARE_ALIGNED_16_NEW_DELETE

	AlignedTestClass(int* i_)
	:	i(i_)
	{
		(*i)++;
	}
	~AlignedTestClass()
	{
		(*i)--;
	}

	int f() { return 5; }

private:
	int* i;
};
#ifdef _WIN32
#pragma warning(pop)
#endif


///////////////////////////////////////////////////////////////////////


class ThreadSafeTestClass : public ThreadSafeRefCounted
{
public:
	ThreadSafeTestClass()
	{
	}
	~ThreadSafeTestClass()
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
	virtual ~BaseClass() {}
	virtual void f() {} // Make this class polymorphic
	int base_x;

};

class DerivedClass : public BaseClass
{
public:
	int derived_x;
};


class TestWeakRefCountedClass : public WeakRefCounted
{
public:
	TestWeakRefCountedClass(int* i_)
	:	i(i_)
	{
		(*i)++;
	}
	~TestWeakRefCountedClass()
	{
		(*i)--;
	}

private:
	int* i;
};


void run()
{
	/////////////////////// Test takeFrom() ///////////////////////
	{
		int i = 0;
		{
			Reference<TestClass> ref = new TestClass(&i);
			
			testAssert(ref->getRefCount() == 1);
			testAssert(i == 1);

			Reference<TestClass> ref2;
			ref2.takeFrom(ref);

			testAssert(ref.isNull());
			testAssert(ref2->getRefCount() == 1);
			testAssert(i == 1);
		}
		testAssert(i == 0);
	}
	// Test takeFrom when ref points to another object
	{
		int i = 0;
		{
			Reference<TestClass> ref = new TestClass(&i);
			
			testAssert(ref->getRefCount() == 1);
			testAssert(i == 1);

			Reference<TestClass> ref2 = new TestClass(&i);
			testAssert(i == 2);
			ref2.takeFrom(ref);

			testAssert(ref.isNull());
			testAssert(ref2->getRefCount() == 1);
			testAssert(i == 1);
		}
		testAssert(i == 0);
	}
	// Test takeFrom taking from self
	{
		int i = 0;
		{
			Reference<TestClass> ref = new TestClass(&i);
			testAssert(ref->getRefCount() == 1);
			testAssert(i == 1);

			ref.takeFrom(ref);

			testAssert(ref->getRefCount() == 1);
			testAssert(i == 1);
		}
		testAssert(i == 0);
	}

	/////////////////////// Test setAsNotIndependentlyHeapAllocated() ///////////////////////
	{
		int i = 0;
		{
			TestClass testob(&i);
			testob.setAsNotIndependentlyHeapAllocated();
			testAssert(i == 1);

			// Create a reference to testob and then destroy the reference.
			// Without the setAsNotIndependentlyHeapAllocated call, this would incorrectly destroy testob.
			Reference<TestClass> ref = &testob;
			ref = NULL;
		}
		testAssert(i == 0);
	}

	/////////////////////// Test setAsNotIndependentlyHeapAllocated() on ThreadSafeRefCounted ///////////////////////
	{
		{
			ThreadSafeTestClass testob;
			testob.setAsNotIndependentlyHeapAllocated();

			// Create a reference to testob and then destroy the reference.
			// Without the setAsNotIndependentlyHeapAllocated call, this would incorrectly destroy testob.
			Reference<ThreadSafeTestClass> ref = &testob;
			ref = NULL;
		}
	}


	/////////////////////// Test weak references ///////////////////////
	{
		int i = 0;
		Reference<TestWeakRefCountedClass> strongref = new TestWeakRefCountedClass(&i); // Make strong reference

		testAssert(i == 1);
		testAssert(strongref->getRefCount() == 1);
		testAssert(strongref->control_block->ob_is_alive);


		// Make weak reference
		WeakReference<TestWeakRefCountedClass> weakref(strongref);

		testAssert(strongref->control_block.ptr() == weakref.control_block.ptr());

		testAssert(strongref->getRefCount() == 1);
		testAssert(weakref.control_block->ob_is_alive);


		// Upgrade weak reference to strong ref
		Reference<TestWeakRefCountedClass> strongref2 = weakref.upgradeToStrongRef();
		testAssert(strongref2.nonNull());
		testAssert(strongref2.ptr() == strongref.ptr());
		testAssert(strongref->getRefCount() == 2);

		strongref2 = NULL; // Destroy strong ref 2.

		// Destroy strong ref.  This should destroy the referenced object as well.
		strongref = NULL;
		testAssert(i == 0);

		testAssert(weakref.control_block->ob_is_alive == false);

		testAssert(weakref.upgradeToStrongRef().isNull());
	}

	/////////////////////// Test Reference ///////////////////////
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


		// Test copy constructor
		{
			Reference<TestClass> t = Reference<TestClass>(new TestClass(&i));
		}
		testAssert(i == 0);

		// Test from-pointer constructor
		{
			Reference<TestClass> t = new TestClass(&i);
		}
		testAssert(i == 0);

		{
			Reference<ThreadSafeTestClass> t = new ThreadSafeTestClass();
		}

		{
			Reference<TestClass> t;
			t = new TestClass(&i);
		}
		testAssert(i == 0);

/*	
		{
			std::shared_ptr<TestClass> t;
			t = std::shared_ptr<TestClass>(new TestClass(&i));
		}
		testAssert(i == 0);
*/

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
				testAssert(MemAlloc::isAlignedTo(ref.getPointer(), 16));

				refs.push_back(ref);
			}
		}

		testAssert(i == 0);

		// Test assigning reference to itself
		{
			Reference<TestClass> t(new TestClass(&i));
		
#if defined(__clang__) && (__clang_major__ >= 9)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-assign-overloaded"
#endif
		
			t = t;
			
#if defined(__clang__) && (__clang_major__ >= 9)
#pragma GCC diagnostic pop
#endif
		}
		testAssert(i == 0);

		// Test assigning reference to itself by pointer
		{
			Reference<TestClass> t(new TestClass(&i));
		
			t = t.getPointer();
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

		std::vector<Reference<TestThread> > threads;
		for(int i=0; i<8; ++i)
		{
			threads.push_back(new TestThread(shared_ref));
			threads.back()->launch(/*false*/);
		}

		// Wait for completion, then delete threads
		int total = 0;
		for(size_t i=0; i<threads.size(); ++i)
		{
			threads[i]->join();

			total += threads[i]->sum;
		}

		// Delete threads
		threads.resize(0);


		testAssert(shared_ref->getRefCount() == 1);

	
		double time_per_incr = timer.elapsed() / 8000000;

		conPrint("time elapsed: " + timer.elapsedString());
		conPrint("total: " + toString(total));
		conPrint("time per incr: " + doubleToStringScientific(time_per_incr) + " s");

	}


	///////////// Test downcasting from a const Reference //////////////////////////////
	{

		const Reference<BaseClass> ref(new DerivedClass());

		ref.downcast<DerivedClass>()->derived_x = 1;
		printVar(ref.downcast<DerivedClass>()->derived_x);
	}

	{

		const Reference<const BaseClass> ref(new DerivedClass());

		int x = ref.downcast<const DerivedClass>()->derived_x;
		printVar(x);

		// This gives a compile error, as it should:
		//ref.downcast<DerivedClass>()->derived_x++;
	}


	///////////// Test downcastToPtr from a const Reference //////////////////////////////
	{

		const Reference<BaseClass> ref(new DerivedClass());

		DerivedClass* derived_ptr = ref.downcastToPtr<DerivedClass>();
		derived_ptr->derived_x = 1;
	}

	{

		const Reference<const BaseClass> ref(new DerivedClass());

		const DerivedClass* derived_ptr = ref.downcastToPtr<const DerivedClass>();
		printVar(derived_ptr->derived_x);

		// This gives a compile error, as it should:
		//ref.downcastToPtr<DerivedClass>()->derived_x++;
	}


	////////////// Test downcasting from std::vector of refs /////////////////////////
	{
		std::vector<Reference<BaseClass> > vec;

		vec.push_back(Reference<BaseClass>(new DerivedClass()));

		const std::vector<Reference<BaseClass> > const_vec = vec;

		int x = const_vec[0]->base_x;
		x = const_vec[0].downcast<DerivedClass>()->derived_x;

		TestUtils::silentPrint(toString(x));

	}


	/////////////////// Test conversion from non-const to const reference ///////////////////////////////////
	{

		// Test auto-conversion to Reference 
		int i = 0;
		{
			Reference<TestClass> a(new TestClass(&i));

			Reference<const TestClass> b = a;
		}
		testAssert(i == 0);

		// Test with operator =  
		i = 0;
		{
			Reference<TestClass> a(new TestClass(&i));

			Reference<const TestClass> b;
			b = a;
		}
		testAssert(i == 0);
	}



	/////////////////// Test VRef ///////////////////////////////////
	{
		int i = 0;

		// Test auto-conversion from VRef to Reference 
		{
			VRef<TestClass> vref(new TestClass(&i));

			Reference<TestClass> ref = vref;
		}
		testAssert(i == 0);

		// Test auto-conversion from VRef<const T> to Reference<const T>
		{
			VRef<const TestClass> vref(new TestClass(&i));

			Reference<const TestClass> ref = vref;
		}
		testAssert(i == 0);
	}

	/////////////////// Test conversion from non-const VRef to const Reference ///////////////////////////////////
	{
		int i = 0;
		{
			VRef<TestClass> a(new TestClass(&i));

			Reference<const TestClass> b = a;
		}
		testAssert(i == 0);
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


	// Perf test
	{
		const int N = 1'000'000;
		{
			int z = 0;
			Reference<TestClass> ref = new TestClass(&z);
		

			{
				Timer timer;
				for(int i=0; i<N; ++i)
				{
					ref->incRefCount();
					ref->decRefCount();
				}
				const double time_per_iter = timer.elapsed() / N;
				conPrint("Reference incr + decr time: " + doubleToStringNSigFigs(time_per_iter * 1.0e9, 4) + " ns");
			}
		}
		{
			Reference<ThreadSafeTestClass> ref = new ThreadSafeTestClass();

			{
				Timer timer;
				for(int i=0; i<N; ++i)
				{
					ref->incRefCount();
					ref->decRefCount();
				}
				const double time_per_iter = timer.elapsed() / N;
				conPrint("ThreadSafeRefCounted incr + decr time: " + doubleToStringNSigFigs(time_per_iter * 1.0e9, 4) + " ns");
			}
		}
	}
}


} // end namespace ReferenceTest


#endif // BUILD_TESTS
