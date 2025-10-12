/*=====================================================================
SharedImmutableString.cpp
-------------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "SharedImmutableString.h"


#include "IncludeXXHash.h"
#include "MemAlloc.h"
#include <cstring>


namespace glare
{


Reference<SharedImmutableStringData> makeSharedImmutableStringData(const char* data, size_t len)
{
	assert(len > 0);

	void* str_ptr = MemAlloc::alignedMalloc(sizeof(SharedImmutableStringData) + len, 16);
	if(data != nullptr)
	{
		if(len > 0)
			std::memcpy((uint8*)str_ptr + sizeof(SharedImmutableStringData), data, len);
	} 
	else
	{
		std::memset((uint8*)str_ptr + sizeof(SharedImmutableStringData), 0, len);
	}
	SharedImmutableStringData* str = new(str_ptr) SharedImmutableStringData(); // Placement new
	str->size_ = len;
	str->hash = data ? XXH64(data, len, /*seed=*/1) : 0;
	return str;
}


void doDestroySharedImmutableStringData(SharedImmutableStringData* str)
{
	MemAlloc::alignedFree(str);
}


} // end namespace glare



#if BUILD_TESTS


#include "ConPrint.h"
#include "TestUtils.h"
#include <unordered_set>



void glare::SharedImmutableString::test()
{
	{
		SharedImmutableString s = makeSharedImmutableString("hello");
	}
	{
		SharedImmutableString s = makeSharedImmutableString("");
	}
	{
		SharedImmutableString s = makeEmptySharedImmutableString();
	}

	//--------------------------- Test empty string (handled with null string ref)  ---------------------------
	{
		SharedImmutableString a = makeSharedImmutableString("");
		testAssert(a.string.isNull());

		SharedImmutableString a2 = makeSharedImmutableString(nullptr, 0);
		testAssert(a2.string.isNull());

		SharedImmutableString b = makeEmptySharedImmutableString();
		testAssert(b.string.isNull());
	}

	//--------------------------- Test operator == ---------------------------
	{
		SharedImmutableString a = makeSharedImmutableString("hello");
		SharedImmutableString b = makeSharedImmutableString("hello");
		SharedImmutableString c = makeSharedImmutableString("world");
		SharedImmutableString d = makeSharedImmutableString("");
		testAssert(a == b);
		testAssert(!(a == c));

		// Test empty string
		testAssert(!(a == d));
		testAssert(d == d);
	}

	//--------------------------- Test operator != ---------------------------
	{
		SharedImmutableString a = makeSharedImmutableString("hello");
		SharedImmutableString b = makeSharedImmutableString("hello");
		SharedImmutableString c = makeSharedImmutableString("world");
		SharedImmutableString d = makeSharedImmutableString("");
		testAssert(!(a != b));
		testAssert(a != c);

		// Test empty string
		testAssert(a != d);
		testAssert(!(d != d));
	}

	//--------------------------- Test storing in unordered_set ---------------------------

	// Test with pointer-equal key comparator.
	{
		std::unordered_set<SharedImmutableString, SharedImmutableStringHasher, SharedImmutableStringKeyPointerEqual> set;

		SharedImmutableString s = makeSharedImmutableString("hello");
		set.insert(s);

		SharedImmutableString s2 = makeSharedImmutableString("hello");
		set.insert(s2);

		testAssert(set.size() == 2);
	}

	// Test empty string with pointer-equal key comparator.
	{
		std::unordered_set<SharedImmutableString, SharedImmutableStringHasher, SharedImmutableStringKeyPointerEqual> set;

		SharedImmutableString s = makeSharedImmutableString("");
		set.insert(s);

		SharedImmutableString s2 = makeSharedImmutableString("");
		set.insert(s2);

		testAssert(set.size() == 1); // Even with pointer equality, null pointers mean s and s2 compare equal.
	}

	// Test with value-equal key comparator.
	{
		std::unordered_set<SharedImmutableString, SharedImmutableStringHasher, SharedImmutableStringKeyValueEqual> set;

		SharedImmutableString s = makeSharedImmutableString("hello");
		set.insert(s);

		SharedImmutableString s2 = makeSharedImmutableString("hello");
		set.insert(s2);

		testAssert(set.size() == 1);
	}

	// Test empty string with value-equal key comparator.
	{
		std::unordered_set<SharedImmutableString, SharedImmutableStringHasher, SharedImmutableStringKeyValueEqual> set;

		SharedImmutableString s = makeSharedImmutableString("");
		set.insert(s);

		SharedImmutableString s2 = makeSharedImmutableString("");
		set.insert(s2);

		testAssert(set.size() == 1);
	}

	conPrint("glare::SharedImmutableString::test() done.");
}


#endif
