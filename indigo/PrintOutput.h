/*=====================================================================
PrintOutput.h
-------------
File created by ClassTemplate on Tue Feb 10 11:32:16 2009
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PRINTOUTPUT_H_666_
#define __PRINTOUTPUT_H_666_


#include <string>


/*=====================================================================
PrintOutput
-----------

=====================================================================*/
class PrintOutput
{
public:
	PrintOutput(){}
	virtual ~PrintOutput(){}

	virtual void print(const std::string& s) = 0;
	virtual void printStr(const std::string& s) = 0;
};


#endif //__PRINTOUTPUT_H_666_
