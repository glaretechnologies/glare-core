/*=====================================================================
TestSocket.cpp
--------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "TestSocket.h"


#include "../maths/mathstypes.h"
#include "../utils/BitUtils.h"
#include "../utils/Exception.h"
#include "MySocket.h"
#if !defined(_WIN32)
#include <netinet/in.h>
#endif


TestSocket::TestSocket()
:	read_i(0),
	use_network_byte_order(true)
{}


size_t TestSocket::readSomeBytes(void* buffer, size_t max_num_bytes)
{
	// Advance to next buffer we have not completely read.
	while(1)
	{
		if(buffers.empty())
			return 0;

		if(read_i == buffers.front().size())
		{
			buffers.pop_front();
			read_i = 0;
		}
		else
		{
			break;
		}
	}
	assert(!buffers.empty() && read_i < buffers.front().size());

	// read/copy some bytes from the front buffer
	const size_t read_amount = myMin(buffers.front().size() - read_i, max_num_bytes);
	std::memcpy(buffer, &buffers.front()[read_i], read_amount);
		
	read_i += read_amount;
		
	return read_amount;
}


void TestSocket::readTo(void* buffer, size_t readlen)
{
	readTo(buffer, readlen, NULL);
}


void TestSocket::readTo(void* buffer, size_t readlen, FractionListener* /*frac*/)
{
	while(readlen > 0) // While still bytes to read
	{
		const size_t numbytestoread = readlen;
		assert(numbytestoread > 0);

		// Advance to next buffer we have not completely read.
		while(1)
		{
			if(buffers.empty())
				throw glare::Exception("Connection Closed.");

			if(read_i == buffers.front().size())
			{
				buffers.pop_front();
				read_i = 0;
			}
			else
			{
				break;
			}
		}
		assert(!buffers.empty() && read_i < buffers.front().size());

		// read/copy some bytes from the front buffer
		const size_t read_amount = myMin(buffers.front().size() - read_i, numbytestoread);
		std::memcpy(buffer, &buffers.front()[read_i], read_amount);
		
		read_i += read_amount;
		
		readlen -= read_amount;
		buffer = (void*)((char*)buffer + read_amount);
	}
}


void TestSocket::writeInt32(int32 /*x*/)
{
	assert(0);
}


void TestSocket::writeUInt32(uint32 /*x*/)
{
	assert(0);
}


int TestSocket::readInt32()
{
	uint32 i;
	readTo(&i, sizeof(uint32));
	if(use_network_byte_order)
		i = ntohl(i);
	return bitCast<int32>(i);
}


uint32 TestSocket::readUInt32()
{
	uint32 x;
	readTo(&x, sizeof(uint32));
	if(use_network_byte_order)
		x = ntohl(x);
	return x;
}


void TestSocket::readData(void* buf, size_t num_bytes)
{
	readTo(buf, num_bytes, 
		NULL // fraction listener
	);
}


bool TestSocket::endOfStream()
{
	return false;
}


void TestSocket::writeData(const void* data, size_t num_bytes)
{
	std::vector<uint8> buf((const uint8*)data, (const uint8*)data + num_bytes);
	dest_buffers.push_back(buf);
}


#if BUILD_TESTS


#include "../utils/ConPrint.h"
#include "../utils/TestUtils.h"


void TestSocket::test()
{
	// Test readSomeBytes()
	try
	{
		TestSocket test_socket;
		std::vector<uint8> v;
		v.push_back(1);
		v.push_back(2);
		v.push_back(3);
		test_socket.buffers.push_back(v);
		std::vector<uint8> v2;
		v2.push_back(11);
		v2.push_back(12);
		v2.push_back(13);
		test_socket.buffers.push_back(v2);

		std::vector<uint8> buf(1024, 0);
		testAssert(test_socket.readSomeBytes(buf.data(), 1024) == 3); // Should read 3 bytes
		testAssert(buf[0] == 1);
		testAssert(buf[1] == 2);
		testAssert(buf[2] == 3);
		testAssert(test_socket.readSomeBytes(buf.data(), 1024) == 3); // Should read 3 bytes
		testAssert(buf[0] == 11);
		testAssert(buf[1] == 12);
		testAssert(buf[2] == 13);
		testAssert(test_socket.readSomeBytes(buf.data(), 1024) == 0); // Should read 0 bytes (EOF)
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	// Test readData()
	try
	{
		TestSocket test_socket;
		std::vector<uint8> v;
		v.push_back(1);
		v.push_back(2);
		v.push_back(3);
		test_socket.buffers.push_back(v);
		std::vector<uint8> v2;
		v2.push_back(11);
		v2.push_back(12);
		v2.push_back(13);
		test_socket.buffers.push_back(v2);

		std::vector<uint8> buf(1024, 0);
		test_socket.readData(buf.data(), 1);
		testAssert(buf[0] == 1);
		test_socket.readData(buf.data(), 1);
		testAssert(buf[0] == 2);

		test_socket.readData(buf.data(), 3); // Read last byte from first buf, 2 bytes from next buf
		testAssert(buf[0] == 3);
		testAssert(buf[1] == 11);
		testAssert(buf[2] == 12);

		test_socket.readData(buf.data(), 1); // read last byte
		testAssert(buf[0] == 13);
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


#endif // BUILD_TESTS
