/*=====================================================================
URL.h
-----
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include <string>
#include <map>


/*=====================================================================
URL
---
Parsed URL, broken into the components.

https://en.wikipedia.org/wiki/Uniform_Resource_Identifier#Syntax
https://datatracker.ietf.org/doc/html/rfc3986
=====================================================================*/
class URL
{
public:
	static URL parseURL(const std::string& url); // Parse a URL string.  Throws glare::Exception on failure.

	// Parses query string, also unescapes keys and values.
	// Assumes of the form
	// a=b
	// or
	// a=b&c=d&e=f etc.
	static std::map<std::string, std::string> parseQuery(const std::string& query);

	// Resolves a URL 'reference', such as the Location header of a redirect, against the URL it was received for.
	// 'ref' may be absolute ("https://a.com/x"), scheme-relative ("//a.com/x"), host-relative ("/x"), a query ("?q=1"), a fragment ("#f"),
	// or relative to the base URL's directory ("x/y.png").  Dot segments such as "../" are passed through, not removed.
	static std::string resolveToAbsoluteURL(const URL& base, const std::string& ref);

	static void test();

	std::string scheme; // Without the trailing "://"
	std::string host;
	int port; // -1 if no port parsed.
	std::string path; // With the leading '/'
	std::string query; // Without the '?'
	std::string fragment; // Without the '#'
};
