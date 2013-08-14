/*=====================================================================
OutStream.cpp
-------------------
Copyright Glare Technologies Limited 2012 -
Generated at 2013-01-27 17:37:13 +0000
=====================================================================*/
#include "OutStream.h"



OutStream::OutStream()
{
}


OutStream::~OutStream()
{
}


void OutStream::writeStringLengthFirst(const std::string& s) 
{
	// Write string length
	writeUInt32((uint32)s.length());

	// Write string content
	writeData(s.c_str(), s.length(), NULL);
}


void OutStream::writeUInt64(uint64 x)
{
	writeData(&x, sizeof(x), NULL);
}


void OutStream::writeDouble(double x)
{
	writeData(&x, sizeof(x), NULL);
}
