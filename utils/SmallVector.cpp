/*=====================================================================
SmallSmallVector.cpp
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-03-07 14:27:12 +0000
=====================================================================*/
#include "SmallVector.h"


#if BUILD_TESTS



#include "stringutils.h"
#include "timer.h"
#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"
#include <vector>
#include <assert.h>


namespace SmallVectorTest
{


class TestClass
{};

class TestCounterClass
{
public:
	TestCounterClass(int& i_) : i(i_) { i++; }
	TestCounterClass(const TestCounterClass& other) : i(other.i) { i++; }
	~TestCounterClass() { i--; }

	// std::vector use with GCC seems to require this.
	//TestCounterClass& TestCounterClass::operator=(const TestCounterClass&)

private:
	int& i;
};


void test()
{
	conPrint("SmallVectorTest()");

	printVar(sizeof(TestClass));

	//========================= No-arg constructor =========================

	// int type
	{
		const SmallVector<int, 4> v;
		testAssert(v.size() == 0);
	}


	//========================= Count constructor =========================

	// Count constructor with int type, with zero elems
	{
		const SmallVector<int, 4> v(
			0, // count
			123 // val
		);
		testAssert(v.size() == 0);
	}

	// Count constructor with int type, for various counts
	{
		for(size_t i=0; i<100; ++i)
		{
			const SmallVector<int, 4> v(
				i, // count
				123 // val
			);
			testAssert(v.size() == i);
			for(size_t q=0; q<v.size(); ++q)
				testAssert(v[q] == 123);
		}
	}

	// Count constructor with int type
	{
		const SmallVector<int, 4> v(
			10, // count
			123 // val
		);
		testAssert(v.size() == 10);
		for(size_t i=0; i<v.size(); ++i)
			testAssert(v[i] == 123);
	}

	// Count constructor with TestClass
	{
		const SmallVector<TestClass, 4> v(
			10 // count
		);
		testAssert(v.size() == 10);
	}

	// Count constructor with TestCounterClass
	{
		int ob_count = 0;
		TestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			const SmallVector<TestCounterClass, 4> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
		}

		testAssert(ob_count == 1);
	}


	//========================= Copy constructor =========================


	// With int type, for various counts
	{
		for(size_t count=0; count < 100; ++count)
		{
			const SmallVector<int, 4> v(
				count, // count
				123 // val
			);

			// Copy construct v2 from v
			const SmallVector<int, 4> v2(v);

			testAssert(v2.size() == v.size());
			for(size_t i=0; i<v2.size(); ++i)
				testAssert(v2[i] == 123);
		}
	}

	// With TestCounterClass
	{
		for(size_t count=0; count < 100; ++count)
		{
			int ob_count = 0;
			TestCounterClass dummy(ob_count);
			testAssert(ob_count == 1);

			{
				const SmallVector<TestCounterClass, 4> v(
					count, // count
					dummy
				);
				testAssert(v.size() == count);
				testAssert(ob_count == count + 1);
			
				// Copy construct v2 from v
				const SmallVector<TestCounterClass, 4> v2(v);

				testAssert(v2.size() == v.size());
				testAssert(ob_count == 2*count + 1);
			}

			testAssert(ob_count == 1);
		}
	}



	//========================= operator = =========================

	// Test assigning to self
	{
		SmallVector<int, 4> v(
			10, // count
			123 // val
		);

		v = v;

		testAssert(v.size() == 10);
		for(size_t i=0; i<v.size(); ++i)
			testAssert(v[i] == 123);
	}


	// Test assigning from empty vector to empty vector
	{
		const SmallVector<int, 4> v;
		SmallVector<int, 4> v2;
		v2 = v;

		testAssert(v.size() == 0);
		testAssert(v2.size() == 0);
	}


	// With int type
	{
		const SmallVector<int, 4> v(
			10, // count
			123 // val
		);

		SmallVector<int, 4> v2;
		v2 = v;

		testAssert(v2.size() == 10);
		for(size_t i=0; i<v2.size(); ++i)
			testAssert(v2[i] == 123);
	}

	// With TestCounterClass, with existing capacity < other.size()
	{
		int ob_count = 0;
		TestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			const SmallVector<TestCounterClass, 4> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			SmallVector<TestCounterClass, 4> v2;
			v2 = v;

			testAssert(v2.size() == 10);
			testAssert(ob_count == 21);
		}

