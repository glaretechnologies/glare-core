/*=====================================================================
StackStringBuilder.cpp
----------------------
Copyright Glare Technologies Limited 2026 - 
=====================================================================*/
#include "StackStringBuilder.h"


#if BUILD_TESTS


#include "TestUtils.h"


void StackStringBuilder::test()
{
	{
		glare::StackAllocator allocator(1024 * 1024);

		StackStringBuilder builder(allocator, /*max size=*/10);

		testAssert(builder.getStringView() == "");
		
		builder.append("aaa");

		testAssert(builder.getStringView() == "aaa");

		builder.append("bbb");

		testAssert(builder.getStringView() == "aaabbb");

		builder.append("ccc");

		testAssert(builder.getStringView() == "aaabbbccc");

		builder.append("ddd");

		testAssert(builder.getStringView() == "aaabbbcccd");

		builder.append("eee");

		testAssert(builder.getStringView() == "aaabbbcccd");
	}
}


#endif // BUILD_TESTS
