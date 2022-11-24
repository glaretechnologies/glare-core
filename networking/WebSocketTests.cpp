/*=====================================================================
WebSocketTests.cpp
------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "WebSocketTests.h"


#if BUILD_TESTS


#include "WebSocket.h"
#include "TestSocket.h"
#include "MyThread.h"
#include "Networking.h"
#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/SocketBufferOutStream.h"
#include <cstring>
#include <ContainerUtils.h>



#if 0
// Command line:
// C:\fuzz_corpus\websocket -max_len=1000000

// We will use the '!' character to break apart the input buffer into different 'packets'.
// '|' finishes input data and starts specifying read lengths.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
	try
	{
		// Insert some data into the buffers of a test socket.
		TestSocketRef test_socket = new TestSocket();

		std::vector<uint8> buf;
		buf.reserve(2048);

		size_t i = 0;
		while(i < size)
		{
			if(data[i] == '!')
			{
				// Break
				test_socket->buffers.push_back(buf);
				buf.resize(0);
			}
			else if(data[i] == '|')
			{
				break;
			}
			else
			{
				buf.push_back(data[i]);
			}

			i++;
		}

		if(!buf.empty())
			test_socket->buffers.push_back(buf);

		// Create a websocket, using the testsocket as an underlying socket.
		WebSocketRef web_socket = new WebSocket(test_socket);

		//buf.resize(1024);
		if(i == size)
		{
			const size_t read_size = 1024;
			buf.resize(read_size);
			web_socket->readData(buf.data(), read_size);
		}
		else
		{
			while(i < size)
			{
				const size_t read_size = (size_t)data[i] * 10;
				buf.resize(read_size);
				web_socket->readData(buf.data(), read_size);

				//conPrint("completed read of " + toString(read_size));
			}
		}
	}
	catch(glare::Exception&)
	{
	}

	return 0;  // Non-zero return values are reserved for future use.
}
#endif


static void appendByte(std::vector<uint8>& s, uint8 byte)
{
	s.resize(s.size() + 1);
	std::memcpy(&s[s.size() - 1], &byte, 1);
}


static WebSocketRef makeWebSocketWithFramesWithPayloadLenN(size_t n, bool masking, int num_frame_copies, size_t fill_pattern_modulus)
{
	testAssert(fill_pattern_modulus <= 256);

	std::vector<uint8> query;

	// Add websocket frame
	appendByte(query, 0x80 | 0x1); // Fin | text opcode

	const uint8 mask_bit = masking ? 0x80 : 0x0;
	if(n <= 125)
	{
		appendByte(query, mask_bit | (uint8)n); // Mask bit | payload len
	}
	else if(n <= 65536)
	{
		appendByte(query, mask_bit | (uint8)126); // Mask bit | payload len
		appendByte(query, (uint8)(n >> 8));
		appendByte(query, (uint8)(n & 0xFF));
	}
	else
	{
		appendByte(query, mask_bit | (uint8)127); // Mask bit | payload len
		//appendByte(query, (uint8)(n >> 8));
		//appendByte(query, (uint8)(n & 0xFF));

		for(int i = 0; i < 8; ++i)
			appendByte(query, (uint8)((n >> (8 * (7 - i))) & 0xFF));
	}

	// Write masking key
	if(masking)
	{
		appendByte(query, 0);
		appendByte(query, 1);
		appendByte(query, 2);
		appendByte(query, 3);
	}

	// Add payload
	const size_t sz = query.size();
	query.resize(query.size() + n);
	for(size_t i = 0; i < n; ++i)
		query[sz + i] = (uint8)(i % fill_pattern_modulus);

	std::vector<uint8> total_query;
	for(int i = 0; i < num_frame_copies; ++i)
		ContainerUtils::append(total_query, query);

	//testConnectAndRequestWithPacketBreak(total_query, num_bytes_in_first_packet);

	TestSocketRef test_socket = new TestSocket();
	//test_socket->buffers.resize(2);
	test_socket->buffers.push_back(total_query);


	WebSocketRef web_socket = new WebSocket(test_socket);
	return web_socket;
}



void WebSocketTests::test()
{
	conPrint("WebSocketTests::test()");

	const size_t fill_pattern_modulus = 200;
	// Test reading a single message with a single read
	/*
	| header |          payload                  |
	0        2                                   34
			 |-----------------------------------|
			             buf read   
	*/
	{
		WebSocketRef sock = makeWebSocketWithFramesWithPayloadLenN(32, /*masking=*/false, /*num_frame_copies=*/1, fill_pattern_modulus);

		std::vector<uint8> buffer(32);
		sock->readData(buffer.data(), 32);
		
		for(size_t i = 0; i < buffer.size(); ++i)
			testAssert(buffer[i] == i % fill_pattern_modulus);
	}

	// Test again with masking
	{
		WebSocketRef sock = makeWebSocketWithFramesWithPayloadLenN(32, /*masking=*/true, /*num_frame_copies=*/1, fill_pattern_modulus);

		std::vector<uint8> buffer(32);
		sock->readData(buffer.data(), 32);

		for(size_t i = 0; i < buffer.size(); ++i)
		{
			const size_t mask = i % 4;
			testAssert(buffer[i] == ((i % fill_pattern_modulus) ^ mask));
		}
	}

	// Test a message with zero payload length
	{
		WebSocketRef sock = makeWebSocketWithFramesWithPayloadLenN(0, /*masking=*/true, /*num_frame_copies=*/1, fill_pattern_modulus);

		try
		{
			std::vector<uint8> buffer(10);
			sock->readData(buffer.data(), 10);
		}
		catch(glare::Exception& )
		{
			// expected
		}
	}

	for(size_t n=0; n<10000; ++n)
	{
		WebSocketRef sock = makeWebSocketWithFramesWithPayloadLenN(n, /*masking=*/true, /*num_frame_copies=*/1, fill_pattern_modulus);

		std::vector<uint8> buffer(n);
		sock->readData(buffer.data(), n);

		for(size_t i = 0; i < buffer.size(); ++i)
		{
			const size_t mask = i % 4;
			testAssert(buffer[i] == ((i % fill_pattern_modulus) ^ mask));
		}
	}


	// Test reading multiple reads from a single message
	/*
	| header |          payload                  |
	0        2          12                       34
	         |----------|----------|----------|--|
	           buf read     buf read    buf read  buf read
	*/
	{
		WebSocketRef sock = makeWebSocketWithFramesWithPayloadLenN(32, /*masking=*/false, /*num_frame_copies=*/1, fill_pattern_modulus);

		std::vector<uint8> buffer(32);
		sock->readData(buffer.data() + 0, 10);
		sock->readData(buffer.data() + 10, 10);
		sock->readData(buffer.data() + 20, 10);
		sock->readData(buffer.data() + 30, 2);

		for(size_t i = 0; i < buffer.size(); ++i)
			testAssert(buffer[i] == i % fill_pattern_modulus);
	}


	// Test reading multiple message payloads (2 messages) in a single read
	// Test reading multiple reads from a single message
	/*
	| header |          payload                  | header |          payload                  |
	0        2          12                       34
	         |-----------------------------------|....    |-----------------------------------|
	           buf read                                       buf read (continued)
	*/
	{
		WebSocketRef sock = makeWebSocketWithFramesWithPayloadLenN(32, /*masking=*/false, /*num_frame_copies=*/2, fill_pattern_modulus);

		std::vector<uint8> buffer(64);
		sock->readData(buffer.data() + 0, 64);
		
		for(size_t i = 0; i < 32; ++i)
			testAssert(buffer[i] == i % fill_pattern_modulus);
		for(size_t i = 0; i < 32; ++i)
			testAssert(buffer[32 + i] == i % fill_pattern_modulus);
	}

	// Test reading multiple message payloads (2 messages) in a single read, with offsets
	/*
	| header |          payload                  | header |          payload                  |
	0        2          12                       34
	         |----------|------------------------|....    |--------------|--------------------|
	           buf read     buf read                   buf read (continued)   buf read
	*/
	{
		WebSocketRef sock = makeWebSocketWithFramesWithPayloadLenN(32, /*masking=*/false, /*num_frame_copies=*/2, fill_pattern_modulus);

		std::vector<uint8> buffer(64);
		sock->readData(buffer.data(), 10);

		sock->readData(buffer.data() + 10, 40);

		sock->readData(buffer.data() + 50, 14);

		for(size_t i = 0; i < 32; ++i)
			testAssert(buffer[i] == i % fill_pattern_modulus);
		for(size_t i = 0; i < 32; ++i)
			testAssert(buffer[32 + i] == i % fill_pattern_modulus);
	}


	// Test some large messages, that will trigger the chunking code (2048 bytes)
	{
		const size_t N = 10000;
		WebSocketRef sock = makeWebSocketWithFramesWithPayloadLenN(N, /*masking=*/false, /*num_frame_copies=*/1, fill_pattern_modulus);

		std::vector<uint8> buffer(N);
		sock->readData(buffer.data(), N);

		for(size_t i = 0; i < buffer.size(); ++i)
			testAssert(buffer[i] == i % fill_pattern_modulus);
	}

	//Test with masking
	{
		const size_t N = 10000;
		WebSocketRef sock = makeWebSocketWithFramesWithPayloadLenN(N, /*masking=*/true, /*num_frame_copies=*/1, fill_pattern_modulus);

		std::vector<uint8> buffer(N);
		sock->readData(buffer.data(), N);

		for(size_t i = 0; i < buffer.size(); ++i)
		{
			const size_t mask = i % 4;
			testAssert(buffer[i] == ((i % fill_pattern_modulus) ^ mask));
		}
	}


	// Test with a message that uses the largest payload size encoding, that will trigger the chunking code (2048 bytes)
	{
		const size_t N = 100000;
		WebSocketRef sock = makeWebSocketWithFramesWithPayloadLenN(N, /*masking=*/false, /*num_frame_copies=*/1, fill_pattern_modulus);

		std::vector<uint8> buffer(N);
		sock->readData(buffer.data(), N);

		for(size_t i = 0; i < buffer.size(); ++i)
			testAssert(buffer[i] == i % fill_pattern_modulus);
	}

	// Test with a message that uses the largest payload size encoding, that will trigger the chunking code (2048 bytes). 
	// Test with masking
	{
		const size_t N = 100000;
		WebSocketRef sock = makeWebSocketWithFramesWithPayloadLenN(N, /*masking=*/true, /*num_frame_copies=*/1, fill_pattern_modulus);

		std::vector<uint8> buffer(N);
		sock->readData(buffer.data(), N);

		for(size_t i = 0; i < buffer.size(); ++i)
		{
			const size_t mask = i % 4;
			testAssert(buffer[i] == ((i % fill_pattern_modulus) ^ mask));
		}
	}


	//--------------------------- Test some writes ------------------------------
	{
		for(int n=0; n<200000; n = (n + 1) * 2)
		{
			conPrint("testing write with n=" + toString(n));

			std::vector<uint8> data(n);
		
			for(int i=0; i<n; ++i)
				data[i] = i % fill_pattern_modulus;

			
			TestSocketRef test_socket = new TestSocket();

			WebSocketRef web_socket = new WebSocket(test_socket);
			web_socket->writeData(data.data(), data.size());
			web_socket->flush();
			web_socket->flush(); // Test flush again with no pending data.

			size_t header_size;
			if(n <= 125)
				header_size = 2;
			else if(n <= 65536)
				header_size = 4;
			else
				header_size = 10;

			if(n == 0)
			{
				testAssert(test_socket->dest_buffers.size() == 0); // No frames/messages written if payload size = 0.
			}
			else
			{
				testAssert(test_socket->dest_buffers.size() == 2);

				const std::vector<uint8>& header_buf = test_socket->dest_buffers[0];
				testAssert(header_buf.size() == header_size);

				const std::vector<uint8>& payload_buf = test_socket->dest_buffers[1];
				testAssert((int)payload_buf.size() == n);
				for(int i=0; i<n; ++i)
					testAssert(payload_buf[i] == i % fill_pattern_modulus);
			}
		}
	}


	conPrint("WebSocketTests::test(): done.");
}


#endif // BUILD_TESTS
