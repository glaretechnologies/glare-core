/*=====================================================================
StandardPrintOutput.cpp
-----------------------
File created by ClassTemplate on Tue Feb 10 12:27:26 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "StandardPrintOutput.h"


#include <iostream>


StandardPrintOutput::StandardPrintOutput()
{
}


StandardPrintOutput::~StandardPrintOutput()
{
}


void StandardPrintOutput::print(const std::string& s)
{
	std::cout << s << std::endl;
}


void StandardPrintOutput::printStr(const std::string& s)
{
	std::cout << s;
}
