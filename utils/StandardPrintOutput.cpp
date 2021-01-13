/*=====================================================================
StandardPrintOutput.cpp
-----------------------
Copyright Glare Technologies Limited 2021 -
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
