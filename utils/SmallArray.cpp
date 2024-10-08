/*=====================================================================
SmallArray.cpp
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "SmallArray.h"


#if BUILD_TESTS


#include "StringUtils.h"
#include "Timer.h"
#include "../utils/TestUtils.h"
#include "../indigo/globals.h"
#include <vector>
#include <assert.h>


namespace SmallArrayTest
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


struct TestAligned16
{
	GLARE_ALIGN(16) char data;
};


using namespace glare;


void test()
{
	conPrint("SmallArrayTest()");

	//========================= Test AlignOf =========================
	{
		static_assert(AlignOf<int>::Alignment == 4, "alignment");
		static_assert(AlignOf<double>::Alignment == 8, "alignment");
		static_assert(AlignOf<char*>::Alignment == sizeof(char*), "alignment");
		static_assert(AlignOf<TestAligned16>::Alignment == 16, "alignment");
	}

	//========================= Test AlignedCharArray =========================
	{
		static_assert(GLARE_ALIGNMENT(AlignedCharArray<4, 1>) == 4, "alignment");
		AlignedCharArray<4, 1> test;
		testAssert((uint64)&test.buf % 4 == 0);
	}
	{
		static_assert(GLARE_ALIGNMENT(AlignedCharArray<8, 1>) == 8, "alignment");
		AlignedCharArray<8, 1> test;
		testAssert((uint64)&test.buf % 8 == 0);
	}
	{
		static_assert(GLARE_ALIGNMENT(AlignedCharArray<16, 1>) == 16, "alignment");
		AlignedCharArray<16, 1> test;
		testAssert((uint64)&test.buf % 16 == 0);
	}
	{
		static_assert(GLARE_ALIGNMENT(AlignedCharArray<32, 1>) == 32, "alignment");
		AlignedCharArray<32, 1> test;
		testAssert((uint64)&test.buf % 32 == 0);
	}


	//========================= Check alignment when the value type has large alignment =========================
	{
		static_assert(GLARE_ALIGNMENT(SmallArray<TestAligned16, 4>) == 16, "alignment");
		const SmallArray<TestAligned16, 4> v;
		testAssert((uint64)v.data() % 16 == 0);
	}

	//========================= No-arg constructor =========================

	// int type
	{
		const SmallArray<int, 4> v;
		testAssert(v.size() == 0);
	}


	//========================= Count constructor =========================

	// Count constructor with int type, with zero elems
	{
		const SmallArray<int, 4> v(
			0, // count
			123 // val
		);
		testAssert(v.size() == 0);
	}

	// Count constructor with int type, for various counts
	{
		for(size_t i=0; i<4; ++i)
		{
			const SmallArray<int, 4> v(
				i, // count
				123 // val
			);
			testAssert(!v.storingOnHeap());
			testAssert(v.size() == i);
			for(size_t q=0; q<v.size(); ++q)
				testAssert(v[q] == 123);
		}
	}

	// Count constructor with int type
	{
		const SmallArray<int, 4> v(
			10, // count
			123 // val
		);
		testAssert(v.storingOnHeap());
		testAssert(v.size() == 10);
		for(size_t i=0; i<v.size(); ++i)
			testAssert(v[i] == 123);
	}

	// Count constructor with TestClass
	{
		const SmallArray<TestClass, 4> v(
			10 // count
		);
		testAssert(v.storingOnHeap());
		testAssert(v.size() == 10);
	}

	// Count constructor with TestCounterClass
	{
		int ob_count = 0;
		TestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			const SmallArray<TestCounterClass, 4> v(
				10, // count
				dummy
			);
			testAssert(v.storingOnHeap());
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
			const SmallArray<int, 4> v(
				count, // count
				123 // val
			);

			testAssert(v.storingOnHeap() == (count > 4));

			// Copy construct v2 from v
			const SmallArray<int, 4> v2(v);

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
				const SmallArray<TestCounterClass, 4> v(
					count, // count
					dummy
				);
				testAssert(v.size() == count);
				testAssert(ob_count == (int)count + 1);
			
				// Copy construct v2 from v
				const SmallArray<TestCounterClass, 4> v2(v);

				testAssert(v2.size() == v.size());
				testAssert(ob_count == 2*(int)count + 1);
			}

			testAssert(ob_count == 1);
		}
	}



	//========================= operator = =========================

	// Test assigning to self
	{
		SmallArray<int, 4> v(
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
		const SmallArray<int, 4> v;
		SmallArray<int, 4> v2;
		v2 = v;

		testAssert(v.size() == 0);
		testAssert(v2.size() == 0);
	}


	// With int type
	{
		const SmallArray<int, 4> v(
			10, // count
			123 // val
		);

		SmallArray<int, 4> v2;
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
			const SmallArray<TestCounterClass, 4> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			SmallArray<TestCounterClass, 4> v2;
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
			const SmallArray<TestCounterClass, 4> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			SmallArray<TestCounterClass, 4> v2;
			v2.resize(100, dummy);
			//testAssert(v2.capacity() > 10);
			v2 = v;

			testAssert(v2.size() == 10);
			testAssert(ob_count == 21);
		}

		testAssert(ob_count == 1);
	}


	//========================= resize =========================

	// With int type, zero size
	{
		SmallArray<int, 4> v;
		v.resize(0);
		testAssert(v.size() == 0);
		testAssert(!v.storingOnHeap());
	}

	{
		SmallArray<int, 1> v;
		v.resize(1);
		testAssert(v.size() == 1);
		testAssert(!v.storingOnHeap());
	}

	{
		SmallArray<int, 4> v;
		v.resize(1);
		testAssert(v.size() == 1);
		testAssert(!v.storingOnHeap());

		v.resize(2);
		testAssert(v.size() == 2);
		testAssert(!v.storingOnHeap());

		v.resize(4);
		testAssert(v.size() == 4);
		testAssert(!v.storingOnHeap());

		v.resize(10);
		testAssert(v.size() == 10);
		testAssert(v.storingOnHeap());
	}

	// Resize over N, then back under N, then over N again.
	{
		SmallArray<int, 4> v;
		testAssert(!v.storingOnHeap());
		testAssert(v.size() == 0);

		v.resize(8);
		testAssert(v.size() == 8);
		testAssert(v.storingOnHeap());

		v.resize(4);
		testAssert(v.size() == 4);
		testAssert(!v.storingOnHeap());

		v.resize(12);
		testAssert(v.size() == 12);
		testAssert(v.storingOnHeap());

		v.resize(11);
		testAssert(v.size() == 11);
		testAssert(v.storingOnHeap());

		v.resize(0);
		testAssert(v.size() == 0);
		testAssert(!v.storingOnHeap());

		v.resize(1);
		testAssert(v.size() == 1);
		testAssert(!v.storingOnHeap());
	}

	// Resize over N, then back under N, then over N again.
	{
		int ob_count = 0;
		TestCounterClass counter(ob_count);
		testAssert(ob_count == 1);

		SmallArray<TestCounterClass, 4> v;
		testAssert(!v.storingOnHeap());
		testAssert(v.size() == 0);

		v.resize(8, counter);
		testAssert(v.size() == 8);
		testAssert(v.storingOnHeap());
		testAssert(ob_count == 9);

		v.resize(4, counter);
		testAssert(v.size() == 4);
		testAssert(ob_count == 5);
		testAssert(!v.storingOnHeap());

		v.resize(12, counter);
		testAssert(v.size() == 12);
		testAssert(ob_count == 13);
		testAssert(v.storingOnHeap());

		v.resize(11, counter);
		testAssert(v.size() == 11);
		testAssert(ob_count == 12);
		testAssert(v.storingOnHeap());

		v.resize(0, counter);
		testAssert(v.size() == 0);
		testAssert(ob_count == 1);
		testAssert(!v.storingOnHeap());

		v.resize(1, counter);
		testAssert(v.size() == 1);
		testAssert(ob_count == 2);
		testAssert(!v.storingOnHeap());
	}

	// With int type and val arg
	{
		SmallArray<int, 4> v;
		v.resize(1, 123);
		testAssert(v.size() == 1);
		testAssert(!v.storingOnHeap());

		for(size_t i=0; i<v.size(); ++i)
			testAssert(v[i] == 123);

		v.resize(2, 123);
		testAssert(v.size() == 2);
		testAssert(!v.storingOnHeap());

		for(size_t i=0; i<v.size(); ++i)
			testAssert(v[i] == 123);

		v.resize(4, 123);
		testAssert(v.size() == 4);
		testAssert(!v.storingOnHeap());

		for(size_t i=0; i<v.size(); ++i)
			testAssert(v[i] == 123);

		v.resize(10, 123);
		testAssert(v.size() == 10);
		testAssert(v.storingOnHeap());

		for(size_t i=0; i<v.size(); ++i)
			testAssert(v[i] == 123);
	}


	// With int type
	{
		SmallArray<int, 4> v(
			10, // count
			123 // val
		);

		testAssert(v.size() == 10);

		v.resize(100);

		testAssert(v.size() == 100);
	}


	// With TestCounterClass
	{
		int ob_count = 0;
		TestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			SmallArray<TestCounterClass, 4> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			// Resize a lot larger to cause capacity expansion
			v.resize(100, dummy);

			testAssert(v.size() == 100);
			//testAssert(v.capacity() >= 100);
			testAssert(ob_count == 101);

			// Resize just a little bit larger to avoid capacity expansion
			//v.reserve(101);
			v.resize(101, dummy);

			testAssert(v.size() == 101);
			//testAssert(v.capacity() >= 101);
			testAssert(ob_count == 102);

			v.resize(20, dummy);

			testAssert(v.size() == 20);
			//testAssert(v.capacity() >= 100);
			testAssert(ob_count == 21);

			// Try resizing to same size
			v.resize(20, dummy);

			testAssert(v.size() == 20);
			//testAssert(v.capacity() >= 100);
			testAssert(ob_count == 21);

			// Try resizing to zero.
			v.resize(0, dummy);

			testAssert(v.size() == 0);
			//testAssert(v.capacity() >= 100);
			testAssert(ob_count == 1);
		}

		testAssert(ob_count == 1);
	}

	// With std::string and no string val passed to resize:
	{
		SmallArray<std::string, 4> v(
			10
		);
		testAssert(v.size() == 10);

		// Resize a lot larger to cause capacity expansion
		v.resize(100);

		testAssert(v.size() == 100);

		// Resize just a little bit larger to avoid capacity expansion
		v.resize(101);

		testAssert(v.size() == 101);
		v.resize(20);

		testAssert(v.size() == 20);

		// Try resizing to same size
		v.resize(20);

		testAssert(v.size() == 20);

		// Try resizing to zero.
		v.resize(0);

		testAssert(v.size() == 0);
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
				SmallArray<TestCounterClass, 4> v(
					start_size, // count
					dummy
				);
				testAssert(v.size() == start_size);
				testAssert(ob_count == (int)start_size + 1);
			
				// Resize to end_size
				v.resize(end_size, dummy);

				testAssert(v.size() == end_size);
				//testAssert(v.capacity() >= 100);
				testAssert(ob_count == (int)end_size + 1);

			}

			testAssert(ob_count == 1);
		}
	}


	//========================= back =========================


	// With int type, lots of push backs
	{
		size_t N = 10;
		SmallArray<int, 4> v(N);
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
		SmallArray<int, 4> v(N);
		
		for(size_t i=0; i<N; ++i)
			v[i] = (int)i;

		for(size_t q=0; q<v.size(); ++q)
			testAssert(v[q] == (int)q);

		testAssert(v.size() == N);
		//testAssert(v.capacity() >= N);

		for(size_t i=0; i<N; ++i)
			testAssert(v[i] == (int)i);
	}
}


} // end namespace SmallArrayTest


#endif // BUILD_TESTS
