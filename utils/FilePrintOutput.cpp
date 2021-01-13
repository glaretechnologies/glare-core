/*=====================================================================
FilePrintOutput.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
=====================================================================*/
#include "FilePrintOutput.h"


#include "../utils/Exception.h"


FilePrintOutput::FilePrintOutput(const std::string& path)
{
	file.open(path);
	if(!file)
		throw glare::Exception("Failed to open file '" + path + "' for writing.");
}


FilePrintOutput::~FilePrintOutput()
{

}


void FilePrintOutput::print(const std::string& s)
{
	file << s << std::endl;
}


void FilePrintOutput::printStr(const std::string& s)
{
	file << s;
}
