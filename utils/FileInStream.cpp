/*=====================================================================
FileInStream.cpp
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-01-28 15:26:30 +0000
=====================================================================*/
#include "FileInStream.h"


#include "FileUtils.h"
#include "Exception.h"


FileInStream::FileInStream(const std::string& path)
{
	file.open(FileUtils::convertUTF8ToFStreamPath(path).c_str(), std::ios::binary);

	if(file.fail())
		throw Indigo::Exception("Failed to open file '" + path + "' for reading.");
}


FileInStream::~FileInStream()
{
}


uint32 FileInStream::readUInt32()
{
	uint32 x;
	file.read((char*)&x, sizeof(uint32));
	return x;
}


void FileInStream::readData(void* buf, size_t num_bytes)
{
	 file.read((char*)buf, num_bytes);
}


bool FileInStream::endOfStream()
{
	 return file.eof();
}
