/*=====================================================================
PrintOutput.h
-------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include <string>


/*=====================================================================
PrintOutput
-----------

=====================================================================*/
class PrintOutput
{
public:
	PrintOutput() {}
	virtual ~PrintOutput() {}

	virtual void print(const std::string& s) = 0; // Print a message and a newline character.
	virtual void printStr(const std::string& s) = 0; // Print a message without a newline character.
};
