/*=====================================================================
WebWorkerThreadTests.cpp
------------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "WebWorkerThreadTests.h"


#if BUILD_TESTS


#include "WebWorkerThread.h"
#include "TestUtils.h"
#include "WebListenerThread.h"
#include "ResponseUtils.h"
#include "RequestInfo.h"
#include "WebsiteExcep.h"
#include "Escaping.h"
#include "RequestHandler.h"
#include <maths/mathstypes.h>
#include <ConPrint.h>
#include <Clock.h>
#include <AESEncryption.h>
#include <SHA256.h>
#include <Base64.h>
#include <Exception.h>
#include <networking/MySocket.h>
#include <networking/HTTPClient.h>
#include <Lock.h>
#include <StringUtils.h>
#include <PlatformUtils.h>
#include <networking/TestSocket.h>
#include <KillThreadMessage.h>
#include <Parser.h>
#include <MemMappedFile.h>
#include <maths/PCG32.h>


namespace web
{


static const std::string CRLF = "\r\n";
static const std::string CRLFCRLF = "\r\n\r\n";


class TestRequestHandler : public RequestHandler
{
public:
	TestRequestHandler() : num_requests_handled(0) {}

	virtual void handleRequest(const RequestInfo& request_info, ReplyInfo& reply_info)
	{
		if(request_info.ranges.empty())
		{
			const std::string message = "ping";
			reply_info.socket->writeData(message.c_str(), message.size());
		}
		else
		{
			// Resource is present, send it
			try
			{
				const std::string local_path = TestUtils::getTestReposDir() + "/testfiles/antialias_test3.png"; // 1,351,819 B

				const std::string content_type = web::ResponseUtils::getContentTypeForPath(local_path); // Guess content type

				MemMappedFile file(local_path);

				// NOTE: only handle a single range for now, because the response content types (and encoding?) get different for multiple ranges.
				if(request_info.ranges.size() == 1)
				{
					for(size_t i=0; i<request_info.ranges.size(); ++i)
					{
						const web::Range range = request_info.ranges[i];
						if(range.start < 0 || range.start >= (int64)file.fileSize())
							throw glare::Exception("invalid range");

						int64 range_size;
						if(range.end_incl == -1) // if this range is just to the end:
							range_size = (int64)file.fileSize() - range.start;
						else
						{
							if(range.start > range.end_incl)
								throw glare::Exception("invalid range");
							range_size = range.end_incl - range.start + 1;
						}

						const int64 use_range_end = range.start + range_size;
						if(use_range_end > (int64)file.fileSize())
							throw glare::Exception("invalid range");

						conPrint("\thandleResourceRequest: serving data range (start: " + toString(range.start) + ", range_size: " + toString(range_size) + ")");

						const std::string response = 
							"HTTP/1.1 206 Partial Content\r\n"
							"Content-Type: " + content_type + "\r\n"
							"Content-Range: bytes " + toString(range.start) + "-" + toString(use_range_end - 1) + "/" + toString(file.fileSize()) + "\r\n" // Note that ranges are inclusive, hence the - 1.
							"Connection: Keep-Alive\r\n"
							"Content-Length: " + toString(range_size) + "\r\n"
							"\r\n";

						reply_info.socket->writeData(response.c_str(), response.size());

						// Sanity check range.start and range_size.  Should be valid by here.
						assert((range.start >= 0) && (range.start <= (int64)file.fileSize()) && (range.start + range_size <= (int64)file.fileSize()));
						if(!(range.start >= 0) && (range.start <= (int64)file.fileSize()) && (range.start + range_size <= (int64)file.fileSize()))
							throw glare::Exception("internal error computing ranges");

						reply_info.socket->writeData((const uint8*)file.fileData() + range.start, range_size);

						conPrint("\thandleResourceRequest: sent data range. (len: " + toString(range_size) + ")");
					}
				}
				else
				{
					conPrint("\thandleResourceRequest: serving data (len: " + toString(file.fileSize()) + ")");

					web::ResponseUtils::writeHTTPOKHeaderAndData(reply_info, file.fileData(), file.fileSize(), content_type.c_str());

					conPrint("\thandleResourceRequest: sent data. (len: " + toString(file.fileSize()) + ")");
				}
			}
			catch(glare::Exception& e)
			{
				conPrint("Error while handling resource request: " + e.what());
				web::ResponseUtils::writeHTTPNotFoundHeaderAndData(reply_info, "resource not found.");
				return;
			}
		}
		
		num_requests_handled++;
	}

	int num_requests_handled;
};


class TestSharedRequestHandler : public SharedRequestHandler
{
public:
	virtual Reference<RequestHandler> getOrMakeRequestHandler() // Factory method for request handler.
	{
		return new TestRequestHandler();
	}
};


static const int port = 666;


static void testPacketBreaksWithRequest(const std::string& request, int expected_num_requests)
{
	conPrint("testPacketBreaksWithRequest(), request='" + request + "'");
	PCG32 rng(1);
	//for(int split_i=0; i<split_i; ++i)
	for(int i=0; i<100; ++i)
	{
		TestSocketRef test_socket = new TestSocket();

		int64 last_break_i = 0;
		for(int64 break_i=1 + (int64)(rng.unitRandom() * 10); break_i<(int64)request.size(); )
		{
			std::vector<uint8> buf(break_i - last_break_i);
			std::memcpy(buf.data(), &request[last_break_i], break_i - last_break_i);
			test_socket->buffers.push_back(buf);

			last_break_i = break_i;
			break_i += 1 + (int64)(rng.unitRandom() * 10);
		}
		// Append final bytes
		{
			std::vector<uint8> buf((int64)request.size() - last_break_i);
			std::memcpy(buf.data(), &request[last_break_i], (int64)request.size() - last_break_i);
			test_socket->buffers.push_back(buf);
		}
		
		Reference<TestRequestHandler> request_handler = new TestRequestHandler();
		Reference<web::WorkerThread> worker = new web::WorkerThread(0, test_socket, request_handler, /*tls connection=*/false);
		//ThreadManager thread_manager;
		//thread_manager.addThread(worker);
		//thread_manager.killThreadsBlocking();

		worker->doRun();

		testAssert(request_handler->num_requests_handled == expected_num_requests);
	}
}




