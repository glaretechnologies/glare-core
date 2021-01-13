/*=====================================================================
StandardPrintOutput.h
---------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "PrintOutput.h"


/*=====================================================================
StandardPrintOutput
-------------------

=====================================================================*/
class StandardPrintOutput : public PrintOutput
{
public:
	StandardPrintOutput();

	virtual ~StandardPrintOutput();

	static StandardPrintOutput create() { StandardPrintOutput s; return s; }


	virtual void print(const std::string& s);
	virtual void printStr(const std::string& s);
};
