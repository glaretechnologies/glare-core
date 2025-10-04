/*=====================================================================
SharedImmutableString.cpp
-------------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "SharedImmutableString.h"


#include "IncludeXXHash.h"
#include "MemAlloc.h"


namespace glare
{


Reference<SharedImmutableString> makeSharedImmutableString(const char* data, size_t len)
{
	void* str_ptr = MemAlloc::alignedMalloc(sizeof(SharedImmutableString) + len, 16);
	if(data != nullptr)
	{
		if(len > 0)
			std::memcpy((uint8*)str_ptr + sizeof(SharedImmutableString), data, len);
	} 
	else
	{
		std::memset((uint8*)str_ptr + sizeof(SharedImmutableString), 0, len);
	}
	//SharedImmutableString* str = (SharedImmutableString*)str_ptr;
	SharedImmutableString* str = new(str_ptr) SharedImmutableString(); // Placement new
	str->size_ = len;
	str->hash = data ? XXH64(data, len, /*seed=*/1) : 0;
	//SharedImmutableStringHandle handle;
	//handle.string = str;
	//return handle;
	return str;
}


Reference<SharedImmutableString> makeSharedImmutableString(string_view str)
{
	return makeSharedImmutableString(str.data(), str.size());
}


void doDestroySharedImmutableString(SharedImmutableString* str)
{
	MemAlloc::alignedFree(str);
}



SharedImmutableStringHandle makeSharedImmutableStringHandle(const char* data, size_t len)
{
	SharedImmutableStringHandle handle(makeSharedImmutableString(data, len));
	return handle;
}


SharedImmutableStringHandle makeSharedImmutableStringHandle(string_view str)
{
	if(str.empty())
		return SharedImmutableStringHandle(nullptr);
	else
		return SharedImmutableStringHandle(makeSharedImmutableString(str.data(), str.size()));
}


} // end namespace glare



#if BUILD_TESTS


#include "ConPrint.h"
#include "TestUtils.h"
#include <unordered_set>



void glare::SharedImmutableString::test()
{
	{
		SharedImmutableStringHandle s = makeSharedImmutableStringHandle("hello");
	}
	{
		SharedImmutableStringHandle s = makeSharedImmutableStringHandle("");
	}
	{
		SharedImmutableStringHandle s = makeEmptySharedImmutableStringHandle();
	}

	//--------------------------- Test empty string (handled with null string ref)  ---------------------------
	{
		SharedImmutableStringHandle a = makeSharedImmutableStringHandle("");
		testAssert(a.string.isNull());

		SharedImmutableStringHandle b = makeEmptySharedImmutableStringHandle();
		testAssert(b.string.isNull());
	}

	//--------------------------- Test operator == ---------------------------
	{
		SharedImmutableStringHandle a = makeSharedImmutableStringHandle("hello");
		SharedImmutableStringHandle b = makeSharedImmutableStringHandle("hello");
		SharedImmutableStringHandle c = makeSharedImmutableStringHandle("world");
		SharedImmutableStringHandle d = makeSharedImmutableStringHandle("");
		testAssert(a == b);
		testAssert(!(a == c));

		// Test empty string
		testAssert(!(a == d));
		testAssert(d == d);
	}

	//--------------------------- Test operator != ---------------------------
	{
		SharedImmutableStringHandle a = makeSharedImmutableStringHandle("hello");
		SharedImmutableStringHandle b = makeSharedImmutableStringHandle("hello");
		SharedImmutableStringHandle c = makeSharedImmutableStringHandle("world");
		SharedImmutableStringHandle d = makeSharedImmutableStringHandle("");
		testAssert(!(a != b));
		testAssert(a != c);

		// Test empty string
		testAssert(a != d);
		testAssert(!(d != d));
	}

	//--------------------------- Test storing in unordered_set ---------------------------

	// Test with pointer-equal key comparator.
	{
		std::unordered_set<SharedImmutableStringHandle, SharedImmutableStringHandleHasher, SharedImmutableStringHandleKeyPointerEqual> set;

		SharedImmutableStringHandle s = makeSharedImmutableStringHandle("hello");
		set.insert(s);

		SharedImmutableStringHandle s2 = makeSharedImmutableStringHandle("hello");
		set.insert(s2);

		testAssert(set.size() == 2);
	}

	// Test empty string with pointer-equal key comparator.
	{
		std::unordered_set<SharedImmutableStringHandle, SharedImmutableStringHandleHasher, SharedImmutableStringHandleKeyPointerEqual> set;

		SharedImmutableStringHandle s = makeSharedImmutableStringHandle("");
		set.insert(s);

		SharedImmutableStringHandle s2 = makeSharedImmutableStringHandle("");
		set.insert(s2);

		testAssert(set.size() == 1); // Even with pointer equality, null pointers mean s and s2 compare equal.
	}

	// Test with value-equal key comparator.
	{
		std::unordered_set<SharedImmutableStringHandle, SharedImmutableStringHandleHasher, SharedImmutableStringHandleKeyValueEqual> set;

		SharedImmutableStringHandle s = makeSharedImmutableStringHandle("hello");
		set.insert(s);

		SharedImmutableStringHandle s2 = makeSharedImmutableStringHandle("hello");
		set.insert(s2);

		testAssert(set.size() == 1);
	}

	// Test empty string with value-equal key comparator.
	{
		std::unordered_set<SharedImmutableStringHandle, SharedImmutableStringHandleHasher, SharedImmutableStringHandleKeyValueEqual> set;

		SharedImmutableStringHandle s = makeSharedImmutableStringHandle("");
		set.insert(s);

		SharedImmutableStringHandle s2 = makeSharedImmutableStringHandle("");
		set.insert(s2);

		testAssert(set.size() == 1);
	}

	conPrint("glare::SharedImmutableString::test() done.");
}


#endif