static void testConnectAndRequest(const std::string& request)
{
	

	try
	{
		Reference<MySocket> socket = new MySocket("localhost", port);

		socket->write(request.c_str(), request.size(), NULL);
	}
	catch(MySocketExcep& )
	{}
}


static void testConnectAndRequestWithPacketBreak(const std::string& request, size_t num_bytes_in_first_packet)
{
	try
	{
		Reference<MySocket> socket = new MySocket("localhost", port);

		socket->setNoDelayEnabled(true);

		socket->write(request.c_str(), myMin(num_bytes_in_first_packet, request.size()), NULL);

		// Send rest of data
		if(request.size() > num_bytes_in_first_packet)
		{
			PlatformUtils::Sleep(1);
			socket->write(request.c_str() + num_bytes_in_first_packet, request.size() - num_bytes_in_first_packet, NULL);
		}
	}
	catch(MySocketExcep& )
	{}
}


static void testPost(const std::string& request, const std::string& content)
{
	try
	{
		Reference<MySocket> socket = new MySocket("localhost", port);

		socket->write(request.c_str(), request.size(), NULL);

		if(content.size() > 0)
		{
			/*std::vector<uint8> data(buffer_size_to_send);
			for(size_t i=0; i<data.size(); ++i)
				data[i] = i % 256;*/

			// Write the post content
			socket->write(&content[0], content.size(), NULL);
		}
	}
	catch(MySocketExcep& )
	{}
}


