/*=====================================================================
URL.cpp
-------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "URL.h"


#include "../utils/StringUtils.h"
#include "../utils/Parser.h"
#include "../utils/Exception.h"


URL URL::parseURL(const std::string& url) // throws Indigo::Exception
{
	URL result;
	result.port = -1;

	Parser parser(url.data(), (unsigned int)url.size());

	//---------------- Parse scheme ----------------
	const size_t scheme_terminator_pos = url.find("://");
	if(scheme_terminator_pos != std::string::npos)
	{
		result.scheme = url.substr(0, scheme_terminator_pos);
		parser.setCurrentPos((unsigned int)scheme_terminator_pos + 3);
	}

	//---------------- Parse host ----------------
	const size_t host_start = parser.currentPos();

	// Parse host until we come to ':', '/', '?' or '#'
	for(; !(parser.eof() || parser.current() == '/' || parser.current() == '?' || parser.current() == '#' || parser.current() == ':'); parser.advance())
	{}

	result.host = url.substr(host_start, parser.currentPos() - host_start);

	//---------------- Parse port ----------------
	if(parser.currentIsChar(':'))
	{
		parser.consume(':');
		if(!parser.parseInt(result.port))
			throw Indigo::Exception("Failed to parse port.");
	}

	//---------------- Parse path ----------------
	if(parser.currentIsChar('/'))
	{
		const size_t path_start = parser.currentPos();

		// Parse path until we get to '?' or '#'
		for(; !(parser.eof() || parser.current() == '?' || parser.current() == '#'); parser.advance())
		{}

		result.path = url.substr(path_start, parser.currentPos() - path_start);
	}

	//---------------- Parse query ----------------
	if(parser.currentIsChar('?'))
	{
		parser.consume('?');
		const size_t query_start = parser.currentPos();

		for(; !(parser.eof() || parser.current() == '#'); parser.advance())
		{}

		result.query = url.substr(query_start, parser.currentPos() - query_start);
	}

	//---------------- Parse fragment ----------------
	if(parser.currentIsChar('#'))
	{
		parser.consume('#');
		// Fragment is remaining part of string.
		result.fragment = url.substr(parser.currentPos(), url.size() - parser.currentPos());
	}

	return result;
}


#if BUILD_TESTS


#include "../utils/TestUtils.h"


void URL::test()
{
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
}


#endif
