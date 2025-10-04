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


void OutStream::writeStringLengthFirst(const string_view s) 
{
	// Write string length
	writeUInt32((uint32)s.length());

	// Write string content
	if(!s.empty())
		writeData(s.data(), s.length());
}


void OutStream::writeStringLengthFirst(const glare::SharedImmutableString& s)
{
	// Write string length
	const uint32 size = (uint32)s.size();
	writeUInt32(size);

	// Write string content
	if(size > 0)
		writeData(s.data(), size);
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
