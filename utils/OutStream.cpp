/*=====================================================================
OutStream.cpp
-------------
Copyright Glare Technologies Limited 2021 -
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
	if(!s.empty())
		writeData(s.c_str(), s.length());
}


void OutStream::writeUInt64(uint64 x)
{
	writeData(&x, sizeof(x));
}


void OutStream::writeFloat(float x)
{
	writeData(&x, sizeof(x));
}


void OutStream::writeDouble(double x)
{
	writeData(&x, sizeof(x));
}
