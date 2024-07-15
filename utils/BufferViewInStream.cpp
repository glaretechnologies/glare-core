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
	if(!canReadNBytes(sizeof(int32)))
		throw glare::Exception("Read past end of buffer.");

	int32 x;
	std::memcpy(&x, &data.data()[read_index], sizeof(x));
	read_index += sizeof(x);
	return x;
}


uint32 BufferViewInStream::readUInt32()
{
	if(!canReadNBytes(sizeof(uint32)))
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
		if(!canReadNBytes(num_bytes))
			throw glare::Exception("Read past end of buffer.");

		std::memcpy(target_buf, &data.data()[read_index], num_bytes);
		read_index += num_bytes;
	}
}


bool BufferViewInStream::endOfStream()
{
	return read_index >= data.size();
}


bool BufferViewInStream::canReadNBytes(size_t N) const
{
	return ((read_index + N) <= data.size()) && !Maths::unsignedIntAdditionWraps(read_index, N);
}


void BufferViewInStream::setReadIndex(size_t i)
{
	if(i > data.size())
		throw glare::Exception("Invalid read index for setReadIndex().");
	read_index = i;
}


void BufferViewInStream::advanceReadIndex(size_t n)
{
	if(!canReadNBytes(n)) // Does wrapping/overflow check and checks against file size.
		throw glare::Exception("Invalid number of bytes to advance for advanceReadIndex - read past end of file.");
	read_index += n;
}


uint8 BufferViewInStream::readUInt8()
{
	if(!canReadNBytes(sizeof(uint8)))
		throw glare::Exception("Read past end of buffer.");

	uint8 x;
	std::memcpy(&x, data.data() + read_index, sizeof(x));
	read_index += sizeof(x);
	return x;
}


uint16 BufferViewInStream::readUInt16()
{
	if(!canReadNBytes(sizeof(uint16)))
		throw glare::Exception("Read past end of file.");

	uint16 x;
	std::memcpy(&x, data.data() + read_index, sizeof(x));
	read_index += sizeof(x);
	return x;
}
