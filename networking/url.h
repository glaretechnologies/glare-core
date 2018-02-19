/*=====================================================================
url.h
-----
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include <string>


/*=====================================================================
URL
---
Parsed URL, broken into the components.
=====================================================================*/
class URL
{
public:
	static URL parseURL(const std::string& url); // Parse a URL string.  Throws Indigo::Exception on failure.

	static void test();

	std::string scheme; // Without the trailing "://"
	std::string host;
	int port; // -1 if no port parsed.
	std::string path; // With the leading '/'
	std::string query; // Without the '?'
	std::string fragment; // Without the '#'
};
