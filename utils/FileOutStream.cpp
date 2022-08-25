/*=====================================================================
FileOutStream.cpp
-----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "FileOutStream.h"


#include "FileUtils.h"
#include "Exception.h"


FileOutStream::FileOutStream(const std::string& path, std::ios_base::openmode openmode)
:	write_i(0)
{
	file.open(FileUtils::convertUTF8ToFStreamPath(path).c_str(), openmode);

	if(file.fail())
		throw glare::Exception("Failed to open file '" + path + "' for writing.");
}


FileOutStream::~FileOutStream()
{
}


void FileOutStream::close()
{
	file.close();
	if(file.fail())
		throw glare::Exception("close failed.");
}


void FileOutStream::writeInt32(int32 x)
{
	file.write((const char*)&x, sizeof(int32));

	if(file.fail())
		throw glare::Exception("Write to file failed.");

	write_i += sizeof(int32);
}


void FileOutStream::writeUInt32(uint32 x)
{
	file.write((const char*)&x, sizeof(uint32));

	if(file.fail())
		throw glare::Exception("Write to file failed.");

	write_i += sizeof(uint32);
}


void FileOutStream::writeUInt64(uint64 x)
{
	file.write((const char*)&x, sizeof(uint64));

	if(file.fail())
		throw glare::Exception("Write to file failed.");

	write_i += sizeof(uint64);
}


void FileOutStream::writeData(const void* data, size_t num_bytes)
{
	file.write((const char*)data, num_bytes);

	if(file.fail())
		throw glare::Exception("Write to file failed.");

	write_i += num_bytes;
}


void FileOutStream::seek(size_t new_index)
{
	file.seekp(new_index);

	if(file.fail())
		throw glare::Exception("Seek failed.");

	write_i = new_index;
}


void FileOutStream::flush()
{
	file.flush();
}
