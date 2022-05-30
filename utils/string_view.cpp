/*=====================================================================
string_view.cpp
---------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "string_view.h"


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/Timer.h"
#include "../utils/ConPrint.h"


void testStringView()
{
	conPrint("testStringView()");

	//==================================== Test copy constructor ====================================
	{
		const std::string str = "hello";
		const string_view v(str);
		testAssert(v.size() == 5);
		testAssert(v.data() == str.data());
		testAssert(v[0] == 'h');
		testAssert(v[4] == 'o');
		testAssert(v == "hello");
	}

	//==================================== Test copy constructor ====================================
	{
		const std::string str = "hello";
		const string_view v = str;
		testAssert(v == "hello");
	}

	//==================================== Test assignment operator ====================================
	{
		const std::string str = "hello";
		string_view v;
		v = str;
		testAssert(v == "hello");
	}


	//==================================== Test a string_view part way inside a buffer ====================================
	{
		const std::string str = "abcde";
		{
			const string_view v(str.data(), 1);
			testAssert(v == "a");
		}
		{
			const string_view v(str.data(), 4);
			testAssert(v == "abcd");
		}
		{
			const string_view v(str.data() + 1, 1);
			testAssert(v == "b");
		}
		{
			const string_view v(str.data() + 1, 4);
			testAssert(v == "bcde");
		}
	}

	//==================================== Test operator == ====================================
	{
		const std::string str = "abcde";
		string_view v(str);
		testAssert(v == str);
		testAssert(v == "abcde");
		testAssert(str == v);
		testAssert("abcde" == v);
		testAssert(v == v);

		{
		string_view v2("abcd1"); // same length
		testAssert(!(v == v2));
		testAssert(v != v2);
		}
		{
		string_view v2("abcd"); // different length
		testAssert(!(v == v2));
		testAssert(v != v2);
		}
		{
		string_view v2(str.data(), 0); // emptry view
		testAssert(!(v == v2));
		testAssert(v != v2);
		}
	}

	//==================================== Test to_string ====================================
	{
		{
			string_view v2("a");
			testAssert(v2.to_string() == std::string("a"));
		}
		{
			string_view v2("ab");
			testAssert(v2.to_string() == std::string("ab"));
		}

		// Empty string
		{
			string_view v2("");
			testAssert(v2.to_string() == std::string(""));
		}
	}

	//==================================== Test substr ====================================
	{
		testAssert(string_view("").substr(0, 0) == std::string(""));
		testAssert(string_view("a").substr(0, 0) == std::string(""));
		testAssert(string_view("a").substr(0, 1) == std::string("a"));
		testAssert(string_view("ab").substr(0, 1) == std::string("a"));
		testAssert(string_view("ab").substr(1, 1) == std::string("b"));
		testAssert(string_view("ab").substr(0, 2) == std::string("ab"));

		// Test n being greater than the number of remaining chars, in this case just returns all remaining chars.
		testAssert(string_view("").substr(0, 10) == std::string(""));
		testAssert(string_view("ab").substr(0, 10) == std::string("ab"));
		testAssert(string_view("ab").substr(1, 10) == std::string("b"));
	}
}


#endif // BUILD_TESTS
