/*=====================================================================
InStream.cpp
-------------------
Copyright Glare Technologies Limited 2012 -
Generated at 2013-01-27 17:47:46 +0000
=====================================================================*/
#include "InStream.h"


float InStream::readFloat()
{
	float x;
	readData(&x, sizeof(x));
	return x;
}


double InStream::readDouble()
{
	double x;
	readData(&x, sizeof(x));
	return x;
}


// Read uint64
uint64 InStream::readUInt64()
{
	uint64 x;
	readData(&x, sizeof(x));
	return x;
}


const std::string InStream::readStringLengthFirst()
{
	// Read string byte size
	const uint32 size = readUInt32();

	std::string s(size, '\0'); // Use fill constructor

	// Read content
	if(size > 0)
		readData(&s[0], size);

	return s;
}
