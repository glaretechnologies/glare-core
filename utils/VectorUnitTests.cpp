/*=====================================================================
VectorUnitTests.cpp
-------------------
File created by ClassTemplate on Sat Sep 02 20:18:26 2006
Code By Nicholas Chapman.
=====================================================================*/
#include "VectorUnitTests.h"


#include "Vector.h"
#include "StringUtils.h"
#include "Timer.h"
#include "../utils/TestUtils.h"
#include "../indigo/globals.h"
#include <vector>
#include <assert.h>


namespace js
{


VectorUnitTests::VectorUnitTests()
{
}


VectorUnitTests::~VectorUnitTests()
{
}


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


void VectorUnitTests::test()
{
	conPrint("VectorUnitTests::test()");
	
	//========================= Test alignment =========================
	static_assert(GLARE_ALIGNMENT(align_16) == 16, "GLARE_ALIGNMENT(align_16) == 16");
	static_assert(GLARE_ALIGNMENT(align_32) == 32, "GLARE_ALIGNMENT(align_32) == 32");
	static_assert(GLARE_ALIGNMENT(align_64) == 64, "GLARE_ALIGNMENT(align_64) == 64");

	{
		js::Vector<align_16, 16> v;
	}
	{
		// js::Vector<align_32, 16> v; // should fail at compile time
	}
	{
		js::Vector<align_16, 32> v;
	}
	{
		js::Vector<align_16, 64> v;
	}

	{
		js::Vector<align_32, 32> v;
	}
	{
		// js::Vector<align_64, 32> v; // should fail at compile time
	}
	{
		js::Vector<align_32, 64> v;
	}
	{
		js::Vector<align_32, 128> v;
	}

	//========================= No-arg constructor =========================
	{
		// Test initialisation of vector of integers
		std::vector<int> stdvec(10);

		js::Vector<int,4> jsvec(10);

	}

	//========================= No-arg constructor =========================

	// int type
	{
		const Vector<int, 4> v;
		testAssert(v.size() == 0);
	}


	//========================= Count constructor =========================

	// Count constructor with int type, with zero elems
	{
		const Vector<int, 4> v(
			0, // count
			123 // val
		);
		testAssert(v.size() == 0);
	}

	// Count constructor with int type, count > 0.
	{
		const Vector<int, 4> v(
			10, // count
			123 // val
		);
		testAssert(v.size() == 10);
		for(size_t i=0; i<v.size(); ++i)
			testAssert(v[i] == 123);
	}

	// Count constructor with TestClass
	{
		const Vector<TestClass, 4> v(
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
			const Vector<TestCounterClass, 16> v(
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
		const Vector<int, 4> v(
			10, // count
			123 // val
		);

		// Copy construct v2 from v
		const Vector<int, 4> v2(v);

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
			const Vector<TestCounterClass, 16> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			// Copy construct v2 from v
			const Vector<TestCounterClass, 16> v2(v);

			testAssert(v2.size() == 10);
			testAssert(ob_count == 21);
		}

		testAssert(ob_count == 1);
	}



	//========================= operator = =========================

	// Test assigning to self
	{
		Vector<int, 4> v(
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
		const Vector<int, 4> v;
		Vector<int, 4> v2;
		v2 = v;

		testAssert(v.size() == 0);
		testAssert(v2.size() == 0);
	}


	// With int type
	{
		const Vector<int, 4> v(
			10, // count
			123 // val
		);

		Vector<int, 4> v2;
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
			const Vector<TestCounterClass, 16> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			Vector<TestCounterClass, 16> v2;
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
			const Vector<TestCounterClass, 16> v(
				10, // count
				dummy
			);
			testAssert(v.size() == 10);
			testAssert(ob_count == 11);
			
			Vector<TestCounterClass, 16> v2;
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
		Vector<int, 4> v(
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
			Vector<TestCounterClass, 16> v(
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
		Vector<int, 4> v;
		v.resize(0);
		testAssert(v.size() == 0);
	}


	// With int type
	{
		Vector<int, 4> v(
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
			Vector<TestCounterClass, 16> v(
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
		Vector<int, 4> v;
		v.push_back(1);
		v.push_back(2);
		v.push_back(3);
		v.resizeNoCopy(100);

		testAssert(v.size() == 100);
	}


	// Test performance of resize() vs resizeNoCopy().
	// Neither resize() nor resizeNoCopy() should zero out the integer data.
	// std::vector will however, so should be significantly slower.

	// Warmup
	{
		Timer timer;
		Vector<int, 4> v;
		v.resize(1000000);
		testAssert(v.size() == 1000000);
	}

	const int N = 1000000;
	conPrint("\nResizing from zero size to size=" + toString(1000000) + ":\n");
	{
		Timer timer;
		Vector<int, 4> v;
		v.resizeNoCopy(N);
		testAssert(v.size() == N);
		conPrint("\tjs::Vector resizeNoCopy(): " + timer.elapsedString());
	}
	{
		Timer timer;
		Vector<int, 4> v;
		v.resize(N);
		testAssert(v.size() == N);
		conPrint("\tjs::Vector resize():       " + timer.elapsedString());
	}
	{
		Timer timer;
		std::vector<int> v;
		v.resize(N);
		testAssert(v.size() == N);
		conPrint("\tstd::vector resize():      " + timer.elapsedString());
	}


	// Test resize performance vs resizeNoCopy
	conPrint("\nResizing from zero size to size=" + toString(1000000) + " in powers of 10:\n");
	{
		Timer timer;
		Vector<int, 4> v;
		for(int i=1; i<=1000000; i*=10)
			v.resizeNoCopy(i);
		testAssert(v.size() == 1000000);
		conPrint("\tjs::Vector resizeNoCopy(): " + timer.elapsedString());
	}
	{
		Timer timer;
		Vector<int, 4> v;
		for(int i=1; i<=1000000; i*=10)
			v.resize(i);
		testAssert(v.size() == 1000000);
		conPrint("\tjs::Vector resize():       " + timer.elapsedString());
	}
	{
		Timer timer;
		std::vector<int> v;
		for(int i=1; i<=1000000; i*=10)
			v.resize(i);
		testAssert(v.size() == 1000000);
		conPrint("\tstd::vector resize():      " + timer.elapsedString());
	}


	//========================= clear =========================


	// With int type, zero size
	{
		Vector<int, 4> v;
		v.clear();
		testAssert(v.size() == 0);
	}


	// With int type
	{
		Vector<int, 4> v(
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
			Vector<TestCounterClass, 16> v(
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
		Vector<int, 4> v;
		v.clearAndFreeMem();
		testAssert(v.size() == 0);
	}


	// With int type
	{
		Vector<int, 4> v(
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
			Vector<TestCounterClass, 16> v(
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
		Vector<int, 4> v;
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
			Vector<TestCounterClass, 16> v;

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
		Vector<int, 4> v;
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
			Vector<TestCounterClass, 16> v;
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
		Vector<int, 4> v(4);
		for(int i = 0; i < 4; ++i)
			v[i] = i + 1;

		v.erase(v.begin() + 3); // Erase from the back, result is (1, 2, 3)
		testAssert(v.size() == 3);
		testAssert(v.back() == 3);

		v.erase(v.begin() + 1); // Erase from the middle, result is (1, 3)
		testAssert(v.size() == 2);
		testAssert(v[1] == 3);

		v.erase(v.begin()); // Erase from the beginning, result is (3)
		testAssert(v.size() == 1);
		testAssert(v[0] == 3);

		v.erase(v.begin()); // Erase from the beginning, result is ()
		testAssert(v.size() == 0);

		v.erase(v.begin()); // Erase from the beginning, does nothing (empty Vector erase is a null op)
		testAssert(v.size() == 0);
	}



	// With TestCounterClass
	//{
	//	int ob_count = 0;
	//	TestCounterClass dummy(ob_count);
	//	testAssert(ob_count == 1);

	//	{
	//		Vector<TestCounterClass, 4> v;
	//		v.push_back(dummy);
	//		v.push_back(dummy);
	//		v.push_back(dummy);
	//		v.push_back(dummy);

	//		v.erase(3); // Erase from the back
	//		testAssert(v.size() == 3);
	//		testAssert(ob_count == 4);

	//		v.erase(1); // Erase from the middle
	//		testAssert(v.size() == 2);
	//		testAssert(ob_count == 3);

	//		v.erase(0); // Erase from the beginning
	//		testAssert(v.size() == 1);
	//		testAssert(ob_count == 2);

	//		v.erase(0); // Erase from the beginning
	//		testAssert(v.size() == 0);
	//		testAssert(ob_count == 1);
	//	}

	//	testAssert(ob_count == 1);
	//}

	//========================= operator ==, operator != =========================

	{
		Vector<int, 4> v;
		testAssert(v == v);
		testAssert(!(v != v));
	}

	{
		Vector<int, 4> v;
		Vector<int, 4> v2;
		testAssert(v == v2);
		testAssert(!(v != v2));
	}

	{
		Vector<int, 4> v;
		v.push_back(1);

		Vector<int, 4> v2;

		testAssert(!(v == v2));
		testAssert(v != v2);
	}

	{
		Vector<int, 4> v;
		v.push_back(1);

		Vector<int, 4> v2;
		v2.push_back(100);

		testAssert(!(v == v2));
		testAssert(v != v2);
	}

	//========================= swapWith =========================
	{
		Vector<int> v, v2;
		v.push_back(123);

		v2.swapWith(v);

		testAssert(v.empty());
		testAssert(v2.size() == 1);
		testAssert(v2[0] == 123);
	}

	{
		Vector<int> v, v2;
		v.push_back(123);

		v2.push_back(1);
		v2.push_back(2);
		v2.push_back(3);

		v.swapWith(v2);

		testAssert(v.size() == 3);
		testAssert(v[0] == 1);
		testAssert(v[1] == 2);
		testAssert(v[2] == 3);

		testAssert(v2.size() == 1);
		testAssert(v2[0] == 123);
	}

	// With TestCounterClass
	{
		int ob_count = 0;
		TestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			Vector<TestCounterClass, 16> v(
				10, // count
				dummy
			);

			Vector<TestCounterClass, 16> v2(
				100, // count
				dummy
			);
			
			testAssert(ob_count == 111);

			v.swapWith(v2);

			testAssert(ob_count == 111);
		}
	}

	//========================= takeForm =========================
	{
		Vector<int> v, v2;
		v.push_back(123);

		v2.takeFrom(v);

		testAssert(v.empty());
		testAssert(v2.size() == 1);
		testAssert(v2[0] == 123);
	}

	{
		Vector<int> v, v2;
		v.push_back(123);

		v2.push_back(1);
		v2.push_back(2);
		v2.push_back(3);

		v.takeFrom(v2);

		testAssert(v.size() == 3);
		testAssert(v[0] == 1);
		testAssert(v[1] == 2);
		testAssert(v[2] == 3);

		testAssert(v2.empty());
	}

	
	// With TestCounterClass
	{
		int ob_count = 0;
		TestCounterClass dummy(ob_count);
		testAssert(ob_count == 1);

		{
			Vector<TestCounterClass, 16> v(
				10, // count
				dummy
			);

			Vector<TestCounterClass, 16> v2(
				100, // count
				dummy
			);
			
			testAssert(ob_count == 111);

			v.takeFrom(v2);

			testAssert(ob_count == 101);
		}
	}



	//========================= Old tests =========================


	// Test (count, val) constructor
	{
		
		const Vector<int, 4> v(4, 555);
		testAssert(v.size() == 4);

		// Test const_iterator
		for(Vector<int, 8>::const_iterator i = v.begin(); i != v.end(); ++i)
		{
			testAssert(*i == 555);
		}
	}

	// Test operator = 
	{
		Vector<int, 4> a;
		Vector<int, 4> b;
		a = b;
	}

	{
		Vector<int, 4> a;
		Vector<int, 4> b(2, 555);
		a = b;

		testAssert(b.size() == 2);
		testAssert(b[0] == 555);
		testAssert(b[1] == 555);
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

	//========================= Performance test vs std::vector =========================
#if 0 // Commented out due to annoying GCC requiring operator = for TestCounterClass with std::vector
	{

		//========================= construction of vector of ints =========================
		{
		conPrint("construction of vector of ints");

		const size_t n = 10000000;
		{
			Timer timer;
			
			js::Vector<int, 4> v(n);
			
			conPrint("js::Vector:  " + timer.elapsedStringNPlaces(5));
			printVar(v.size());
		}

		{
			Timer timer;
			
			std::vector<int> v(n);

			conPrint("std::vector: " + timer.elapsedStringNPlaces(5));
			printVar(v.size());
		}

		conPrint("");
		}


		//========================= push_back() =========================
		conPrint("int push_back()");

		const size_t n = 10000000;
		{
			js::Vector<int, 4> v;
			Timer timer;
			for(size_t i=0; i<n; ++i)
				v.push_back((int)i);

			conPrint("js::Vector:  " + timer.elapsedStringNPlaces(5));
			printVar(v.size());
		}

		{
			std::vector<int> v;
			Timer timer;
			for(size_t i=0; i<n; ++i)
				v.push_back((int)i);

			conPrint("std::vector: " + timer.elapsedStringNPlaces(5));
			printVar(v.size());
		}

		conPrint("");

		//========================= push_back() =========================
		conPrint("TestCounterClass push_back()");

		{
			int count = 0;
			TestCounterClass dummy(count);

			js::Vector<TestCounterClass, 4> v;
			Timer timer;
			for(size_t i=0; i<n; ++i)
				v.push_back(dummy);

			conPrint("js::Vector:  " + timer.elapsedStringNPlaces(5));
			printVar(v.size());
		}

		{
			int count = 0;
			TestCounterClass dummy(count);

			std::vector<TestCounterClass> v;
			Timer timer;
			for(size_t i=0; i<n; ++i)
				v.push_back(dummy);

			conPrint("std::vector: " + timer.elapsedStringNPlaces(5));
			printVar(v.size());
		}

		conPrint("");

		//========================= push_back() =========================
		conPrint("TestCounterClass push_back() with literal construction");

		{
			int count = 0;
			TestCounterClass dummy(count);

			js::Vector<TestCounterClass, 4> v;
			Timer timer;
			for(size_t i=0; i<n; ++i)
				v.push_back(TestCounterClass(count));

			conPrint("js::Vector:  " + timer.elapsedStringNPlaces(5));
			printVar(v.size());
		}

		{
			int count = 0;
			TestCounterClass dummy(count);

			std::vector<TestCounterClass> v;
			Timer timer;
			for(size_t i=0; i<n; ++i)
				v.push_back(TestCounterClass(count));

			conPrint("std::vector: " + timer.elapsedStringNPlaces(5));
			printVar(v.size());
		}

		conPrint("");
	}
#endif

}


#endif


} //end namespace js
