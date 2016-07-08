/*=====================================================================
StandardPrintOutput.h
---------------------
File created by ClassTemplate on Tue Feb 10 12:27:26 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __STANDARDPRINTOUTPUT_H_666_
#define __STANDARDPRINTOUTPUT_H_666_


#include "PrintOutput.h"


/*=====================================================================
StandardPrintOutput
-------------------

=====================================================================*/
class StandardPrintOutput : public PrintOutput
{
public:
	/*=====================================================================
	StandardPrintOutput
	-------------------

	=====================================================================*/
	StandardPrintOutput();

	virtual ~StandardPrintOutput();

	static StandardPrintOutput create() { StandardPrintOutput s; return s; }


	virtual void print(const std::string& s);
	virtual void printStr(const std::string& s);
};


#endif //__STANDARDPRINTOUTPUT_H_666_
