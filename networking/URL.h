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

	static void test();

	std::string scheme; // Without the trailing "://"
	std::string host;
	int port; // -1 if no port parsed.
	std::string path; // With the leading '/'
	std::string query; // Without the '?'
	std::string fragment; // Without the '#'
};
