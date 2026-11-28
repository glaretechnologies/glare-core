/*=====================================================================
StringBuilder.cpp
-----------------
Copyright Glare Technologies Limited 2026 - 
=====================================================================*/
#include "StringBuilder.h"


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"


void StringBuilder::test()
{
	conPrint("StringBuilder::test()");

	// Test append
	{
		StringBuilder builder;
		testAssert(builder.getStringView() == "");
		
		builder.append("aaa");

		testAssert(builder.getStringView() == "aaa");

		builder.append("bbb");
		testAssert(builder.getStringView() == "aaabbb");

		builder.append("ccc");
		testAssert(builder.getStringView() == "aaabbbccc");
	}

	// Test appendInt
	{
		StringBuilder builder;
		builder.appendInt(0);
		testAssert(builder.getStringView() == "0");
		builder.appendInt(123);
		testAssert(builder.getStringView() == "0123");
		builder.appendInt(-456);
		testAssert(builder.getStringView() == "0123-456");
	}

	// Test appendSizeT
	{
		StringBuilder builder;
		builder.appendSizeT(0);
		testAssert(builder.getStringView() == "0");
		builder.appendSizeT(123);
		testAssert(builder.getStringView() == "0123");
		builder.appendSizeT(456);
		testAssert(builder.getStringView() == "0123456");
	}

	// Test removeSuffixInPlace
	{
		StringBuilder builder;
		builder.removeSuffixInPlace("aa");
		testAssert(builder.getStringView() == "");

		builder.append("aabb");
		builder.removeSuffixInPlace("bb");
		testAssert(builder.getStringView() == "aa");

		builder.removeSuffixInPlace("cccccc");
		testAssert(builder.getStringView() == "aa");
	}

	// Test exceeding the smallvector size
	{
		StringBuilder builder;
		for(size_t i=0; i<512; ++i)
			builder.append("a");
		testAssert(builder.getStringView() == std::string(512, 'a'));

		builder.append("b");
		testAssert(builder.getStringView() == std::string(512, 'a') + std::string("b"));

		builder.removeSuffixInPlace("ab");
		testAssert(builder.getStringView() == std::string(511, 'a'));
	}


	conPrint("StringBuilder::test() done");
}


#endif // BUILD_TESTS
