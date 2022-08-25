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
	FileOutStream(const std::string& path, std::ios_base::openmode openmode = std::ios::binary);
	~FileOutStream();

	void close(); // Manually closes stream, checks for errors.

	virtual void writeInt32(int32 x);
	virtual void writeUInt32(uint32 x);
	void writeUInt64(uint64 x);
	virtual void writeData(const void* data, size_t num_bytes);


	void seek(size_t new_index);

	size_t getWriteIndex() const { return write_i; } // std::ofstream.tellp() is non-const, so maintain the write index ourselves.

	void flush();
private:
	std::ofstream file;
	size_t write_i; 
};
