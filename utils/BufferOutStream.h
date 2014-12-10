/*=====================================================================
BufferOutStream.h
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-01-27 17:56:55 +0000
=====================================================================*/
#pragma once


#include "OutStream.h"
#include <vector>


/*=====================================================================
BufferOutStream
-------------------

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
