/*=====================================================================
SocketBufferOutStream.cpp
-------------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-01-27 17:56:55 +0000
=====================================================================*/
#include "SocketBufferOutStream.h"


#include "Exception.h"
#include "BitUtils.h"
#include <cstring> // For std::memcpy

// For htonl:
#if defined(_WIN32)
// Stop windows.h from defining the min() and max() macros
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock.h>
#else
#include <netinet/in.h>
#endif


SocketBufferOutStream::SocketBufferOutStream(UseNetworkByteOrder use_network_byte_order_)
:	use_network_byte_order(use_network_byte_order_ == DoUseNetworkByteOrder)
{
}


SocketBufferOutStream::~SocketBufferOutStream()
{
}


void SocketBufferOutStream::writeData(const void* data, size_t num_bytes)
{
	try
	{
		const size_t pos = buf.size(); // Get position to write to (also current size of buffer)
		buf.resize(pos + num_bytes); // Resize buffer to make room for new data
		if(num_bytes > 0)
			std::memcpy(&buf[pos], data, num_bytes); // Copy data to buffer.
	}
	catch(std::bad_alloc&)
	{
		throw glare::Exception("Memory alloc failure while writing to buffer.");
	}
}


void SocketBufferOutStream::writeInt32(int32 x_)
{
	try
	{
		uint32 ux = bitCast<uint32>(x_);

		if(use_network_byte_order)
			ux = htonl(ux);

		const size_t pos = buf.size(); // Get position to write to (also current size of buffer)
		buf.resize(pos + sizeof(ux)); // Resize buffer to make room for new uint32
		std::memcpy(&buf[pos], &ux, sizeof(ux)); // Copy x to buffer.
	}
	catch(std::bad_alloc&)
	{
		throw glare::Exception("Memory alloc failure while writing to buffer.");
	}
}


void SocketBufferOutStream::writeUInt32(uint32 x)
{
	try
	{
		if(use_network_byte_order)
			x = htonl(x);

		const size_t pos = buf.size(); // Get position to write to (also current size of buffer)
		buf.resize(pos + sizeof(x)); // Resize buffer to make room for new uint32
		std::memcpy(&buf[pos], &x, sizeof(x)); // Copy x to buffer.
	}
	catch(std::bad_alloc&)
	{
		throw glare::Exception("Memory alloc failure while writing to buffer.");
	}
}


void SocketBufferOutStream::writeUInt64(uint64 x)
{
	if(use_network_byte_order)
	{
		uint32 a, b;
		std::memcpy(&a, (uint8*)&x + 0, sizeof(uint32));
		std::memcpy(&b, (uint8*)&x + 4, sizeof(uint32));

		writeUInt32(a);
		writeUInt32(b);
	}
	else
	{
		const size_t pos = buf.size(); // Get position to write to (also current size of buffer)
		buf.resize(pos + sizeof(uint64)); // Resize buffer to make room for new uint64
		std::memcpy(&buf[pos], &x, sizeof(uint64)); // Copy x to buffer.
	}
}