static void testPost(const std::string& content)
{
	try
	{
		Reference<MySocket> socket = new MySocket("localhost", port);

		const std::string request = "POST / HTTP/1.1" + CRLF + "Content-Length: " + toString(content.size()) + CRLFCRLF;
		socket->write(request.c_str(), request.size(), NULL);

		if(content.size() > 0)
		{
			socket->write(&content[0], content.size(), NULL);
		}
	}
	catch(MySocketExcep& )
	{}
}


/*static void testRequest(Reference<MySocket>& socket, const std::string& request)
{
	socket->write(request.c_str(), request.size(), NULL, NULL);
}*/


static void testRequestAndResponse(Reference<MySocket>& socket, const std::string& request, const std::string& target_response)
{
	socket->write(request.c_str(), request.size(), NULL);

	// Read response:
	char buf[100000];
	size_t total_num_bytes_read = 0;
	while(total_num_bytes_read < target_response.size())
	{
		const size_t num_bytes_read = socket->readSomeBytes(buf + total_num_bytes_read, sizeof(buf) - total_num_bytes_read);
		if(num_bytes_read == 0)
			failTest("connection was closed.");
		total_num_bytes_read += num_bytes_read;
	}

	const std::string response(buf, buf + total_num_bytes_read);
	conPrint("Response: ");
	conPrint(response);

	testAssert(response == target_response);
}