		testAssert(ob_count == 1);
	}

	// With TestCounterClass, with existing capacity >= other.size()
	{
		int ob_count = 0;
		TestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			const SmallVector<TestCounterClass, 4> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			SmallVector<TestCounterClass, 4> v2;
			v2.resize(100, dummy);
			testAssert(v2.capacity() > 10);
			v2 = v;

			testAssert(v2.size() == 10);
			testAssert(ob_count == 21);
		}

		testAssert(ob_count == 1);
	}


	//========================= reserve =========================
	/*
	
	// With int type
	{
		SmallVector<int, 4> v(
			10, // count
			123 // val
		);

		testAssert(v.size() == 10);
		testAssert(v.capacity() == 10);

		v.reserve(100);

		testAssert(v.size() == 10);
		testAssert(v.capacity() == 100);
	}


	// With TestCounterClass
	{
		int ob_count = 0;
		TestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			SmallVector<TestCounterClass, 4> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			v.reserve(100);

			testAssert(v.size() == 10);
			testAssert(v.capacity() == 100);
			testAssert(ob_count == 11);

			v.reserve(50);

			testAssert(v.size() == 10);
			testAssert(v.capacity() == 100);
			testAssert(ob_count == 11);
		}

		testAssert(ob_count == 1);
	}

	*/
	//========================= resize =========================

	// With int type, zero size
	{
		SmallVector<int, 4> v;
		v.resize(0);
		testAssert(v.size() == 0);
	}


	// With int type
	{
		SmallVector<int, 4> v(
			10, // count
			123 // val
		);

		testAssert(v.size() == 10);
		testAssert(v.capacity() == 10);

		v.resize(100);

		testAssert(v.size() == 100);
		testAssert(v.capacity() >= 100);
	}


	// With TestCounterClass
	{
		int ob_count = 0;
		TestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			SmallVector<TestCounterClass, 4> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			// Resize a lot larger to cause capacity expansion
			v.resize(100, dummy);

			testAssert(v.size() == 100);
			testAssert(v.capacity() >= 100);
			testAssert(ob_count == 101);

			// Resize just a little bit larger to avoid capacity expansion
			//v.reserve(101);
			v.resize(101, dummy);

			testAssert(v.size() == 101);
			testAssert(v.capacity() >= 101);
			testAssert(ob_count == 102);

			v.resize(20, dummy);

			testAssert(v.size() == 20);
			testAssert(v.capacity() >= 100);
			testAssert(ob_count == 21);

			// Try resizing to same size
			v.resize(20, dummy);

			testAssert(v.size() == 20);
			testAssert(v.capacity() >= 100);
			testAssert(ob_count == 21);

			// Try resizing to zero.
			v.resize(0, dummy);

			testAssert(v.size() == 0);
			//testAssert(v.capacity() >= 100);
			testAssert(ob_count == 1);
		}

		testAssert(ob_count == 1);
	}

	// With TestCounterClass
	{
		for(size_t start_size=0; start_size<10; ++start_size)
		for(size_t end_size=0; end_size<100; ++end_size)
		{
			int ob_count = 0;
			TestCounterClass dummy(ob_count);
			testAssert(ob_count == 1);

			{
				SmallVector<TestCounterClass, 4> v(
					start_size, // count
					dummy
				);
				testAssert(v.size() == start_size);
				testAssert(ob_count == start_size + 1);
			
				// Resize to end_size
				v.resize(end_size, dummy);

				testAssert(v.size() == end_size);
				//testAssert(v.capacity() >= 100);
				testAssert(ob_count == end_size + 1);

			}

			testAssert(ob_count == 1);
		}
	}


	//========================= push_back =========================

	// With int type
	{
		SmallVector<int, 4> v;
		v.push_back(1);
		testAssert(v.size() == 1);
		testAssert(v[0] == 1);

		v.push_back(2);
		v.push_back(3);
		v.push_back(4);

		testAssert(v.size() == 4);

		for(int i=0; i<4; ++i)
			testAssert(v[i] == i + 1);
	}

	// With int type, lots of push backs
	{
		SmallVector<int, 4> v;
		const size_t N = 10000;
		for(size_t i=0; i<N; ++i)
		{
			v.push_back((int)i);
			testAssert(v.size() == i + 1);
			testAssert(v.capacity() >= i + 1);
		}

		testAssert(v.size() == N);
		testAssert(v.capacity() >= N);

		for(int i=0; i<N; ++i)
			testAssert(v[i] == (int)i);
	}



	// With TestCounterClass
	{
		int ob_count = 0;
		TestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			SmallVector<TestCounterClass, 4> v;

			const size_t N = 10000;
			for(size_t i=0; i<N; ++i)
			{
				v.push_back(dummy);

				testAssert(v.size() == i + 1);
				testAssert(ob_count == v.size() + 1);
			}
		}

		testAssert(ob_count == 1);
	}

	//========================= back =========================


	// With int type, lots of push backs
	{
		SmallVector<int, 4> v;
		size_t N = 10000;
		for(size_t i=0; i<N; ++i)
		{
			v.push_back((int)i);

			testAssert(v.back() == (int)i);
		}

		testAssert(v.size() == N);
		testAssert(v.capacity() >= N);

		for(int i=0; i<N; ++i)
			testAssert(v[i] == (int)i);
	}

	//========================= operator [] =========================


	// With int type, lots of push backs
	{
		SmallVector<int, 4> v;
		size_t N = 100;
		for(size_t i=0; i<N; ++i)
		{
			v.push_back((int)i);

			testAssert(v.back() == (int)i);

			for(size_t q=0; q<v.size(); ++q)
				testAssert(v[q] == (int)q);
		}

		testAssert(v.size() == N);
		testAssert(v.capacity() >= N);

		for(int i=0; i<N; ++i)
			testAssert(v[i] == (int)i);
	}




	//========================= pop_back =========================

	// With int type
	{
		SmallVector<int, 4> v;
		const size_t N = 100;
		for(size_t i=0; i<N; ++i)
			v.push_back((int)i);

		for(size_t i=0; i<N; ++i)
		{
			v.pop_back();

			testAssert(v.size() == N - (i + 1));

			// Check vector elements
			for(size_t q=0; q<v.size(); ++q)
			{
				testAssert(v[q] == (int)q);
			}
		}
	}



	// With TestCounterClass
	{
		const size_t N = 100;

		int ob_count = 0;
		TestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			SmallVector<TestCounterClass, 4> v;
			
			// Push back N times
			for(size_t i=0; i<N; ++i)
				v.push_back(dummy);

			testAssert(ob_count == 1 + v.size());
			
			// Pop back N times
			for(size_t i=0; i<N; ++i)
			{
				v.pop_back();

				testAssert(v.size() == N - (i + 1));
				testAssert(ob_count == 1 + v.size());
			}
		}

		testAssert(ob_count == 1);
	}
}


} // end namespace SmallVectorTest


#endif // BUILD_TESTS
