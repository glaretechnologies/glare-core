/*=====================================================================
BufferViewInStream.cpp
----------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "BufferViewInStream.h"


#include "Exception.h"
#include <cstring> // For std::memcpy


BufferViewInStream::BufferViewInStream(const ArrayRef<uint8> data_)
:	read_index(0),
	data(data_)
{}


BufferViewInStream::~BufferViewInStream()
{
}


int32 BufferViewInStream::readInt32()
{
	if(read_index + sizeof(int32) > data.size())
		throw glare::Exception("Read past end of buffer.");

	int32 x;
	std::memcpy(&x, &data.data()[read_index], sizeof(x));
	read_index += sizeof(x);
	return x;
}


uint32 BufferViewInStream::readUInt32()
{
	if(read_index + sizeof(uint32) > data.size())
		throw glare::Exception("Read past end of buffer.");

	uint32 x;
	std::memcpy(&x, &data.data()[read_index], sizeof(x));
	read_index += sizeof(x);
	return x;
}


void BufferViewInStream::readData(void* target_buf, size_t num_bytes)
{
	if(num_bytes > 0)
	{
		if(read_index + num_bytes > data.size())
			throw glare::Exception("Read past end of buffer.");

		std::memcpy(target_buf, &data.data()[read_index], num_bytes);
		read_index += num_bytes;
	}
}


bool BufferViewInStream::endOfStream()
{
	return read_index >= data.size();
}


void BufferViewInStream::setReadIndex(size_t i)
{
	if(i > data.size())
		throw glare::Exception("Invalid read index for setReadIndex().");
	read_index = i;
}
