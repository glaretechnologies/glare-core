/*=====================================================================
BufferInStream.h
-------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "InStream.h"
#include "Vector.h"
#include <vector>


/*=====================================================================
BufferInStream
--------------
Input stream that reads from a buffer
=====================================================================*/
class BufferInStream : public InStream
{
public:
	BufferInStream();
	BufferInStream(const std::vector<unsigned char>& buf);
	virtual ~BufferInStream();

	void clear(); // Resizes buffer to zero, resets read_index to zero.

	virtual int32 readInt32();
	virtual uint32 readUInt32();
	virtual void readData(void* buf, size_t num_bytes);
	virtual bool endOfStream();

	js::Vector<unsigned char, 16> buf;
	size_t read_index;
};
