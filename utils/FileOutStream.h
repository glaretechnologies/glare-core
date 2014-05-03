/*=====================================================================
FileOutStream.h
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-01-28 15:26:26 +0000
=====================================================================*/
#pragma once


#include "OutStream.h"
#include <fstream>
#include <string>


/*=====================================================================
FileOutStream
-------------------

=====================================================================*/
class FileOutStream : public OutStream
{
public:
	FileOutStream(const std::string& path);
	~FileOutStream();

	virtual void writeInt32(int32 x);
	virtual void writeUInt32(uint32 x);
	virtual void writeData(const void* data, size_t num_bytes, StreamShouldAbortCallback* should_abort_callback);

private:
	std::ofstream file;
};
