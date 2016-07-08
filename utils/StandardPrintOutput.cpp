/*=====================================================================
StandardPrintOutput.cpp
-----------------------
File created by ClassTemplate on Tue Feb 10 12:27:26 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "StandardPrintOutput.h"


#include "../utils/ConPrint.h"


StandardPrintOutput::StandardPrintOutput()
{
}


StandardPrintOutput::~StandardPrintOutput()
{
}


void StandardPrintOutput::print(const std::string& s)
{
	conPrint(s);
}


void StandardPrintOutput::printStr(const std::string& s)
{
	conPrintStr(s);
}
