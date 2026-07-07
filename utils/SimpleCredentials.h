/*=====================================================================
SimpleCredentials.h
-------------------
Copyright Glare Technologies Limited 2026 -
=====================================================================*/
#pragma once


#include <map>
#include <string>


/*=====================================================================
SimpleCredentials
-----------------
A simple map of credentials.
Can be parsed from a text file.
=====================================================================*/
class SimpleCredentials
{
public:
	// Throws glare::Exception on failure.
	static SimpleCredentials parseCredentials(const std::string& credentials_path);

	std::map<std::string, std::string> creds;
};
