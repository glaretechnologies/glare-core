/*=====================================================================
InStream.h
----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <string>


/*=====================================================================
InStream
-------------------
Input stream - a binary stream that can be read from.
All methods return glare::Exception on failure.
=====================================================================*/
class InStream
{
public:
	InStream() {}
	virtual ~InStream() {}

	// Read a single 32-bit signed int.  May do network->host byte reordering.
	virtual int32 readInt32() = 0;

	// Read a single 32-bit unsigned int.  May do network->host byte reordering.
	virtual uint32 readUInt32() = 0;

	// Read 'num_bytes' bytes to buf.  Buf must point at a buffer of at least num_bytes bytes.
	virtual void readData(void* buf, size_t num_bytes) = 0;

	// Read float
	float readFloat();

	// Read double
	double readDouble();

	// Read uint64
	uint64 readUInt64();

	// Are we at the end of the stream?  If the stream has no natural end (e.g. a socket connection) then returns false.
	// TODO: Remove
	virtual bool endOfStream() = 0;


	const std::string readStringLengthFirst(size_t max_string_length);
};
