/*=====================================================================
BufferOutStream.h
------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "OutStream.h"
#include <vector>


/*=====================================================================
BufferOutStream
---------------
Output stream that writes to a buffer.
=====================================================================*/
class BufferOutStream : public OutStream
{
public:
	BufferOutStream();
	virtual ~BufferOutStream();

	virtual void writeInt32(int32 x);
	virtual void writeUInt32(uint32 x);
	virtual void writeData(const void* data, size_t num_bytes);


	std::vector<unsigned char> buf;
};
