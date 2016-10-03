/*=====================================================================
FileInStream.h
-------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2013-01-28 15:26:30 +0000
=====================================================================*/
#pragma once


#include "InStream.h"
#include "MemMappedFile.h"
#include <string>


/*=====================================================================
FileInStream
-------------------

=====================================================================*/
class FileInStream : public InStream
{
public:
	FileInStream(const std::string& path); // Throws Indigo::Exception on failure.
	~FileInStream();

	virtual int32 readInt32();
	virtual uint32 readUInt32();
	virtual void readData(void* buf, size_t num_bytes);
	virtual bool endOfStream();

private:
	MemMappedFile file;
	size_t read_index;
};
