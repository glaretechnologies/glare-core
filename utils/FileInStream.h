/*=====================================================================
FileInStream.h
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-01-28 15:26:30 +0000
=====================================================================*/
#pragma once


#include "InStream.h"
#include <fstream>
#include <string>


/*=====================================================================
FileInStream
-------------------

=====================================================================*/
class FileInStream : public InStream
{
public:
	FileInStream(const std::string& path);
	~FileInStream();

	virtual uint32 readUInt32();
	virtual void readData(void* buf, size_t num_bytes, StreamShouldAbortCallback* should_abort_callback);
	virtual bool endOfStream();

private:
	std::ifstream file;
};
