/*=====================================================================
FilePrintOutput.h
-------------------
Copyright Glare Technologies Limited 2010 -
=====================================================================*/
#pragma once


#include "PrintOutput.h"
#include <fstream>


/*=====================================================================
FilePrintOutput
-------------------
A print output that writes to a file.
=====================================================================*/
class FilePrintOutput : public PrintOutput
{
public:
	FilePrintOutput(const std::string& path); // Throws Indigo::Exception
	~FilePrintOutput();

	virtual void print(const std::string& s);
	virtual void printStr(const std::string& s);

private:
	std::ofstream file;
};