static void testRangeRequestAndResponse(web::Range range, const MemMappedFile& file, bool expect_206_response)
{
	try
	{
		HTTPClient client;
		client.setAsNotIndependentlyHeapAllocated();
		
		std::string range_header;
		range_header = "range: bytes=" + toString(range.start) + "-";
		if(range.end_incl!= -1)
			range_header += toString(range.end_incl);

		client.additional_headers.push_back(range_header);

		std::vector<uint8> response;
		HTTPClient::ResponseInfo info = client.downloadFile("http://localhost:" + toString(port) + "/", response);
		if(expect_206_response)
			testAssert(info.response_code == 206);
		else
			testAssert(info.response_code == 404);

		if(info.response_code == 206)
		{
			const int64 range_size = (range.end_incl == -1) ? file.fileSize() - range.start : range.end_incl - range.start + 1;

			testAssert((int64)response.size() == range_size);

			for(int64 i=range.start; i<range.start + range_size; ++i)
				testAssert(((const char*)file.fileData())[i] == response[i - range.start]);
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


static void appendByte(std::string& s, uint8 byte)
{
	s.resize(s.size() + 1);
	std::memcpy(&s[s.size() - 1], &byte, 1);
}

static void testWebsocketFramesWithDataSizeN(int n, bool masking, size_t num_bytes_in_first_packet, int num_frame_repititions)
{
	assert(n < (1 << 16));
	const std::string http_query = "GET / HTTP/1.1" + CRLF + "Sec-WebSocket-Key: bleh" + CRLFCRLF;
	std::string query;

	// Add websocket frame
	appendByte(query, 0x80 | 0x1); // Fin | text opcode

	const uint8 mask_bit = masking ? 0x80 : 0x0;
	if(n <= 125)
	{
		appendByte(query, mask_bit | (uint8)n); // Mask bit | payload len
	}
	else
	{
		appendByte(query, mask_bit | (uint8)126); // Mask bit | payload len
		appendByte(query, (uint8)(n >> 8));
		appendByte(query, (uint8)(n & 0xFF));
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
	for(int i=0; i<n; ++i)
		query[sz + i] = 0;

	std::string total_query = http_query;
	for(int i=0; i<num_frame_repititions; ++i)
		total_query += query;

	testConnectAndRequestWithPacketBreak(total_query, http_query.size() + num_bytes_in_first_packet);
}


static void testRangeParsing(const std::string& field_value, const std::vector<web::Range>& expected_ranges)
{
	std::vector<web::Range> ranges;
	WorkerThread::parseRanges(field_value, ranges);
	testAssert(ranges == expected_ranges);
}


static void testAcceptEncodingParsing(const std::string& field_value, bool expected_deflate_accept_encoding, bool expected_zstd_accept_encoding)
{
	bool deflate_accept_encoding, zstd_accept_encoding;
	WorkerThread::parseAcceptEncodings(field_value, deflate_accept_encoding, zstd_accept_encoding);
	testAssert(deflate_accept_encoding == expected_deflate_accept_encoding);
	testAssert(zstd_accept_encoding == expected_zstd_accept_encoding);
}



class TestDummyRequestHandler : public RequestHandler
{
public:
	TestDummyRequestHandler() {}

	virtual void handleRequest(const RequestInfo& request_info, ReplyInfo& reply_info)
	{}
};


#if 0 // FUZZING

// Direct fuzzing of WorkerThread::handleSingleRequest()
// Command line:
// C:\fuzz_corpus\worker_thread_handle_single_request c:\code\glare-core\testfiles\fuzz_seeds\worker_thread_handle_single_request -max_len=1000000 -seed=1

static void testHandleSingleRequest(const uint8_t* data, size_t size)
{
	try
	{
		Reference<TestDummyRequestHandler> request_handler = new TestDummyRequestHandler();
		TestSocketRef test_socket = new TestSocket();
		Reference<web::WorkerThread> worker = new web::WorkerThread(0, test_socket, request_handler, /*tls connection=*/false);

		// Scan through for a double CRLF
		size_t request_header_size = std::numeric_limits<size_t>::max();
		for(size_t i=0; i+3<size; ++i)
		{
			if(data[i] == '\r' && data[i+1] == '\n' && data[i+2] == '\r' && data[i+3] == '\n')
			{
				request_header_size = i + 4;
				break;
			}
		}

		if(request_header_size == std::numeric_limits<size_t>::max()) // If double CRLF not found:
		{
			// Insert a double CRLF on the end manually
			worker->socket_buffer.resize(size + 4);
			request_header_size = size + 4;

			// Copy the fuzz input data
			std::memcpy(worker->socket_buffer.data(), data, size);

			// Add the CRLFCRLF
			worker->socket_buffer[size+0] = '\r';
			worker->socket_buffer[size+1] = '\n';
			worker->socket_buffer[size+2] = '\r';
			worker->socket_buffer[size+3] = '\n';
		}
		else
		{
			worker->socket_buffer.resize(size);
			std::memcpy(worker->socket_buffer.data(), data, size);
		}
		
		worker->request_start_index = 0;
		worker->handleSingleRequest(request_header_size);
	}
	catch(glare::Exception&)
	{
	}
}

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	testHandleSingleRequest(data, size);

	return 0;  // Non-zero return values are reserved for future use.
}
#endif


#if 0

// Fuzzing of WorkerThread::doRunMainLoop()
// Command line:
// C:\fuzz_corpus\worker_thread -max_len=1000000 -seed=1

// We will use the '!' character to break apart the input buffer into different 'packets'.
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
	try
	{
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
			else
			{
				buf.push_back(data[i]);
			}

			i++;
		}

		if(!buf.empty())
			test_socket->buffers.push_back(buf);

		
		Reference<TestDummyRequestHandler> request_handler = new TestDummyRequestHandler();
		Reference<web::WorkerThread> worker = new web::WorkerThread(0, test_socket, request_handler, /*tls connection=*/false);
		
		worker->doRunMainLoop();
	}
	catch(glare::Exception&)
	{
	}
	
	return 0;  // Non-zero return values are reserved for future use.
}
#endif


void WebWorkerThreadTests::test()
{
	//=========================== Test accept-encoding parsing ===============================
	{
		testAcceptEncodingParsing("", /*expected_deflate_accept_encoding=*/false, /*expected_zstd_accept_encoding=*/false);
		testAcceptEncodingParsing("deflate", /*expected_deflate_accept_encoding=*/true, /*expected_zstd_accept_encoding=*/false);
		testAcceptEncodingParsing("zstd", /*expected_deflate_accept_encoding=*/false, /*expected_zstd_accept_encoding=*/true);
		testAcceptEncodingParsing("deflate,zstd", /*expected_deflate_accept_encoding=*/true, /*expected_zstd_accept_encoding=*/true);
		testAcceptEncodingParsing("deflate, zstd", /*expected_deflate_accept_encoding=*/true, /*expected_zstd_accept_encoding=*/true);
		testAcceptEncodingParsing("deflate,  zstd", /*expected_deflate_accept_encoding=*/true, /*expected_zstd_accept_encoding=*/true);
		testAcceptEncodingParsing("deflate ,  zstd", /*expected_deflate_accept_encoding=*/true, /*expected_zstd_accept_encoding=*/true);
		testAcceptEncodingParsing("deflate ,zstd", /*expected_deflate_accept_encoding=*/true, /*expected_zstd_accept_encoding=*/true);

		// Test quality stuff (see https://www.rfc-editor.org/rfc/rfc9110#field.accept-encoding)
		testAcceptEncodingParsing("deflate;q=1.0", /*expected_deflate_accept_encoding=*/true, /*expected_zstd_accept_encoding=*/false);
		testAcceptEncodingParsing("deflate;q=1.0, zstd;q=0", /*expected_deflate_accept_encoding=*/true, /*expected_zstd_accept_encoding=*/true);

		testAcceptEncodingParsing("abc", /*expected_deflate_accept_encoding=*/false, /*expected_zstd_accept_encoding=*/false);
		testAcceptEncodingParsing("abc, zstd", /*expected_deflate_accept_encoding=*/false, /*expected_zstd_accept_encoding=*/true);
		testAcceptEncodingParsing("abc, deflate", /*expected_deflate_accept_encoding=*/true, /*expected_zstd_accept_encoding=*/false);
	}


	//=========================== Test range parsing ===============================
	{
		// See https://developer.mozilla.org/en-US/docs/Web/HTTP/Headers/Range

		testRangeParsing("", std::vector<web::Range>());
		testRangeParsing("bleh", std::vector<web::Range>());
		testRangeParsing("bytes", std::vector<web::Range>());
		testRangeParsing("bytes=", std::vector<web::Range>());
		testRangeParsing("bytes=,", std::vector<web::Range>());
		testRangeParsing("bytes=-123", std::vector<web::Range>());
		testRangeParsing("bytes=123", std::vector<web::Range>());
		testRangeParsing("bytes=123-", std::vector<web::Range>(1, web::Range(123, -1)));
		testRangeParsing("bytes=123-456", std::vector<web::Range>(1, web::Range(123, 456)));
		testRangeParsing("bytes=123-456 ", std::vector<web::Range>(1, web::Range(123, 456)));
		testRangeParsing("bytes=123-456 ,", std::vector<web::Range>(1, web::Range(123, 456)));
		testRangeParsing("bytes=123-456,", std::vector<web::Range>(1, web::Range(123, 456)));

		{
			std::vector<web::Range> ranges(1, web::Range(123, 456));
			ranges.push_back(web::Range(1000, -1));
			testRangeParsing("bytes=123-456, 1000-", ranges);
		}
		{
			std::vector<web::Range> ranges(1, web::Range(123, 456));
			ranges.push_back(web::Range(1000, 2000));
			testRangeParsing("bytes=123-456, 1000-2000", ranges);
		}
	}

	

	//testConnectAndRequest();
	{
		testPacketBreaksWithRequest("BLEH", /*expected_num_requests=*/0);
		testPacketBreaksWithRequest("BLEH" + CRLFCRLF, 0);
		testPacketBreaksWithRequest("GET / HTTP/1.1" + CRLFCRLF, 1);
		testPacketBreaksWithRequest("GET / HTTP/1.1" + CRLFCRLF + "GET / HTTP/1.1", 1); // No trailing double CRLF on second request
		testPacketBreaksWithRequest("GET / HTTP/1.1" + CRLFCRLF + "GET / HTTP/1.1" + CRLF, 1); // No trailing double CRLF on second request
		testPacketBreaksWithRequest("GET / HTTP/1.1" + CRLFCRLF + "GET / HTTP/1.1" + CRLFCRLF, 2);
		testPacketBreaksWithRequest(CRLFCRLF, 0);
		testPacketBreaksWithRequest(CRLFCRLF + CRLFCRLF, 0);
		testPacketBreaksWithRequest(CRLFCRLF + CRLFCRLF + CRLFCRLF, 0);
	}
	
	// Create and launch a server listener thread.
	
	Reference<TestSharedRequestHandler> shared_request_handler = new TestSharedRequestHandler();

	ThreadManager thread_manager;
	Reference<WebListenerThread> listener_thread = new WebListenerThread(port, shared_request_handler.getPointer(), 
		NULL // tls_configuration
	);
	thread_manager.addThread(listener_thread);

	
	//=========================== Test range handling ===============================
	try
	{
		const std::string local_path = TestUtils::getTestReposDir() + "/testfiles/antialias_test3.png"; // 1,351,819 B
		MemMappedFile file(local_path);
		testRangeRequestAndResponse(web::Range(0, 0), file, /*expect 206 response=*/true);
		testRangeRequestAndResponse(web::Range(0, 1), file, /*expect 206 response=*/true);
		testRangeRequestAndResponse(web::Range(0, 1), file, /*expect 206 response=*/true);
		testRangeRequestAndResponse(web::Range(0, 1000), file, /*expect 206 response=*/true);
		testRangeRequestAndResponse(web::Range(1000, 2000), file, /*expect 206 response=*/true);
		testRangeRequestAndResponse(web::Range(1000000, 2000000), file, /*expect 206 response=*/false); // past end of file
		testRangeRequestAndResponse(web::Range(1000000, -1), file, /*expect 206 response=*/true);
		testRangeRequestAndResponse(web::Range(2000000, 1000000), file, /*expect 206 response=*/false); // starts past end of file, neg range
		testRangeRequestAndResponse(web::Range(2000000, 3000000), file, /*expect 206 response=*/false); // starts past end of file

	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	//=========================== Test some websocket connections ===============================
	{
		testWebsocketFramesWithDataSizeN(5, /*masking=*/false, 5000000000000, 10);
		for(size_t packet_size = 0; packet_size < 200; ++packet_size)
		{
			testWebsocketFramesWithDataSizeN(5, /*masking=*/false, /*num bytes in first packet=*/packet_size, /*num frame repititions=*/4);
			testWebsocketFramesWithDataSizeN(5, /*masking=*/true, /*num bytes in first packet=*/packet_size, /*num frame repititions=*/4);
		}

		for(int i=0; i<1000; ++i)
			testWebsocketFramesWithDataSizeN(i, /*masking=*/true, /*num bytes in first packet=*/100000, /*num frame repititions=*/4);
		for(int i=0; i<1000; ++i)
			testWebsocketFramesWithDataSizeN(i, /*masking=*/false, /*num bytes in first packet=*/100000, /*num frame repititions=*/4);

		/*
		testConnectAndRequest(http_query);

		// Add another websocket frame

		http_query.resize(http_query.size() + 2);
		byte = 0x80 | 0x1; // Fin | text opcode
		std::memcpy(&http_query[http_query.size() - 2], &byte, 1);
		byte = 5; // Mask bit | payload len
		std::memcpy(&http_query[http_query.size() - 1], &byte, 1);

		testConnectAndRequest(http_query);
		http_query += "hello";
		testConnectAndRequest(http_query);*/
	}



	//============================= Test posts ================================
	testPost("POST / HTTP/1.1" + CRLF + "Content-Length: -1" + CRLFCRLF, "");
	testPost("POST / HTTP/1.1" + CRLF + "Content-Length: 0" + CRLFCRLF, "");
	testPost("POST / HTTP/1.1" + CRLF + "Content-Length: 1" + CRLFCRLF, "a");
	testPost("POST / HTTP/1.1" + CRLF + "Content-Length: 100" + CRLFCRLF, std::string(100, 'a'));
	testPost("POST / HTTP/1.1" + CRLF + "Content-Length: 10000" + CRLFCRLF, std::string(10000, 'a'));

	// Test post form parsing
	testPost("");
	testPost(" ");
	testPost("a");
	testPost("1");
	testPost("*");
	testPost("a=");
	testPost("a=a");
	testPost("a==");
	testPost("a=?");
	testPost("a=1&");
	testPost("a=1&b");
	testPost("a=1&b=");
	testPost("a=1&b=2");
	testPost("a=1&b=2&");
	testPost("a=1&=2&");
	testPost("a=1&2&");
	testPost("a=1&&");
	testPost("a=1&?&");







	testConnectAndRequest("");
	testConnectAndRequest("a");
	testConnectAndRequest("GET");
	testConnectAndRequest("GET / HTTP/1.1");
	testConnectAndRequest("" + CRLFCRLF);
	testConnectAndRequest("" + CRLFCRLF);
	testConnectAndRequest("a" + CRLFCRLF);
	testConnectAndRequest("GET" + CRLFCRLF);
	testConnectAndRequest("GET / HTTP/1.1" + CRLFCRLF);
	
	// Try some partial CRLFs
	testConnectAndRequest("\r");
	testConnectAndRequest("\r\n");
	testConnectAndRequest("\r\n\r");
	testConnectAndRequest("\r\n\r\n");
	
	// Try some double CRLFCRLFs
	testConnectAndRequest("" + CRLFCRLF + CRLFCRLF);
	testConnectAndRequest("a" + CRLFCRLF + CRLFCRLF);
	testConnectAndRequest("GET" + CRLFCRLF + CRLFCRLF);
	testConnectAndRequest("GET / HTTP/1.1" + CRLFCRLF + CRLFCRLF);

	// Missing URL
	testConnectAndRequest("GET  HTTP/1.1" + CRLFCRLF);

	// Bad Verb
	testConnectAndRequest("BLEH HTTP/1.1" + CRLFCRLF);

	// Bad HTTP versions:
	testConnectAndRequest("BLEH / HTTPA/1.1" + CRLFCRLF);

	testConnectAndRequest("BLEH / HTTP/0.2" + CRLFCRLF);
	testConnectAndRequest("BLEH / HTTP/04.2" + CRLFCRLF);
	testConnectAndRequest("BLEH / HTTP/a.2" + CRLFCRLF);
	testConnectAndRequest("BLEH / HTTP/1.a" + CRLFCRLF);
	testConnectAndRequest("BLEH / HTTP/04.26" + CRLFCRLF);
	testConnectAndRequest("BLEH / HTTP/04a.2bn6" + CRLFCRLF);
	testConnectAndRequest("BLEH / HTTP/04a 2bn6" + CRLFCRLF);

	// Test various Header fields (malformed etc..)
	testConnectAndRequest("GET / HTTP/1.1" + CRLF + "a:b" + CRLFCRLF);

	testConnectAndRequest("GET / HTTP/1.1" + CRLF + "a" + CRLFCRLF);
	testConnectAndRequest("GET / HTTP/1.1" + CRLF + "  a  " + CRLFCRLF);
	testConnectAndRequest("GET / HTTP/1.1" + CRLF + "a  " + CRLFCRLF);
	testConnectAndRequest("GET / HTTP/1.1" + CRLF + "a:" + CRLFCRLF);
	testConnectAndRequest("GET / HTTP/1.1" + CRLF + "a: " + CRLFCRLF);
	testConnectAndRequest("GET / HTTP/1.1" + CRLF + "a:  " + CRLFCRLF);
	testConnectAndRequest("GET / HTTP/1.1" + CRLF + "a:   b" + CRLFCRLF);

	// Test headers with missing CRLFCRLF afterwards
	/*testConnectAndRequest("GET / HTTP/1.1" + CRLF + "");
	testConnectAndRequest("GET / HTTP/1.1" + CRLF + "a");
	testConnectAndRequest("GET / HTTP/1.1" + CRLF + "a:");
	testConnectAndRequest("GET / HTTP/1.1" + CRLF + "a: ");
	testConnectAndRequest("GET / HTTP/1.1" + CRLF + "a:  ");
	testConnectAndRequest("GET / HTTP/1.1" + CRLF + "a: b");
	testConnectAndRequest("GET / HTTP/1.1" + CRLF + ": b");*/

	// Test various URLs
	testConnectAndRequest("BLEH a HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a/ HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH /a HTTP/1.1" + CRLFCRLF);
	
	testConnectAndRequest("BLEH ? HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a? HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a?a HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a?1 HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a?* HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a?a= HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a?a=a HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a?a== HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a?a=? HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a?a=1& HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a?a=1&b HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a?a=1&b= HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a?a=1&b=2 HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a?a=1&b=2& HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a?a=1&=2& HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a?a=1&2& HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a?a=1&& HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("BLEH a?a=1&?& HTTP/1.1" + CRLFCRLF);

	// test multiple request in one packet
	testConnectAndRequest("GET / HTTP/1.1" + CRLFCRLF + "GET / HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("GET / HTTP/1.1" + CRLFCRLF + "GET / HTTP/1.1" + CRLFCRLF + "GET / HTTP/1.1" + CRLFCRLF);
	testConnectAndRequest("GET / HTTP/1.1" + CRLFCRLF + "GET / HTTP/1.1" + CRLFCRLF + "GET / HTTP/1.1" + CRLFCRLF + "GET / HTTP/1.1" + CRLFCRLF);


	// Connect to local port
	{
	Reference<MySocket> socket = new MySocket("localhost", port);

	const std::string std_request = "GET / HTTP/1.1" + CRLF + "Host: localhost" + CRLF + CRLF;

	// Write the request n times.  We expect to hear back n responses.
	testRequestAndResponse(socket, std_request, "ping");
	testRequestAndResponse(socket, std_request + std_request, "pingping");
	testRequestAndResponse(socket, std_request + std_request + std_request, "pingpingping");

	}

	


	// Make a request
	/*{
		const std::string request = "GET / HTTP/1.1" + CRLF + 
			"Host: www.example.com" + CRLF + CRLF;

		socket->write(request.c_str(), request.size(), NULL, NULL);

		// Read response:
		char buf[10000];
		const size_t num_bytes_read = socket->readSomeBytes(buf, sizeof(buf));

		conPrint("Response: ");
		conPrint(std::string(buf, buf + num_bytes_read));
	}

	}


	// Connect to local port
	{
	Reference<MySocket> socket = new MySocket("localhost", port, NULL);


	// Make a request
	{
		const std::string request = "GET / HTTP/1.1" + CRLF + 
			"Host: www.example.com" + CRLF + CRLF;

		socket->write(request.c_str(), request.size(), NULL, NULL);

		// Read response:
		char buf[10000];
		const size_t num_bytes_read = socket->readSomeBytes(buf, sizeof(buf));

		conPrint("Response: ");
		conPrint(std::string(buf, buf + num_bytes_read));
	}

	// Make a request
	{
		const std::string request = "GET / HTTP/1.1" + CRLF + 
			"Host: www.example.com" + CRLF + CRLF;

		socket->write(request.c_str(), request.size(), NULL, NULL);

		// Read response:
		char buf[10000];
		const size_t num_bytes_read = socket->readSomeBytes(buf, sizeof(buf));

		conPrint("Response: ");
		conPrint(std::string(buf, buf + num_bytes_read));
	}*/


	// Try lots of different requests
	/*for(int i=0; i<10000; ++i)
	{
		try
		{
			Reference<MySocket> socket = new MySocket("localhost", port, NULL);

			std::string request = std::string(i, 'a') + CRLF + CRLF + std::string(i, 'a');
			testRequest(socket, request);
		}
		catch(MySocketExcep& )
		{}
	}*/


	////listener->getMessageQueue().enqueue(new KillThreadMessage());
	//listener->join();

	thread_manager.killThreadsBlocking();
}


} // end namespace web


#endif
