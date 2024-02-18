/*=====================================================================
BufferInStream.cpp
------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "BufferInStream.h"


#include "Exception.h"
#include <cstring> // For std::memcpy


BufferInStream::BufferInStream()
:	read_index(0)
{}


BufferInStream::BufferInStream(const std::string& s)
:	read_index(0)
{
	buf.resizeNoCopy(s.size());

	if(!s.empty())
		std::memcpy(buf.data(), s.data(), s.size());
}


BufferInStream::BufferInStream(const std::vector<unsigned char>& buf_)
:	read_index(0)
{
	buf.resizeNoCopy(buf_.size());
	if(!buf_.empty())
		std::memcpy(buf.data(), buf_.data(), buf_.size());
}


BufferInStream::BufferInStream(const ArrayRef<unsigned char>& data)
:	read_index(0)
{
	buf.resizeNoCopy(data.size());
	if(!data.empty())
		std::memcpy(buf.data(), data.data(), data.size());
}


BufferInStream::~BufferInStream()
{
}


void BufferInStream::clear() // Resizes buffer to zero, resets read_index to zero.
{
	buf.clear();
	read_index = 0;
}


int32 BufferInStream::readInt32()
{
	if(!canReadNBytes(sizeof(int32)))
		throw glare::Exception("Read past end of buffer.");

	int32 x;
	std::memcpy(&x, &buf[read_index], sizeof(x));
	read_index += sizeof(x);
	return x;
}


uint32 BufferInStream::readUInt32()
{
	if(!canReadNBytes(sizeof(uint32)))
		throw glare::Exception("Read past end of buffer.");

	uint32 x;
	std::memcpy(&x, &buf[read_index], sizeof(x));
	read_index += sizeof(x);
	return x;
}


void BufferInStream::readData(void* target_buf, size_t num_bytes)
{
	if(num_bytes > 0)
	{
		if(!canReadNBytes(num_bytes))
			throw glare::Exception("Read past end of buffer.");

		std::memcpy(target_buf, &buf[read_index], num_bytes);
		read_index += num_bytes;
	}
}


bool BufferInStream::endOfStream()
{
	return read_index >= buf.size();
}


bool BufferInStream::canReadNBytes(size_t N) const
{
	return ((read_index + N) <= buf.size()) && !Maths::unsignedIntAdditionWraps(read_index, N);
}


void BufferInStream::setReadIndex(size_t i)
{
	if(i > buf.size())
		throw glare::Exception("Invalid read index for setReadIndex().");
	read_index = i;
}


void BufferInStream::advanceReadIndex(size_t n)
{
	if(!canReadNBytes(n)) // Does wrapping/overflow check and checks against file size.
		throw glare::Exception("Invalid number of bytes to advance for advanceReadIndex - read past end of file.");
	read_index += n;
}
