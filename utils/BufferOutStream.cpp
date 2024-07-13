/*=====================================================================
BufferOutStream.cpp
-------------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "BufferOutStream.h"


#include "Exception.h"
#include <cstring> // For std::memcpy


BufferOutStream::BufferOutStream()
{
}


BufferOutStream::~BufferOutStream()
{
}


void BufferOutStream::writeUInt8(uint8 x)
{
	try
	{
		const size_t pos = buf.size(); // Get position to write to (also current size of buffer)
		buf.resize(pos + sizeof(x)); // Resize buffer to make room for new uint32
		std::memcpy(&buf[pos], &x, sizeof(x)); // Copy x to buffer.
	}
	catch(std::bad_alloc&)
	{
		throw glare::Exception("Memory alloc failure while writing to buffer.");
	}
}


void BufferOutStream::writeInt32(int32 x)
{
	try
	{
		const size_t pos = buf.size(); // Get position to write to (also current size of buffer)
		buf.resize(pos + sizeof(x)); // Resize buffer to make room for new uint32
		std::memcpy(&buf[pos], &x, sizeof(x)); // Copy x to buffer.
	}
	catch(std::bad_alloc&)
	{
		throw glare::Exception("Memory alloc failure while writing to buffer.");
	}
}


void BufferOutStream::writeUInt32(uint32 x)
{
	try
	{
		const size_t pos = buf.size(); // Get position to write to (also current size of buffer)
		buf.resize(pos + sizeof(x)); // Resize buffer to make room for new uint32
		std::memcpy(&buf[pos], &x, sizeof(x)); // Copy x to buffer.
	}
	catch(std::bad_alloc&)
	{
		throw glare::Exception("Memory alloc failure while writing to buffer.");
	}
}


void BufferOutStream::writeData(const void* data, size_t num_bytes)
{
	if(num_bytes > 0)
	{
		try
		{
			const size_t pos = buf.size(); // Get position to write to (also current size of buffer)
			buf.resize(pos + num_bytes); // Resize buffer to make room for new data
			std::memcpy(&buf[pos], data, num_bytes); // Copy data to buffer.
		}
		catch(std::bad_alloc&)
		{
			throw glare::Exception("Memory alloc failure while writing to buffer.");
		}
	}
}


void* BufferOutStream::getWritePtrAtIndex(size_t i)
{
	if(i >= buf.size())
		throw glare::Exception("getWritePtrAtIndex called with an index past end of buffer.");
	return &buf[i];
}
