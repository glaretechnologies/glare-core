/*=====================================================================
GenerationalArray.cpp
---------------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "GenerationalArray.h"


#include "MemAlloc.h"
#include "StringUtils.h"
#include "../maths/mathstypes.h"



#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "Reference.h"
#include "../maths/PCG32.h"
#include <set>


class GenTestClass : public RefCounted
{
public:
	GenTestClass(int x_)
	:	x(x_)
	{}

	int x;
};


void glare::testGenerationalArray()
{
	{
		GenerationalArray<GenTestClass> gen_array;
		GenerationalHandle<GenTestClass> h0 = gen_array.insert(new GenTestClass(0));

		testAssert(gen_array.getRefForHandle(h0).nonNull());
		testAssert(gen_array.getRefForHandle(h0)->x == 0);

		GenerationalHandle<GenTestClass> h1 = gen_array.insert(new GenTestClass(1));

		testAssert(gen_array.getRefForHandle(h1).nonNull());
		testAssert(gen_array.getRefForHandle(h1)->x == 1);

		gen_array.erase(h0);
		testAssert(gen_array.getRefForHandle(h0).isNull());

		GenerationalHandle<GenTestClass> h2 = gen_array.insert(new GenTestClass(2));

		testAssert(gen_array.getRefForHandle(h0).isNull());
		testAssert(gen_array.getRefForHandle(h1).nonNull());
		testAssert(gen_array.getRefForHandle(h2).nonNull());

		gen_array.erase(h1);
		testAssert(gen_array.getRefForHandle(h1).isNull());

		testAssert(gen_array.getRefForHandle(h2).nonNull());
		GenerationalHandle<GenTestClass> h2_copy = h2;

		testAssert(gen_array.getRefForHandle(h2).nonNull());
		testAssert(gen_array.getRefForHandle(h2_copy).nonNull());

		gen_array.erase(h2);
		testAssert(gen_array.getRefForHandle(h2).isNull());
	}
}


#endif // BUILD_TESTS
