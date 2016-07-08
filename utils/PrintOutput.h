/*=====================================================================
PrintOutput.h
-------------
File created by ClassTemplate on Tue Feb 10 11:32:16 2009
Code By Nicholas Chapman.
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
