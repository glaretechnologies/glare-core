/*=====================================================================
FilePrintOutput.h
-------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "PrintOutput.h"
#include <fstream>


/*=====================================================================
FilePrintOutput
---------------
A print output that writes to a file.
=====================================================================*/
class FilePrintOutput : public PrintOutput
{
public:
	FilePrintOutput(const std::string& path); // Throws glare::Exception
	~FilePrintOutput();

	virtual void print(const std::string& s);
	virtual void printStr(const std::string& s);

private:
	std::ofstream file;
};
