/*=====================================================================
InStream.cpp
-------------------
Copyright Glare Technologies Limited 2012 -
Generated at 2013-01-27 17:47:46 +0000
=====================================================================*/
#include "InStream.h"


double InStream::readDouble()
{
	double x;
	readData(&x, sizeof(x));
	return x;
}

const std::string InStream::readString()
{
	// Read string byte size
	const uint32 size = readUInt32();

	std::string s(size, '\0'); // Use fill constructor

	// Read content
	if(size > 0)
		readData(&s[0], size);

	return s;
}
