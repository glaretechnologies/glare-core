/*=====================================================================
FileOutStream.cpp
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-01-28 15:26:26 +0000
=====================================================================*/
#include "FileOutStream.h"


#include "FileUtils.h"
#include "Exception.h"


FileOutStream::FileOutStream(const std::string& path)
{
	file.open(FileUtils::convertUTF8ToFStreamPath(path).c_str(), std::ios::binary);

	if(file.fail())
		throw Indigo::Exception("Failed to open file '" + path + "' for writing.");
}


FileOutStream::~FileOutStream()
{
}


void FileOutStream::writeInt32(int32 x)
{
	file.write((const char*)&x, sizeof(int32));

	if(file.fail())
		throw Indigo::Exception("Write to file failed.");
}


void FileOutStream::writeUInt32(uint32 x)
{
	file.write((const char*)&x, sizeof(uint32));

	if(file.fail())
		throw Indigo::Exception("Write to file failed.");
}


void FileOutStream::writeData(const void* data, size_t num_bytes)
{
	file.write((const char*)data, num_bytes);

	if(file.fail())
		throw Indigo::Exception("Write to file failed.");
}
