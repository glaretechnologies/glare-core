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
	PrintOutput() { }
	virtual ~PrintOutput() { }

	virtual void print(const std::string& s) = 0;
	virtual void printStr(const std::string& s) = 0;
};
