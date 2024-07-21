/*=====================================================================
URL.cpp
-------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "URL.h"


#include "../webserver/Escaping.h"
#include "../utils/StringUtils.h"
#include "../utils/Parser.h"
#include "../utils/Exception.h"


// https://datatracker.ietf.org/doc/html/rfc3986#section-3.1
static void checkSchemeValid(const std::string& scheme)
{
	for(size_t i=0; i<scheme.size(); ++i)
	{
		const char c = scheme[i];
		const bool valid = isAlphaNumeric(c) || (c == '+') || (c == '-') || (c == '.');
		if(!valid)
			throw glare::Exception("invalid char in scheme: '" + std::string(1, c) + "'");
	}
}


// Check host is valid
// https://datatracker.ietf.org/doc/html/rfc3986#section-3.2.2
static void checkHostValid(const std::string& host)
{
	for(size_t i=0; i<host.size(); ++i)
	{
		const char c = host[i];

		// ':' character is used in IPv6 addresses, '[', and ']' enclose IPv6 addresses.
		// '%' to allow percent encoding
		const bool valid = isAlphaNumeric(c) || (c == '-') || (c == '.') || (c == ':') || (c == '[') || (c == ']') || (c == '%');
		if(!valid)
			throw glare::Exception("invalid or unhandled char in host: '" + std::string(1, c) + "'");
	}
}


// Unreserved Characters: https://datatracker.ietf.org/doc/html/rfc3986#section-2.3
inline static bool isUnreserved(char c)
{
	return isAlphaNumeric(c) || (c == '-') || (c == '.') || (c == '_') || (c == '~');
}


// https://datatracker.ietf.org/doc/html/rfc3986#appendix-A
inline static bool isSubDelim(char c)
{
	return 
		(c == '!') || (c == '$') || (c == '&') || (c == '\'') || (c == '(') || (c == ')') || 
		(c == '*') || (c == '+') || (c == ',') || (c == ';')  || (c == '=');
}


// pchar         = unreserved / pct-encoded / sub-delims / ":" / "@"
inline static bool isPChar(char c)
{
	return isUnreserved(c) || (c == '%') || isSubDelim(c) || (c == ':') || (c == '@');
}


// https://datatracker.ietf.org/doc/html/rfc3986#section-3.3
static void checkURLPathValid(const std::string& path)
{
	for(size_t i=0; i<path.size(); ++i)
	{
		const char c = path[i];
		const bool valid = isPChar(c) || (c == '/');
		if(!valid)
			throw glare::Exception("invalid or unhandled char in URL path: '" + std::string(1, c) + "'");
	}
}


static void checkURLQueryValid(const std::string& query)
{
	for(size_t i=0; i<query.size(); ++i)
	{
		const char c = query[i];
		const bool valid = isPChar(c) || (c == '/') || (c == '?');
		if(!valid)
			throw glare::Exception("invalid or unhandled char in URL query: '" + std::string(1, c) + "'");
	}
}


static void checkFragmentValid(const std::string& fragment)
{
	for(size_t i=0; i<fragment.size(); ++i)
	{
		const char c = fragment[i];
		const bool valid = isPChar(c) || (c == '/') || (c == '?');
		if(!valid)
			throw glare::Exception("invalid or unhandled char in URL fragment: '" + std::string(1, c) + "'");
	}
}


URL URL::parseURL(const std::string& url) // throws glare::Exception
{
	URL result;
	result.port = -1;

	Parser parser(url);

	//---------------- Parse scheme ----------------
	const size_t scheme_terminator_pos = url.find("://");
	if(scheme_terminator_pos != std::string::npos)
	{
		result.scheme = url.substr(0, scheme_terminator_pos);
		parser.setCurrentPos(scheme_terminator_pos + 3);

		checkSchemeValid(result.scheme);
	}

	//---------------- Parse host ----------------
	const size_t host_start = parser.currentPos();

	// Parse host until we come to ':', '/', '?' or '#'
	for(; !(parser.eof() || parser.current() == '/' || parser.current() == '?' || parser.current() == '#' || parser.current() == ':'); parser.advance())
	{}

	result.host = url.substr(host_start, parser.currentPos() - host_start);

	checkHostValid(result.host);

	//---------------- Parse port ----------------
	if(parser.currentIsChar(':'))
	{
		parser.consume(':');
		if(!parser.parseInt(result.port))
			throw glare::Exception("Failed to parse port.");
	}

	//---------------- Parse path ----------------
	if(parser.currentIsChar('/'))
	{
		const size_t path_start = parser.currentPos();

		// Parse path until we get to '?' or '#'
		for(; !(parser.eof() || parser.current() == '?' || parser.current() == '#'); parser.advance())
		{}

		result.path = url.substr(path_start, parser.currentPos() - path_start);

		checkURLPathValid(result.path);
	}

	//---------------- Parse query ----------------
	if(parser.currentIsChar('?'))
	{
		parser.consume('?');
		const size_t query_start = parser.currentPos();

		for(; !(parser.eof() || parser.current() == '#'); parser.advance())
		{}

		result.query = url.substr(query_start, parser.currentPos() - query_start);

		checkURLQueryValid(result.query);
	}

	//---------------- Parse fragment ----------------
	if(parser.currentIsChar('#'))
	{
		parser.consume('#');
		// Fragment is remaining part of string.
		result.fragment = url.substr(parser.currentPos(), url.size() - parser.currentPos());

		checkFragmentValid(result.fragment);
	}

	return result;
}


// NOTE: copied from Escaping::URLUnescape
static const std::string URLUnescape(const std::string& s)
{
	std::string result;
	result.reserve(s.size());

	for(size_t i=0; i<s.size(); ++i)
	{
		if(s[i] == '+')
		{
			result.push_back(' ');
		}
		else if(s[i] == '%')
		{
			if((i + 2) < s.size())
			{
				const unsigned int nibble_a = hexCharToUInt(s[i+1]);
				const unsigned int nibble_b = hexCharToUInt(s[i+2]);
				// TODO: handle char out of range
				result.push_back((char)((nibble_a << 4) + nibble_b));
				i += 2;
			}
			else
				throw glare::Exception("End of string while parsing escape sequence");
		}
		else
		{
			result.push_back(s[i]);
		}
	}
	return result;
}


// Parse query string, assuming is of the form
// a=b
// or
// a=b&c=d&e=f etc.
std::map<std::string, std::string> URL::parseQuery(const std::string& query)
{
	std::map<std::string, std::string> result_map;

	Parser parser(query);

	while(parser.notEOF())
	{
		string_view key, value;
		parser.parseToCharOrEOF('=', key);
		
		if(parser.parseChar('='))
		{
			parser.parseToCharOrEOF('&', value);

			if(!key.empty())
				result_map[URLUnescape(toString(key))] = URLUnescape(toString(value));

			parser.parseChar('&');
		}
	}
	return result_map;
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"
#include "../utils/TestExceptionUtils.h"


//========================== Fuzzing ==========================
#if 0
// Command line:
// C:\fuzz_corpus\url

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
		const std::string s((const char*)data, size);
		URL::parseURL(s);
	}
	catch(glare::Exception& e)
	{
		//conPrint("Excep: " + e.what());
	}

	return 0; // Non-zero return values are reserved for future use.
}
#endif
//========================== End fuzzing ==========================


#define testAssert2(expr) (TestUtils::doTestAssert((expr), (#expr), (__LINE__), (__FILE__)))


static void testQuery(const std::string& query, const std::map<std::string, std::string>& expected_map)
{
	try
	{
		const std::map<std::string, std::string> result_map = URL::parseQuery(query);
		testAssert2(result_map == expected_map);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


void URL::test()
{
	//================== Test parseQuery ===================
	testQuery(
		"",
		std::map<std::string, std::string>({})
	);

	testQuery(
		"a=b",
		std::map<std::string, std::string>({{"a", "b"}})
	);

	testQuery(
		"a=b&c=d",
		std::map<std::string, std::string>({{"a", "b"}, {"c", "d"}})
	);

	testQuery(
		"a=b&c=d&",
		std::map<std::string, std::string>({{"a", "b"}, {"c", "d"}})
	);

	testQuery(
		"a=b&c=d&e",
		std::map<std::string, std::string>({{"a", "b"}, {"c", "d"}})
	);

	testQuery(
		"a=b&c=d&e=",
		std::map<std::string, std::string>({{"a", "b"}, {"c", "d"}, {"e", ""}})
	);

	testQuery(
		"a=b&c=d&e=f",
		std::map<std::string, std::string>({{"a", "b"}, {"c", "d"}, {"e", "f"}})
	);

	// Test with unescaping
	testQuery(
		"a=b+c",
		std::map<std::string, std::string>({{"a", "b c"}})
	);

	testQuery(
		"a%20b=c%20d", // %20 is a space char
		std::map<std::string, std::string>({{"a b", "c d"}})
	);



	// Test URL with all components
	{
		URL url = parseURL("schemez://a.b.c:80/d/e/f?g=h&i=k#lmn");
		testStringsEqual(url.scheme, "schemez");
		testStringsEqual(url.host, "a.b.c");
		testEqual(url.port, 80);
		testStringsEqual(url.path, "/d/e/f");
		testStringsEqual(url.query, "g=h&i=k");
		testStringsEqual(url.fragment, "lmn");
	}

	// Test without scheme
	{
		URL url = parseURL("a.b.c:80/d/e/f?g=h&i=k#lmn");
		testStringsEqual(url.scheme, "");
		testStringsEqual(url.host, "a.b.c");
		testEqual(url.port, 80);
		testStringsEqual(url.path, "/d/e/f");
		testStringsEqual(url.query, "g=h&i=k");
		testStringsEqual(url.fragment, "lmn");
	}

	// Test without port
	{
		URL url = parseURL("schemez://a.b.c/d/e/f?g=h&i=k#lmn");
		testStringsEqual(url.scheme, "schemez");
		testStringsEqual(url.host, "a.b.c");
		testEqual(url.port, -1);
		testStringsEqual(url.path, "/d/e/f");
		testStringsEqual(url.query, "g=h&i=k");
		testStringsEqual(url.fragment, "lmn");
	}
	
	// Test without query and fragment
	{
		URL url = parseURL("schemez://a.b.c/d/e/f");
		testStringsEqual(url.scheme, "schemez");
		testStringsEqual(url.host, "a.b.c");
		testEqual(url.port, -1);
		testStringsEqual(url.path, "/d/e/f");
		testStringsEqual(url.query, "");
		testStringsEqual(url.fragment, "");
	}

	// Test without query but with fragment
	{
		URL url = parseURL("schemez://a.b.c/d/e/f#lmn");
		testStringsEqual(url.scheme, "schemez");
		testStringsEqual(url.host, "a.b.c");
		testEqual(url.port, -1);
		testStringsEqual(url.path, "/d/e/f");
		testStringsEqual(url.query, "");
		testStringsEqual(url.fragment, "lmn");
	}

	// Test just domain and no path etc..
	{
		URL url = parseURL("schemez://a.b.c");
		testStringsEqual(url.scheme, "schemez");
		testStringsEqual(url.host, "a.b.c");
		testEqual(url.port, -1);
		testStringsEqual(url.path, "");
		testStringsEqual(url.query, "");
		testStringsEqual(url.fragment, "");
	}

	// Test empty string
	{
		URL url = parseURL("");
		testStringsEqual(url.scheme, "");
		testStringsEqual(url.host, "");
		testEqual(url.port, -1);
		testStringsEqual(url.path, "");
		testStringsEqual(url.query, "");
		testStringsEqual(url.fragment, "");
	}

	// Test some invalid chars
	testThrowsExcepContainingString([&]() {  parseURL("sch\nemez://a.b.c:80/d/e/f?g=h&i=k#lmn"); }, "scheme");
	testThrowsExcepContainingString([&]() {  parseURL("schemez://a.\n.c:80/d/e/f?g=h&i=k#lmn"); }, "host");
	testThrowsExcepContainingString([&]() {  parseURL("schemez://a.b.c:80/d/\n/f?g=h&i=k#lmn"); }, "path");
	testThrowsExcepContainingString([&]() {  parseURL("schemez://a.b.c:80/d/e/f?g=\n&i=k#lmn"); }, "query");
	testThrowsExcepContainingString([&]() {  parseURL("schemez://a.b.c:80/d/e/f?g=h&i=k#l\nn"); }, "fragment");
}


#endif
