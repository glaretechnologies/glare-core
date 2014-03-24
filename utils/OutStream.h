/*=====================================================================
OutStream.h
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-01-27 17:37:13 +0000
=====================================================================*/
#pragma once


#include "StreamShouldAbortCallback.h"
#include "Platform.h"
#include <string>


/*=====================================================================
OutStream
-------------------
Output stream - a binary stream that can be written to.
All methods return Indigo::Exception on failure.
=====================================================================*/
class OutStream
{
public:
	OutStream();
	virtual ~OutStream();

	virtual void writeUInt32(uint32 x) = 0;

	// should_abort_callback can be NULL.
	virtual void writeData(const void* data, size_t num_bytes, StreamShouldAbortCallback* should_abort_callback) = 0;

	// Write a string.  Must be less than 2^32 bytes long.
	virtual void writeStringLengthFirst(const std::string& s);

	virtual void writeDouble(double x);
};
