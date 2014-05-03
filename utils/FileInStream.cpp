/*=====================================================================
FileInStream.cpp
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-01-28 15:26:30 +0000
=====================================================================*/
#include "FileInStream.h"


#include "FileUtils.h"
#include "Exception.h"
#include "../maths/mathstypes.h"


FileInStream::FileInStream(const std::string& path)
{
	file.open(FileUtils::convertUTF8ToFStreamPath(path).c_str(), std::ios::binary);

	if(file.fail())
		throw Indigo::Exception("Failed to open file '" + path + "' for reading.");
}


FileInStream::~FileInStream()
{
}


int32 FileInStream::readInt32()
{
	int32 x;
	file.read((char*)&x, sizeof(int32));

	if(file.fail())
		throw Indigo::Exception("Read from file failed.");

	return x;
}


uint32 FileInStream::readUInt32()
{
	uint32 x;
	file.read((char*)&x, sizeof(uint32));

	if(file.fail())
		throw Indigo::Exception("Read from file failed.");

	return x;
}


void FileInStream::readData(void* buf, size_t num_bytes, StreamShouldAbortCallback* should_abort_callback)
{
	// There's a bug on OS X that causes reads to fail when num_bytes >= 2^31 or so.
	// Work around it by breaking the read into chunks.
#ifdef OSX
	char* tempbuf = (char*)buf;

	while(num_bytes > 0)
	{
		const size_t num_to_read = myMin((size_t)1000000000u, num_bytes);
		
		file.read((char*)tempbuf, num_to_read); // Read chunk

		if(file.fail())
			throw Indigo::Exception("Read from file failed.");

		tempbuf += num_to_read;
		num_bytes -= num_to_read;
	}
#else
	file.read((char*)buf, num_bytes);

	if(file.fail())
		throw Indigo::Exception("Read from file failed.");
#endif
}


bool FileInStream::endOfStream()
{
	 return file.eof();
}
