/*=====================================================================
Array.cpp
---------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "Array.h"


#if BUILD_TESTS


#include "StringUtils.h"
#include "Timer.h"
#include "ConPrint.h"
#include "TestUtils.h"


namespace glare
{


class ArrayTestClass
{};

class ArrayTestCounterClass
{
public:
	ArrayTestCounterClass(int& i_) : i(i_) { i++; }
	ArrayTestCounterClass(const ArrayTestCounterClass& other) : i(other.i) { i++; }
	~ArrayTestCounterClass() { i--; }

private:
	int& i;
};


struct ArrayTestAligned16
{
	GLARE_ALIGN(16) char data;
};



void testArray()
{
	conPrint("testArray()");

	//========================= No-arg constructor =========================

	// int type
	{
		const Array<int> v;
		testAssert(v.size() == 0);
	}


	//========================= Count constructor =========================

	// Count constructor with int type, with zero elems
	{
		const Array<int> v(
			0, // count
			123 // val
		);
		testAssert(v.size() == 0);
	}

	// Count constructor with int type, for various counts
	{
		for(size_t i=0; i<4; ++i)
		{
			const Array<int> v(
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
		const Array<int> v(
			10, // count
			123 // val
		);
		testAssert(v.size() == 10);
		for(size_t i=0; i<v.size(); ++i)
			testAssert(v[i] == 123);
	}

	// Count constructor with TestClass
	{
		const Array<ArrayTestClass> v(
			10 // count
		);
		testAssert(v.size() == 10);
	}

	// Count constructor with TestCounterClass
	{
		int ob_count = 0;
		ArrayTestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			const Array<ArrayTestCounterClass> v(
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
			const Array<int> v(
				count, // count
				123 // val
			);


			// Copy construct v2 from v
			const Array<int> v2(v);

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
			ArrayTestCounterClass dummy(ob_count);
			testAssert(ob_count == 1);

			{
				const Array<ArrayTestCounterClass> v(
					count, // count
					dummy
				);
				testAssert(v.size() == count);
				testAssert(ob_count == (int)count + 1);
			
				// Copy construct v2 from v
				const Array<ArrayTestCounterClass> v2(v);

				testAssert(v2.size() == v.size());
				testAssert(ob_count == 2*(int)count + 1);
			}

			testAssert(ob_count == 1);
		}
	}



	//========================= operator = =========================

	// Test assigning to self
	{
		Array<int> v(
			10, // count
			123 // val
		);
		
#if defined(__clang__) && (__clang_major__ >= 9)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wself-assign-overloaded"
#endif

		v = v;
		
#if defined(__clang__) && (__clang_major__ >= 9)
#pragma GCC diagnostic pop
#endif

		testAssert(v.size() == 10);
		for(size_t i=0; i<v.size(); ++i)
			testAssert(v[i] == 123);
	}


	// Test assigning from empty vector to empty vector
	{
		const Array<int> v;
		Array<int> v2;
		v2 = v;

		testAssert(v.size() == 0);
		testAssert(v2.size() == 0);
	}


	// With int type
	{
		const Array<int> v(
			10, // count
			123 // val
		);

		Array<int> v2;
		v2 = v;

		testAssert(v2.size() == 10);
		for(size_t i=0; i<v2.size(); ++i)
			testAssert(v2[i] == 123);
	}

	// With TestCounterClass, with existing capacity < other.size()
	{
		int ob_count = 0;
		ArrayTestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			const Array<ArrayTestCounterClass> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			Array<ArrayTestCounterClass> v2;
			v2 = v;

			testAssert(v2.size() == 10);
			testAssert(ob_count == 21);
		}

		testAssert(ob_count == 1);
	}

	// With TestCounterClass, with existing capacity >= other.size()
	{
		int ob_count = 0;
		ArrayTestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			const Array<ArrayTestCounterClass> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			//Array<TestCounterClass> v2;
			//v2.resize(100, dummy);
			////testAssert(v2.capacity() > 10);
			//v2 = v;

			//testAssert(v2.size() == 10);
			//testAssert(ob_count == 21);
		}

		testAssert(ob_count == 1);
	}


	

	//========================= back =========================


	// With int type, lots of push backs
	{
		size_t N = 10;
		Array<int> v(N);
		for(size_t i=0; i<N; ++i)
			v[i] = (int)i;

		testAssert(v.back() == (int)N-1);

		testAssert(v.size() == N);
		//testAssert(v.capacity() >= N);

		for(size_t i=0; i<N; ++i)
			testAssert(v[i] == (int)i);
	}

	//========================= operator [] =========================


	// With int type, lots of push backs
	{
		size_t N = 100;
		Array<int> v(N);
		
		for(size_t i=0; i<N; ++i)
			v[i] = (int)i;

		for(size_t q=0; q<v.size(); ++q)
			testAssert(v[q] == (int)q);

		testAssert(v.size() == N);
		//testAssert(v.capacity() >= N);

		for(size_t i=0; i<N; ++i)
			testAssert(v[i] == (int)i);
	}

	conPrint("testArray() done.");
}


} // end namespace glare


#endif // BUILD_TESTS
