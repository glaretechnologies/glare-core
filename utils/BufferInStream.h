/*=====================================================================
BufferInStream.h
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-01-27 17:57:00 +0000
=====================================================================*/
#pragma once


#include "InStream.h"
#include <vector>


/*=====================================================================
BufferInStream
-------------------

=====================================================================*/
class BufferInStream : public InStream
{
public:
	BufferInStream(const std::vector<unsigned char>& buf);
	virtual ~BufferInStream();

	virtual uint32 readUInt32();
	virtual void readData(void* buf, size_t num_bytes, StreamShouldAbortCallback* should_abort_callback);
	virtual bool endOfStream();

private:
	std::vector<unsigned char> buf;
	size_t read_index;
};
