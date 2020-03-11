/*=====================================================================
AllocatorVectorUnitTests.cpp
----------------------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#include "AllocatorVectorUnitTests.h"


#include "AllocatorVector.h"
#include "StringUtils.h"
#include "Timer.h"
#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"
#include <vector>
#include <assert.h>


namespace glare
{


#if BUILD_TESTS


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


#ifdef COMPILER_MSVC
#define DO_ALIGNMENT(x) _CRT_ALIGN(x)
#else
#define DO_ALIGNMENT(x) __attribute__ ((aligned (x)))
#endif


struct DO_ALIGNMENT(16) align_16
{
	uint8 x[16];
};

struct DO_ALIGNMENT(32) align_32
{
	uint8 x[32];
};

struct DO_ALIGNMENT(64) align_64
{
	uint8 x[64];
};


class TestAllocator : public Allocator
{
public:
	virtual void* alloc(size_t size, size_t alignment)
	{
		(*i)++;
		return MemAlloc::alignedMalloc(size, alignment);
	}

	virtual void free(void* ptr)
	{
		if(ptr)
		{
			MemAlloc::alignedFree(ptr);
			(*i)--;
		}
	}

	int* i;
};



void AllocatorVectorUnitTests::test()
{
	conPrint("VectorUnitTests::test()");
	
	//========================= Test alignment =========================
	static_assert(GLARE_ALIGNMENT(align_16) == 16, "GLARE_ALIGNMENT(align_16) == 16");
	static_assert(GLARE_ALIGNMENT(align_32) == 32, "GLARE_ALIGNMENT(align_32) == 32");
	static_assert(GLARE_ALIGNMENT(align_64) == 64, "GLARE_ALIGNMENT(align_64) == 64");

	{
		glare::AllocatorVector<align_16, 16> v;
	}
	{
		// glare::AllocatorVector<align_32, 16> v; // should fail at compile time
	}
	{
		glare::AllocatorVector<align_16, 32> v;
	}
	{
		glare::AllocatorVector<align_16, 64> v;
	}

	{
		glare::AllocatorVector<align_32, 32> v;
	}
	{
		// glare::AllocatorVector<align_64, 32> v; // should fail at compile time
	}
	{
		glare::AllocatorVector<align_32, 64> v;
	}
	{
		glare::AllocatorVector<align_32, 128> v;
	}

	//========================= Test with allocator =========================
	{
		int i = 0;
		{
			glare::AllocatorVector<int, 4> v;

			Reference<TestAllocator> al = new TestAllocator();
			al->i = &i;
			v.setAllocator(al);
			v.resize(100);
		}
		testAssert(i == 0);
	}


	//========================= No-arg constructor =========================
	{
		// Test initialisation of vector of integers
		std::vector<int> stdvec(10);

		glare::AllocatorVector<int,4> jsvec(10);

	}

	//========================= No-arg constructor =========================

	// int type
	{
		const AllocatorVector<int, 4> v;
		testAssert(v.size() == 0);
	}


	//========================= Count constructor =========================

	// Count constructor with int type, with zero elems
	{
		const AllocatorVector<int, 4> v(
			0, // count
			123 // val
		);
		testAssert(v.size() == 0);
	}

	// Count constructor with int type, count > 0.
	{
		const AllocatorVector<int, 4> v(
			10, // count
			123 // val
		);
		testAssert(v.size() == 10);
		for(size_t i=0; i<v.size(); ++i)
			testAssert(v[i] == 123);
	}

	// Count constructor with TestClass
	{
		const AllocatorVector<TestClass, 4> v(
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
			const AllocatorVector<TestCounterClass, 16> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
		}

		testAssert(ob_count == 1);
	}


	//========================= Copy constructor =========================

	// With int type
	{
		const AllocatorVector<int, 4> v(
			10, // count
			123 // val
		);

		// Copy construct v2 from v
		const AllocatorVector<int, 4> v2(v);

		testAssert(v2.size() == 10);
		for(size_t i=0; i<v2.size(); ++i)
			testAssert(v2[i] == 123);
	}

	// With TestCounterClass
	{
		int ob_count = 0;
		TestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			const AllocatorVector<TestCounterClass, 16> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			// Copy construct v2 from v
			const AllocatorVector<TestCounterClass, 16> v2(v);

			testAssert(v2.size() == 10);
			testAssert(ob_count == 21);
		}

		testAssert(ob_count == 1);
	}



	//========================= operator = =========================

	// Test assigning to self
	{
		AllocatorVector<int, 4> v(
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
		const AllocatorVector<int, 4> v;
		AllocatorVector<int, 4> v2;
		v2 = v;

		testAssert(v.size() == 0);
		testAssert(v2.size() == 0);
	}


	// With int type
	{
		const AllocatorVector<int, 4> v(
			10, // count
			123 // val
		);

		AllocatorVector<int, 4> v2;
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
			const AllocatorVector<TestCounterClass, 16> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			AllocatorVector<TestCounterClass, 16> v2;
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
			const AllocatorVector<TestCounterClass, 16> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			AllocatorVector<TestCounterClass, 16> v2;
			v2.resize(100, dummy);
			testAssert(v2.capacity() > 10);
			v2 = v;

			testAssert(v2.size() == 10);
			testAssert(ob_count == 21);
		}

		testAssert(ob_count == 1);
	}


	//========================= reserve =========================

	
	// With int type
	{
		AllocatorVector<int, 4> v(
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
			AllocatorVector<TestCounterClass, 16> v(
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


	//========================= resize =========================

	// With int type, zero size
	{
		AllocatorVector<int, 4> v;
		v.resize(0);
		testAssert(v.size() == 0);
	}


	// With int type
	{
		AllocatorVector<int, 4> v(
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
			AllocatorVector<TestCounterClass, 16> v(
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
			v.reserve(101);
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
			testAssert(v.capacity() >= 100);
			testAssert(ob_count == 1);
		}

		testAssert(ob_count == 1);
	}



	// With int type, zero size
	{
		AllocatorVector<int, 4> v;
		v.push_back(1);
		v.push_back(2);
		v.push_back(3);
		v.resizeNoCopy(100);

		testAssert(v.size() == 100);
	}

	//========================= clear =========================


	// With int type, zero size
	{
		AllocatorVector<int, 4> v;
		v.clear();
		testAssert(v.size() == 0);
	}


	// With int type
	{
		AllocatorVector<int, 4> v(
			10, // count
			123 // val
		);

		v.clear();
		testAssert(v.size() == 0);
	}


	// With TestCounterClass
	{
		int ob_count = 0;
		TestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			AllocatorVector<TestCounterClass, 16> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			v.clear();

			testAssert(v.size() == 0);
		}

		testAssert(ob_count == 1);
	}


	//========================= clearAndFreeMem =========================


	// With int type, zero size
	{
		AllocatorVector<int, 4> v;
		v.clearAndFreeMem();
		testAssert(v.size() == 0);
	}


	// With int type
	{
		AllocatorVector<int, 4> v(
			10, // count
			123 // val
		);

		v.clearAndFreeMem();
		testAssert(v.size() == 0);
		testAssert(v.capacity() == 0);
	}


	// With TestCounterClass
	{
		int ob_count = 0;
		TestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			AllocatorVector<TestCounterClass, 16> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			v.clearAndFreeMem();

			testAssert(v.size() == 0);
			testAssert(v.capacity() == 0);
		}

		testAssert(ob_count == 1);
	}


	//========================= clearAndSetCapacity =========================

	// TODO

	//========================= push_back =========================

	// With int type
	{
		AllocatorVector<int, 4> v;
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



	// With TestCounterClass
	{
		int ob_count = 0;
		TestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			AllocatorVector<TestCounterClass, 16> v;

			v.push_back(dummy);

			testAssert(v.size() == 1);
			testAssert(ob_count == 2);

			v.push_back(TestCounterClass(ob_count));

			testAssert(v.size() == 2);
			testAssert(ob_count == 3);
		}

		testAssert(ob_count == 1);
	}

	//========================= pop_back =========================

	// With int type
	{
		AllocatorVector<int, 4> v;
		v.push_back(1);
		v.push_back(2);
		v.push_back(3);
		v.push_back(4);

		v.pop_back();
		testAssert(v.size() == 3);

		v.pop_back();
		testAssert(v.size() == 2);

		v.pop_back();
		testAssert(v.size() == 1);

		v.pop_back();
		testAssert(v.size() == 0);
	}



	// With TestCounterClass
	{
		int ob_count = 0;
		TestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			AllocatorVector<TestCounterClass, 16> v;
			v.push_back(dummy);
			v.push_back(dummy);
			v.push_back(dummy);
			v.push_back(dummy);

			v.pop_back();
			testAssert(v.size() == 3);
			testAssert(ob_count == 4);

			v.pop_back();
			testAssert(v.size() == 2);
			testAssert(ob_count == 3);

			v.pop_back();
			testAssert(v.size() == 1);
			testAssert(ob_count == 2);

			v.pop_back();
			testAssert(v.size() == 0);
			testAssert(ob_count == 1);
		}

		testAssert(ob_count == 1);
	}

	//========================= erase =========================

	// With int type
	{
		// Initialise vector to (1, 2, 3, 4)
		AllocatorVector<int, 4> v(4);
		for(int i = 0; i < 4; ++i)
			v[i] = i + 1;

		v.erase(3); // Erase from the back, result is (1, 2, 3)
		testAssert(v.size() == 3);
		testAssert(v.back() == 3);

		v.erase(1); // Erase from the middle, result is (1, 3)
		testAssert(v.size() == 2);
		testAssert(v[1] == 3);

		v.erase(0); // Erase from the beginning, result is (3)
		testAssert(v.size() == 1);
		testAssert(v[0] == 3);

		v.erase(0); // Erase from the beginning, result is ()
		testAssert(v.size() == 0);

		v.erase(0); // Erase from the beginning, does nothing (empty Vector erase is a null op)
		testAssert(v.size() == 0);
	}


	//========================= operator ==, operator != =========================

	{
		AllocatorVector<int, 4> v;
		testAssert(v == v);
		testAssert(!(v != v));
	}

	{
		AllocatorVector<int, 4> v;
		AllocatorVector<int, 4> v2;
		testAssert(v == v2);
		testAssert(!(v != v2));
	}

	{
		AllocatorVector<int, 4> v;
		v.push_back(1);

		AllocatorVector<int, 4> v2;

		testAssert(!(v == v2));
		testAssert(v != v2);
	}

	{
		AllocatorVector<int, 4> v;
		v.push_back(1);

		AllocatorVector<int, 4> v2;
		v2.push_back(100);

		testAssert(!(v == v2));
		testAssert(v != v2);
	}


	//========================= Old tests =========================


	// Test (count, val) constructor
	{
		
		const AllocatorVector<int, 4> v(4, 555);
		testAssert(v.size() == 4);

		// Test const_iterator
		for(AllocatorVector<int, 8>::const_iterator i = v.begin(); i != v.end(); ++i)
		{
			testAssert(*i == 555);
		}
	}

	// Test operator = 
	{
		AllocatorVector<int, 4> a;
		AllocatorVector<int, 4> b;
		a = b;
	}

	{
		AllocatorVector<int, 4> a;
		AllocatorVector<int, 4> b(2, 555);
		a = b;

		testAssert(b.size() == 2);
		testAssert(b[0] == 555);
		testAssert(b[1] == 555);
	}



	{
		AllocatorVector<int, 4> v;
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


} //end namespace glare
