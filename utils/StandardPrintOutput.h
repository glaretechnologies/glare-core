/*=====================================================================
StandardPrintOutput.h
---------------------
File created by ClassTemplate on Tue Feb 10 12:27:26 2009
Code By Nicholas Chapman.
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
