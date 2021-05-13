/*=====================================================================
FileInStream.cpp
----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "FileInStream.h"


#include "FileUtils.h"
#include "Exception.h"
#include "../maths/mathstypes.h"


FileInStream::FileInStream(const std::string& path)
:	file(path),
	read_index(0)
{}


FileInStream::~FileInStream()
{
}


int32 FileInStream::readInt32()
{
	if(read_index + sizeof(int32) > file.fileSize())
		throw glare::Exception("Read past end of file.");

	int32 x;
	std::memcpy(&x, (const uint8*)file.fileData() + read_index, sizeof(x));
	read_index += sizeof(x);
	return x;
}


uint32 FileInStream::readUInt32()
{
	if(read_index + sizeof(uint32) > file.fileSize())
		throw glare::Exception("Read past end of file.");

	uint32 x;
	std::memcpy(&x, (const uint8*)file.fileData() + read_index, sizeof(x));
	read_index += sizeof(x);
	return x;
}


uint16 FileInStream::readUInt16()
{
	if(read_index + sizeof(uint16) > file.fileSize())
		throw glare::Exception("Read past end of file.");

	uint16 x;
	std::memcpy(&x, (const uint8*)file.fileData() + read_index, sizeof(x));
	read_index += sizeof(x);
	return x;
}


void FileInStream::readData(void* buf, size_t num_bytes)
{
	if(num_bytes > 0)
	{
		if(read_index + num_bytes > file.fileSize())
			throw glare::Exception("Read past end of file.");

		std::memcpy(buf, (const uint8*)file.fileData() + read_index, num_bytes);
		read_index += num_bytes;
	}
}


bool FileInStream::endOfStream()
{
	return read_index >= file.fileSize();
}


void FileInStream::setReadIndex(size_t i)
{
	if(i > file.fileSize())
		throw glare::Exception("Invalid read index for setReadIndex().");
	read_index = i;
}