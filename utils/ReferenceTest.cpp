/*=====================================================================
referencetest.cpp
-----------------
File created by ClassTemplate on Tue Aug 28 18:25:25 2007
Code By Nicholas Chapman.
=====================================================================*/
#include "referencetest.h"


#include "reference.h"
#include "refcounted.h"
#include "../indigo/TestUtils.h"
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



}




