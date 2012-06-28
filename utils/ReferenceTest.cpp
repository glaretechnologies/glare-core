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
#include "../indigo/TestUtils.h"
#include "../maths/SSE.h"
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


void ReferenceTest::run()
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


#endif // BUILD_TESTS
