/*=====================================================================
VectorUnitTests.cpp
-------------------
File created by ClassTemplate on Sat Sep 02 20:18:26 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "VectorUnitTests.h"


#include "Vector.h"
#include <assert.h>
#include "../indigo/TestUtils.h"

namespace js
{




VectorUnitTests::VectorUnitTests()
{
	
}


VectorUnitTests::~VectorUnitTests()
{
	
}


#if (BUILD_TESTS)
void VectorUnitTests::run()
{

	{
		// Test (count, val) constructor
		const Vector<int, 4> v(4, 555);
		testAssert(v.size() == 4);

		// Test const_iterator
		for(Vector<int, 8>::const_iterator i = v.begin(); i != v.end(); ++i)
		{
			testAssert(*i == 555);
		}
	}


	{
		Vector<int, 4> v;
		testAssert(v.empty());
		testAssert(v.size() == 0);

		v.push_back(3);
		testAssert(!v.empty());
		testAssert(v.size() == 1);
		testAssert(v[0] == 3);

		v.push_back(4);
		testAssert(!v.empty());
		testAssert(v.size() == 2);
		testAssert(v[0] == 3);
		testAssert(v[1] == 4);

		v.resize(100);
		v[99] = 5;
		testAssert(v.size() == 100);
		testAssert(v[0] == 3);
		testAssert(v[1] == 4);

		v.resize(0);
		testAssert(v.empty());
		testAssert(v.size() == 0);
	}

}
#endif


} //end namespace js






