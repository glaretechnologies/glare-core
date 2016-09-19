/*=====================================================================
BufferInStream.h
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-01-27 17:57:00 +0000
=====================================================================*/
#pragma once


#include "InStream.h"
#include "Vector.h"
#include <vector>


/*=====================================================================
BufferInStream
-------------------

=====================================================================*/
class BufferInStream : public InStream
{
public:
	BufferInStream();
	BufferInStream(const std::vector<unsigned char>& buf);
	virtual ~BufferInStream();

	virtual int32 readInt32();
	virtual uint32 readUInt32();
	virtual void readData(void* buf, size_t num_bytes);
	virtual bool endOfStream();

	js::Vector<unsigned char, 16> buf;
	size_t read_index;
};
