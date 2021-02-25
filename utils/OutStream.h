/*=====================================================================
OutStream.h
-----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <string>


/*=====================================================================
OutStream
-------------------
Output stream - a binary stream that can be written to.
All methods return glare::Exception on failure.
=====================================================================*/
class OutStream
{
public:
	OutStream();
	virtual ~OutStream();

	// May do host->network byte reordering.
	virtual void writeInt32(int32 x) = 0;
	// May do host->network byte reordering.
	virtual void writeUInt32(uint32 x) = 0;

	virtual void writeData(const void* data, size_t num_bytes) = 0;

	// Write a string.  Must be less than 2^32 bytes long.
	virtual void writeStringLengthFirst(const std::string& s);

	void writeUInt64(uint64 x);
	void writeFloat(float x);
	void writeDouble(double x);
};
