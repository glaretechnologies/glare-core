/*=====================================================================
GlareString.cpp
---------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "GlareString.h"


#include "Exception.h"


// Put this function in the .cpp file so we can avoid including Exception.h in GlareString.h, which is slow as it includes <string>.
char* glare::String::allocOnHeapForSize(size_t n)
{
	if(n > MAX_CAPACITY)
		throw glare::Exception("Requested capacity exceeded max capacity");

	return static_cast<char*>(MemAlloc::alignedSSEMalloc(n + 1)); // Allocate new memory on heap, + 1 for null terminator
}


#if BUILD_TESTS


#include "StringUtils.h"
#include "Timer.h"
#include "TestUtils.h"
#include "ConPrint.h"
#include "TestExceptionUtils.h"


void glare::String::test()
{
	conPrint("glare::String::test()");

	//--------------------- Check we are compiling for a little-endian system, which glare::String assumes. ---------------------
	const uint32 someuint32 = 0xAABBCCDD;
	testAssert(((uint8*)(&someuint32))[0] == 0xDD);

	//--------------------- Test String() ---------------------
	{
		glare::String s;
		testAssert(s.size() == 0);
		testAssert(s.capacity() == 14);
		testAssert(s.storingOnHeap() == false);
		testAssert(s.c_str()[0] == '\0');
	}

	//--------------------- Test String(size_t count, char val = 0) ---------------------
	{
		glare::String s(0, 'a');
		testAssert(s.size() == 0);
		testAssert(s.capacity() == 14);
		testAssert(s.storingOnHeap() == false);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == 'a');
		testAssert(s.c_str()[s.size()] == '\0');
	}

	{
		glare::String s(10, 'a');
		testAssert(s.size() == 10);
		testAssert(s.capacity() == 14);
		testAssert(s.storingOnHeap() == false);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == 'a');
		testAssert(s.c_str()[s.size()] == '\0');
	}

	// Test just below on-heap threshold
	{
		glare::String s(14, 'a');
		testAssert(s.storingOnHeap() == false);
		testAssert(s.size() == 14);
		testAssert(s.capacity() == 14);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == 'a');
		testAssert(s.c_str()[s.size()] == '\0');
	}

	// Test just above on-heap threshold
	{
		glare::String s(15, 'a');
		testAssert(s.storingOnHeap() == true);
		testAssert(s.size() == 15);
		testAssert(s.capacity() == 15);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == 'a');
		testAssert(s.c_str()[s.size()] == '\0');
	}

	{
		glare::String s(100, 'a');
		testAssert(s.size() == 100);
		testAssert(s.capacity() == 100);
		testAssert(s.storingOnHeap() == true);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == 'a');
		testAssert(s.c_str()[s.size()] == '\0');
	}

	//--------------------- String(const char* str) ---------------------
	{
		glare::String s("0123456789");
		testAssert(s.size() == 10);
		testAssert(s.capacity() == 14);
		testAssert(s.storingOnHeap() == false);
		testAssert(s.c_str()[s.size()] == '\0');
	}
	{
		glare::String s("01234567890123456789");
		testAssert(s.size() == 20);
		testAssert(s.capacity() == 20);
		testAssert(s.storingOnHeap() == true);
		testAssert(s.c_str()[s.size()] == '\0');
	}

	//--------------------- String(const String& other) ---------------------
	{
		glare::String s1("0123456789");
		glare::String s(s1);
		testAssert(s.size() == 10);
		testAssert(s.capacity() == 14);
		testAssert(s.storingOnHeap() == false);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == '0' + i);
		testAssert(s.c_str()[s.size()] == '\0');
	}
	{
		glare::String s1("01234567890123456789");
		glare::String s(s1);
		testAssert(s.size() == 20);
		testAssert(s.capacity() == 20);
		testAssert(s.storingOnHeap() == true);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == '0' + (i % 10));
		testAssert(s.c_str()[s.size()] == '\0');
	}

	//--------------------- operator= ---------------------
	// Test existing string being directly stored, and assigned a directly stored string
	{
		glare::String s1("0123456789");
		glare::String s("abc");
		s = s1;
		testAssert(s.size() == 10);
		testAssert(s.capacity() == 14);
		testAssert(s.storingOnHeap() == false);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == '0' + i);
		testAssert(s.c_str()[s.size()] == '\0');
	}
	// Test existing string being directly stored, and assigned a heap string
	{
		glare::String s1("01234567890123456789");
		glare::String s("abc");
		s = s1;
		testAssert(s.size() == 20);
		testAssert(s.capacity() == 20);
		testAssert(s.storingOnHeap() == true);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == '0' + (i % 10));
		testAssert(s.c_str()[s.size()] == '\0');
	}

	// Test existing string being heap stored, and assigned a directly stored string
	{
		glare::String s1("0123456789");
		glare::String s("aaaaaaaaaaaaaaaaaaaa");
		s = s1;
		testAssert(s.size() == 10);
		testAssert(s.capacity() == 14);
		testAssert(s.storingOnHeap() == false);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == '0' + i);
		testAssert(s.c_str()[s.size()] == '\0');
	}
	// Test existing string being heap, and assigned a larger heap string
	{
		glare::String s1("01234567890123456789");
		glare::String s( "aaaaaaaaaaaaaaaaaa");
		s = s1;
		testAssert(s.size() == 20);
		testAssert(s.capacity() == 20);
		testAssert(s.storingOnHeap() == true);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == '0' + (i % 10));
		testAssert(s.c_str()[s.size()] == '\0');
	}
	// Test existing string being heap, and assigned a smaller heap string
	{
		glare::String s1("01234567890123456789");
		glare::String s( "aaaaaaaaaaaaaaaaaaaa");
		s = s1;
		testAssert(s.size() == 20);
		testAssert(s.capacity() == 20);
		testAssert(s.storingOnHeap() == true);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == '0' + (i % 10));
		testAssert(s.c_str()[s.size()] == '\0');
	}

	//--------------------- reserve ---------------------
	// Test reserving less than enough to need heap storage
	{
		glare::String s("012");

		s.reserve(10);

		testAssert(s.size() == 3);
		testAssert(s.capacity() == 14);
		testAssert(s.storingOnHeap() == false);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == '0' + i);
		testAssert(s.c_str()[s.size()] == '\0');
	}

	// Test reserving enough to need heap storage
	{
		glare::String s("012");

		s.reserve(1000);

		testAssert(s.size() == 3);
		testAssert(s.capacity() == 1000);
		testAssert(s.storingOnHeap() == true);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == '0' + i);
		testAssert(s.c_str()[s.size()] == '\0');
	}

	// Test trying to reserve too large a buffer
	{
		glare::String s;
		testExceptionExpected([&]() { s.reserve(1u << 31); });
	}
	{
		glare::String s;
		s.reserve(1024);
		testExceptionExpected([&]() { s.reserve(1u << 31); });
	}

	//--------------------- reserveNoCopy ---------------------
	// Test reserving less than enough to need heap storage
	{
		glare::String s("012");

		s.reserveNoCopy(10);

		testAssert(s.size() == 3);
		testAssert(s.capacity() == 14);
		testAssert(s.storingOnHeap() == false);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == '0' + i);
		testAssert(s.c_str()[s.size()] == '\0');
	}

	// Test reserving enough to need heap storage
	{
		glare::String s("012");

		s.reserveNoCopy(1000);

		testAssert(s.size() == 3);
		testAssert(s.capacity() == 1000);
		testAssert(s.storingOnHeap() == true);

		// Data will be uninitialised now

		//testAssert(s.c_str()[s.size()] == '\0');
	}

	//--------------------- resize ---------------------
	// Test resizing to less than enough to need heap storage, from direct
	{
		glare::String s("012");

		s.resize(6, 'a');

		testAssert(s.size() == 6);
		testAssert(s.capacity() == 14);
		testAssert(s.storingOnHeap() == false);
		testAssert(s == "012aaa");
		testAssert(s.c_str()[s.size()] == '\0');
	}

	// Test reserving enough to need heap storage, from direct
	{
		glare::String s("012");

		s.resize(1000, 'a');

		testAssert(s.size() == 1000);
		testAssert(s.capacity() == 1000);
		testAssert(s.storingOnHeap() == true);
		for(size_t i=3; i<s.size(); ++i)
			testAssert(s[i] == 'a');
		testAssert(s.c_str()[s.size()] == '\0');
	}

	// Test resizing to less than enough to need heap storage, from heap
	{
		glare::String s("01234567890123456789");

		s.resize(6, 'a');

		testAssert(s.size() == 6);
		testAssert(s.capacity() == 20);
		testAssert(s.storingOnHeap() == true); // For now we remain using heap storage.
		testAssert(s == "012345");
		testAssert(s.c_str()[s.size()] == '\0');
	}

	// Test reserving enough to need heap storage, from heap
	{
		glare::String s("01234567890123456789");

		s.resize(1000, 'a');

		testAssert(s.size() == 1000);
		testAssert(s.capacity() == 1000);
		testAssert(s.storingOnHeap() == true);
		for(size_t i=20; i<s.size(); ++i)
			testAssert(s[i] == 'a');
		testAssert(s.c_str()[s.size()] == '\0');
	}

	// Test trying to resize to too large a buffer
	{
		glare::String s;
		testExceptionExpected([&]() { s.resize(1u << 31); });
	}
	{
		glare::String s;
		s.resize(1024);
		testExceptionExpected([&]() { s.resize(1u << 31); });
	}

	//--------------------- resizeNoCopy ---------------------
	// Test resizing to less than enough to need heap storage, from direct
	{
		glare::String s("012");

		s.resizeNoCopy(6);

		testAssert(s.size() == 6);
		testAssert(s.capacity() == 14);
		testAssert(s.storingOnHeap() == false);
		testAssert(s[0] == '0');
		testAssert(s[1] == '1');
		testAssert(s[2] == '2');
		// s[3], s[4], s[5] are uninitialised.
		testAssert(s.c_str()[s.size()] == '\0');
	}

	// Test reserving enough to need heap storage, from direct
	{
		glare::String s("012");

		s.resizeNoCopy(1000);

		testAssert(s.size() == 1000);
		testAssert(s.capacity() == 1000);
		testAssert(s.storingOnHeap() == true);

		// All data is uninitialised now, apart from null terminator.

		testAssert(s.c_str()[s.size()] == '\0');
	}

	// Test resizing to less than enough to need heap storage, from heap
	{
		glare::String s("01234567890123456789");

		s.resizeNoCopy(6);

		testAssert(s.size() == 6);
		testAssert(s.capacity() == 20);
		testAssert(s.storingOnHeap() == true); // For now we remain using heap storage.
		testAssert(s == "012345");
		testAssert(s.c_str()[s.size()] == '\0');
	}

	// Test reserving enough to need heap storage, from heap
	{
		glare::String s("01234567890123456789");

		s.resizeNoCopy(1000);

		testAssert(s.size() == 1000);
		testAssert(s.capacity() == 1000);
		testAssert(s.storingOnHeap() == true);
		
		// All data is uninitialised now, apart from null terminator.

		testAssert(s.c_str()[s.size()] == '\0');
	}

	//--------------------- push_back ---------------------
	{
		glare::String s;
		s.push_back('a');
		testAssert(s.size() == 1);
		testAssert(s.capacity() == 14);
		testAssert(s.storingOnHeap() == false);
		testAssert(s == "a");
		testAssert(s.c_str()[s.size()] == '\0');

		s.push_back('b');
		testAssert(s.size() == 2);
		testAssert(s.capacity() == 14);
		testAssert(s.storingOnHeap() == false);
		testAssert(s == "ab");
		testAssert(s.c_str()[s.size()] == '\0');
	}

	// Test push_back triggering allocation on heap
	{
		glare::String s(14, 'a');
		testAssert(s.size() == 14);
		testAssert(s.capacity() == 14);
		testAssert(s.storingOnHeap() == false);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == 'a');
		testAssert(s.c_str()[s.size()] == '\0');

		s.push_back('a');

		testAssert(s.size() == 15);
		testAssert(s.capacity() >= 15);
		testAssert(s.storingOnHeap() == true);
		for(size_t i=0; i<s.size(); ++i)
			testAssert(s[i] == 'a');
		testAssert(s.c_str()[s.size()] == '\0');
	}

	//--------------------- pop_back ---------------------
	{
		glare::String s("a");
		s.pop_back();
		testAssert(s.size() == 0);
		testAssert(s.capacity() == 14);
		testAssert(s.storingOnHeap() == false);
		testAssert(s == "");
		testAssert(s.c_str()[s.size()] == '\0');
	}

	{
		glare::String s("01234567890123456789");
		s.pop_back();
		testAssert(s.size() == 19);
		testAssert(s.capacity() == 20);
		testAssert(s.storingOnHeap() == true);
		testAssert(s == "0123456789012345678");
		testAssert(s.c_str()[s.size()] == '\0');
	}

	//--------------------- operator == (const String&) ---------------------
	{
		glare::String s("0123456789");
		testAssert(s == glare::String("0123456789"));
		testAssert(!(s == glare::String("0123456789aaa")));
		testAssert(!(s == glare::String("0123456")));
		testAssert(!(s == glare::String("")));

		testAssert(glare::String("") == glare::String(""));
		testAssert(!(glare::String("") == glare::String("a")));
		testAssert(!(glare::String("a") == glare::String("")));
	}

	//--------------------- operator != (const String&) ---------------------
	{
		glare::String s("0123456789");
		testAssert(!(s != glare::String("0123456789")));
		testAssert(s != glare::String("0123456789aaa"));
		testAssert(s != glare::String("0123456"));
		testAssert(s != glare::String(""));

		testAssert(!(glare::String("") != glare::String("")));
		testAssert(glare::String("") != glare::String("a"));
		testAssert(glare::String("a") != glare::String(""));
	}

	//--------------------- operator == (const char*) ---------------------
	{
		glare::String s("0123456789");
		testAssert(s == "0123456789");
		testAssert(!(s == "0123456789aaa"));
		testAssert(!(s == "0123456"));
		testAssert(!(s == ""));

		testAssert(glare::String("") == "");
		testAssert(!(glare::String("") == "a"));
		testAssert(!(glare::String("a") == ""));
	}

	//--------------------- operator != (const char*) ---------------------
	{
		glare::String s("0123456789");
		testAssert(!(s != "0123456789"));
		testAssert(s != "0123456789aaa");
		testAssert(s != "0123456");
		testAssert(s != "");

		testAssert(!(glare::String("") != ""));
		testAssert(glare::String("") != "a");
		testAssert(glare::String("a") != "");
	}

	conPrint("glare::String::test() done.");
}


#endif // BUILD_TESTS
