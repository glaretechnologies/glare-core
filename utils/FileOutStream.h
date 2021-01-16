/*=====================================================================
FileOutStream.h
---------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "OutStream.h"
#include <fstream>
#include <string>


/*=====================================================================
FileOutStream
-------------

=====================================================================*/
class FileOutStream : public OutStream
{
public:
	FileOutStream(const std::string& path);
	~FileOutStream();

	virtual void writeInt32(int32 x);
	virtual void writeUInt32(uint32 x);
	void writeUInt64(uint64 x);
	virtual void writeData(const void* data, size_t num_bytes);

private:
	std::ofstream file;
};
