/*=====================================================================
InStream.cpp
------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "InStream.h"


#include "Exception.h"
#include "RuntimeCheck.h"
#include "StringUtils.h"


void InStream::readDataChecked(MutableArrayRef<uint8> buf, size_t buf_index, size_t num_bytes)
{
	if(num_bytes > 0)
	{
		runtimeCheck(
			!Maths::unsignedIntAdditionWraps(buf_index, num_bytes) &&
			((buf_index + num_bytes) <= buf.size())
		);

		readData(buf.data() + buf_index, num_bytes);
	}
}


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


const std::string InStream::readStringLengthFirst(size_t max_string_length)
{
	// Read string byte size
	const uint32 size = readUInt32();
	if((size_t)size > max_string_length)
		throw glare::Exception("String length too long (length=" + toString(size) + ")");

	std::string s(size, '\0'); // Use fill constructor

	// Read content
	if(size > 0)
		readData(&s[0], size);

	return s;
}
