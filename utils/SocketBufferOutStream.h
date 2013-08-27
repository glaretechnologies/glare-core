/*=====================================================================
SocketBufferOutStream.h
-----------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-01-27 17:56:55 +0000
=====================================================================*/
#pragma once


#include "OutStream.h"
#include <vector>


/*=====================================================================
SocketBufferOutStream
---------------------
Like BufferOutStream, but does any htonl conversion that MySocket does.
=====================================================================*/
class SocketBufferOutStream : public OutStream
{
public:
	SocketBufferOutStream();
	virtual ~SocketBufferOutStream();

	virtual void writeData(const void* data, size_t num_bytes, StreamShouldAbortCallback* should_abort_callback);

	virtual void writeUInt32(uint32 x);
	virtual void writeUInt64(uint64 x);

	std::vector<unsigned char> buf;
};
