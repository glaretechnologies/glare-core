/*=====================================================================
HTTPClient.cpp
--------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "HTTPClient.h"


#include "URL.h"
#include "TLSSocket.h"
#include "../utils/ConPrint.h"
#include "../utils/Parser.h"
#include "../utils/Exception.h"
#include "../utils/RuntimeCheck.h"
#include "../maths/CheckedMaths.h"
#include <tls.h>
#include <cstring>
#include <limits>


HTTPClient::HTTPClient()
:	max_data_size(std::numeric_limits<size_t>::max()),
	max_socket_buffer_size(1 << 16),
	keepalive_socket(false),
	connected_port(-1),
	enable_TCP_nodelay(false)
{}


HTTPClient::~HTTPClient()
{}


void HTTPClient::connectAndEnableKeepAlive(const std::string& protocol, const std::string& hostname, int port)
{
	this->keepalive_socket = true;

	connect(protocol, hostname, port);
}


static void checkSchemeValid(const std::string& scheme)
{
	if(!(scheme == "http" || scheme == "https"))
		throw glare::Exception("Invalid scheme");
}


// Port = -1 means use default port for protocol.
void HTTPClient::connect(const std::string& protocol, const std::string& hostname, int port)
{
	this->socket = nullptr;
	this->connected_scheme.clear();
	this->connected_hostname.clear();
	this->connected_port = -1;

	if(test_socket)
		this->socket = test_socket;
	else
	{
		if(protocol == "https")
		{
			MySocketRef plain_socket = new MySocket();
			this->socket = plain_socket; // Store in this->socket so we can interrupt in kill() while connecting.
			plain_socket->connect(hostname, (port == -1) ? 443 : port);
			if(enable_TCP_nodelay) 
				plain_socket->setNoDelayEnabled(true);

			TLSConfig client_tls_config;

			tls_config_insecure_noverifycert(client_tls_config.config); // TEMP: try and work out how to remove this call.

			this->socket = new TLSSocket(plain_socket, client_tls_config.config, hostname);
		}
		else
		{
			// Assume http (non-TLS)
			this->socket = new MySocket(hostname, (port == -1) ? 80 : port);
		}
	}

	if(this->keepalive_socket)
		this->socket->enableTCPKeepAlive(/*period=*/5.0);

	this->connected_scheme = protocol;
	this->connected_hostname = hostname;
	this->connected_port = port;
}


void HTTPClient::resetConnection()
{
	this->socket = NULL;
	socket_buffer.clear();
}


// Handle the HTTP response.
// The response header is in [socket_buffer[0], socket_buffer[response_header_size])
HTTPClient::ResponseInfo HTTPClient::handleResponse(size_t response_header_size, RequestType request_type, int num_redirects_done, StreamingDataHandler& response_data_handler)
{
	// conPrint(std::string(&socket_buffer[0], &socket_buffer[0] + response_header_size));

	runtimeCheck(response_header_size > 0);
	runtimeCheck(response_header_size <= socket_buffer.size());

	// Parse request
	Parser parser((const char*)socket_buffer.data(), response_header_size);

	//------------- Parse HTTP version ---------------
	if(!parser.parseString("HTTP/"))
		throw glare::Exception("Failed to parse HTTP version");

	uint32 major_version;
	if(!parser.parseUnsignedInt(major_version))
		throw glare::Exception("Failed to parse HTTP major version");

	// Parse '.'
	if(!parser.parseChar('.'))
		throw glare::Exception("Parser error");

	uint32 minor_version;
	if(!parser.parseUnsignedInt(minor_version))
		throw glare::Exception("Failed to parse HTTP minor version");

	// Parse space(s)
	while(parser.currentIsChar(' '))
		parser.advance();

	//-------------- Parse reponse code --------------
	int code;
	if(!parser.parseInt(code))
		throw glare::Exception("Failed to parse response code");

	//-------------- Parse response message --------------
	string_view response_msg;
	if(!parser.parseToChar('\r', response_msg))
		throw glare::Exception("Failed to parse response message");

	parser.advance(); // Advance past \r
	if(!parser.parseChar('\n')) // Advance past \n
		throw glare::Exception("Parse error");


	// Print out response:
	//conPrint("HTTP version: HTTP/" + toString(major_version) + "." + toString(minor_version));
	//conPrint("Response code: " + toString(code));
	//conPrint("Response message: '" + response_msg.to_string() + "'");
	
	ResponseInfo response_info;
	response_info.response_code = code;
	response_info.response_message = toString(response_msg);

	// Parse response headers:
	uint64 content_length = 0;
	bool have_content_length = false;
	bool chunked = false;
	string_view location;
	while(1)
	{
		if(parser.eof())
			throw glare::Exception("Parser error while parsing header fields");
		if(parser.current() == '\r')
			break;

		// Parse header field name
		string_view field_name;
		if(!parser.parseToChar(':', field_name))
			throw glare::Exception("Parser error while parsing header fields");
		parser.advance(); // Advance past ':'

		// If there is a space, consume it
		if(parser.currentIsChar(' '))
			parser.advance();

		// Parse the field value
		string_view field_value;
		if(!parser.parseToChar('\r', field_value))
			throw glare::Exception("Parser error while parsing header fields");

		parser.advance(); // Advance past \r
		if(!parser.parseChar('\n')) // Advance past \n
			throw glare::Exception("Parse error");

		// conPrint(field_name + ": " + field_value);

		if(StringUtils::equalCaseInsensitive(field_name, "content-length"))
		{
			try
			{
				content_length = stringToUInt64(toString(field_value));
				have_content_length = true;
			}
			catch(StringUtilsExcep& e)
			{
				throw glare::Exception("Failed to parse content length: " + e.what());
			}
		}
		else if(StringUtils::equalCaseInsensitive(field_name, "content-type"))
		{
			response_info.mime_type = toString(field_value);
		}
		else if(StringUtils::equalCaseInsensitive(field_name, "transfer-encoding"))
		{
			if(field_value == "chunked")
				chunked = true;
		}
		else if(StringUtils::equalCaseInsensitive(field_name, "location"))
		{
			location = field_value;
		}
	}

	parser.advance(); // Advance past \r
	if(!parser.parseChar('\n')) // Advance past \n
		throw glare::Exception("Parse error");

	if(code == 301 || code == 302)
	{
		if(location.empty())
			throw glare::Exception("Redirect location was empty");

		if(request_type == RequestType_Get)
		{
			// conPrint("Redirecting to '" + location + "'...");
			return doDownloadFile(toString(location), num_redirects_done + 1, response_data_handler);
		}
		else
			throw glare::Exception("Redirect received for POST request, not supported currently.");
	}

	if(have_content_length) // If the server sent a valid content-length:
	{
		if(content_length > max_data_size)
			throw glare::Exception("Content length (" + toString(content_length) + " B) exceeded max data size (" + toString(max_data_size) + " B)");

		response_data_handler.haveContentLength(content_length);

		// Copy any body data we have from socket_buffer to data_out.
		const uint64 body_data_read = socket_buffer.size() - response_header_size;
		const uint64 use_content_read_B = myMin(body_data_read, content_length);
		if(use_content_read_B > 0)
			response_data_handler.handleData(ArrayRef<uint8>(socket_buffer).getSliceChecked(response_header_size, use_content_read_B), response_info);

		// Now read rest of data from socket, and write to data_out
		runtimeCheck(content_length >= use_content_read_B);
		uint64 remaining_data = content_length - use_content_read_B;
		while(remaining_data > 0)
		{
			// Read chunks of data from socket, pass to response_data_handler.handleData().
			const uint64 MAX_READ_SIZE = 1 << 14; // TODO: use min() with max_socket_buffer_size here?
			
			const size_t read_size = (size_t)myMin(remaining_data, MAX_READ_SIZE);
			socket_buffer.resize(read_size);
			socket->readDataChecked(socket_buffer, /*buf index=*/0, /*num bytes=*/read_size);

			response_data_handler.handleData(ArrayRef<uint8>(socket_buffer).getSliceChecked(0, read_size), response_info);

			runtimeCheck(remaining_data >= read_size);
			remaining_data -= read_size;
		}
	}
	else
	{
		if(chunked)
		{
			// See https://www.jmarshall.com/easy/http/#http1.1c2 for details on chunked transfer encoding.

			size_t chunk_line_start_index = response_header_size;
			while(1)
			{
				const size_t chunk_line_end_index = readUntilCRLF(chunk_line_start_index); // Read the entire chunk description line.
				runtimeCheck(chunk_line_end_index >= chunk_line_start_index + 2); // Should have at least the CRLF at the end of the chunk description line.

				// Parse the chunk description line
				runtimeCheck((chunk_line_start_index < socket_buffer.size()) && (chunk_line_end_index <= socket_buffer.size()));
				Parser size_parser((const char*)&socket_buffer[chunk_line_start_index], chunk_line_end_index - chunk_line_start_index);
				string_view size_str;
				size_parser.parseToCharOrEOF(';', size_str);
				size_t chunk_size = 0;
				if(size_parser.eof())
				{
					runtimeCheck(size_str.size() >= 2);
					chunk_size = ::hexStringToUInt64(toString(string_view(size_str.data(), size_str.size() - 2))); // trim off CRLF
				}
				else
					chunk_size = ::hexStringToUInt64(toString(size_str));

				// Download chunk (and CRLF after it) and copy chunk to data_out
				if(chunk_size > 0)
				{
					const size_t chunk_and_crlf_size  = CheckedMaths::addUnsignedInts(chunk_size, (size_t)2);
					const size_t required_buffer_size = CheckedMaths::addUnsignedInts(chunk_line_end_index, chunk_and_crlf_size);
					if(socket_buffer.size() < required_buffer_size) // If we haven't read enough data yet:
					{
						if(required_buffer_size > max_socket_buffer_size)
							throw glare::Exception("Exceeded max_socket_buffer_size (" + toString(max_socket_buffer_size) + " B)");

						// Read remaining data
						const size_t current_amount_read = socket_buffer.size();
						socket_buffer.resize(required_buffer_size);
						socket->readDataChecked(socket_buffer, /*buf index=*/current_amount_read, /*num bytes=*/required_buffer_size - current_amount_read);
					}

					response_data_handler.handleData(ArrayRef<uint8>(socket_buffer).getSliceChecked(chunk_line_end_index, chunk_size), response_info);

					chunk_line_start_index = chunk_line_end_index + chunk_and_crlf_size; // Advance past chunk line + chunk data.
				}
				else
				{
					// Finished chunks.  Footers follow but we will ignore those.
					break;
				}
			}
		}
		else
		{
			// Server didn't send a valid content length, so just keep reading until server closes the connection
			
			// Pass any data we have already in socket_buffer to handleData().
			const size_t body_data_read = socket_buffer.size() - response_header_size;
			if(body_data_read > max_data_size)
				throw glare::Exception("Body length (" + toString(body_data_read) + " B) exceeded max data size (" + toString(max_data_size) + " B)");

			if(body_data_read > 0)
				response_data_handler.handleData(ArrayRef<uint8>(socket_buffer).getSliceChecked(response_header_size, body_data_read), response_info);

			while(1)
			{
				// Read up to MAX_READ_SIZE additional bytes, pass to data handler.
				const size_t MAX_READ_SIZE = 16 * 1024;
				socket_buffer.resize(MAX_READ_SIZE);
				const size_t amount_read = socket->readSomeBytesChecked(socket_buffer, /*buf index=*/0, /*max num bytes=*/MAX_READ_SIZE);
				if(amount_read > 0)
					response_data_handler.handleData(ArrayRef<uint8>(socket_buffer).getSliceChecked(0, amount_read), response_info);
				else
					break; // Else connection was gracefully closed
			}
		}
	}

	return response_info;
}


// Scans socket_buffer for the first CRLF at or after scan_start_index.
// Downloads more data as needed until a CRLF is found.
// Returns index in socket_buffer after CRLF.
size_t HTTPClient::readUntilCRLF(size_t scan_start_index)
{
	size_t current_scan_i = scan_start_index;
	while(1)
	{
		// Scan from current_scan_i to end of data
		const size_t socket_buf_size = socket_buffer.size();
		for(; current_scan_i + 2 <= socket_buf_size; ++current_scan_i)
			if(socket_buffer[current_scan_i] == '\r' && socket_buffer[current_scan_i + 1] == '\n')
				return current_scan_i + 2; // We have found a CRLF

		// Read up to 'read_chunk_size' bytes of data from the socket.
		const size_t old_socket_buffer_size = socket_buffer.size();
		const size_t read_chunk_size = 2048;

		if((old_socket_buffer_size + read_chunk_size) > max_socket_buffer_size)
			throw glare::Exception("Exceeded max_socket_buffer_size (" + toString(max_socket_buffer_size) + " B)");

		socket_buffer.resize(old_socket_buffer_size + read_chunk_size);
		const size_t num_bytes_read = socket->readSomeBytesChecked(socket_buffer, /*buf index=*/old_socket_buffer_size, /*max num bytes=*/read_chunk_size); // Read up to 'read_chunk_size' bytes.
		if(num_bytes_read == 0) // if connection was closed gracefully:
			throw HTTPClientExcep("Failed to find a CRLF before socket was closed.", HTTPClientExcep::ExcepType_ConnectionClosedGracefully); // Connection was closed before we got to a CRLF.
		socket_buffer.resize(old_socket_buffer_size + num_bytes_read); // Trim the buffer down so it only extends to what we actually read.
	}
}


size_t HTTPClient::readUntilCRLFCRLF(size_t scan_start_index)
{
	size_t current_scan_i = scan_start_index;
	while(1)
	{
		// Scan from current_scan_i to end of data
		const size_t socket_buf_size = socket_buffer.size();
		for(; current_scan_i + 4 <= socket_buf_size; ++current_scan_i)
			if(socket_buffer[current_scan_i] == '\r' && socket_buffer[current_scan_i + 1] == '\n' && socket_buffer[current_scan_i + 2] == '\r' && socket_buffer[current_scan_i + 3] == '\n')
				return current_scan_i + 4; // We have found a CRLFCRLF

		// Read up to 'read_chunk_size' bytes of data from the socket.
		const size_t old_socket_buffer_size = socket_buffer.size();
		const size_t read_chunk_size = 2048;

		if((old_socket_buffer_size + read_chunk_size) > max_socket_buffer_size)
			throw glare::Exception("Exceeded max_socket_buffer_size (" + toString(max_socket_buffer_size) + " B)");

		socket_buffer.resize(old_socket_buffer_size + read_chunk_size);
		const size_t num_bytes_read = socket->readSomeBytesChecked(socket_buffer, /*buf index=*/old_socket_buffer_size, /*max num bytes=*/read_chunk_size); // Read up to 'read_chunk_size' bytes.
		if(num_bytes_read == 0) // if connection was closed gracefully:
			throw HTTPClientExcep("Failed to find a CRLF before socket was closed.", HTTPClientExcep::ExcepType_ConnectionClosedGracefully); // Connection was closed before we got to a CRLF.
		socket_buffer.resize(old_socket_buffer_size + num_bytes_read); // Trim the buffer down so it only extends to what we actually read.
	}
}


class StreamToStringDataHandler : public HTTPClient::StreamingDataHandler
{
public:
	StreamToStringDataHandler(std::vector<uint8>& combined_data_, size_t max_data_size_) : combined_data(combined_data_), max_data_size(max_data_size_) {}

	virtual void haveContentLength(uint64 content_length) override
	{
		if(content_length > max_data_size)
			throw glare::Exception("Content length (" + toString(content_length) + " B) exceeded max data size (" + toString(max_data_size) + " B)");

		combined_data.reserve(content_length);
	}
	virtual void handleData(ArrayRef<uint8> data, const HTTPClient::ResponseInfo& /*response_info*/) override
	{
		if(data.size() > 0)
		{
			const size_t cur_len = combined_data.size();
			const size_t new_len = CheckedMaths::addUnsignedInts(cur_len, data.size());

			if(new_len > max_data_size)
				throw glare::Exception("Data size (" + toString(new_len) + " B) exceeded max data size (" + toString(max_data_size) + " B)");

			combined_data.resize(new_len);
			checkedArrayRefMemcpy(combined_data, /*dest index=*/cur_len, /*src=*/data, /*src index=*/0, /*size=*/data.size());
		}
	}

	std::vector<uint8>& combined_data;
	size_t max_data_size;
};


HTTPClient::ResponseInfo HTTPClient::sendPost(const std::string& url, const std::string& post_content, const std::string& content_type, std::vector<uint8>& data_out) // Throws glare::Exception on failure.
{
	StreamToStringDataHandler handler(data_out, max_data_size);
	return sendPost(url, post_content, content_type, handler);
}


HTTPClient::ResponseInfo HTTPClient::sendPost(const std::string& url, const std::string& post_content, const std::string& content_type, std::string& data_out)
{
	std::vector<uint8> vec_data;
	StreamToStringDataHandler handler(vec_data, max_data_size);
	ResponseInfo response = sendPost(url, post_content, content_type, handler);

	data_out.assign((const char*)vec_data.data(), vec_data.size());

	return response;
}


HTTPClient::ResponseInfo HTTPClient::sendPost(const std::string& url, const std::string& post_content, const std::string& content_type, StreamingDataHandler& response_data_handler) // Throws glare::Exception on failure.
{
	socket_buffer.clear();
	
	const URL url_components = URL::parseURL(url);

	checkSchemeValid(url_components.scheme);

	if(this->socket.isNull() || (this->connected_scheme != url_components.scheme) || (this->connected_hostname != url_components.host) || (this->connected_port != url_components.port)) // If not connected yet, or connected to wrong host
		connect(url_components.scheme, url_components.host, url_components.port);

	const std::string path_and_query = url_components.path + (!url_components.query.empty() ? ("?" + url_components.query) : "");

	std::string additional_header_lines;
	for(size_t i=0; i<additional_headers.size(); ++i)
		additional_header_lines += additional_headers[i] + "\r\n";

	// Send request
	const std::string request_header = "POST " + path_and_query + " HTTP/1.1\r\n"
		"Host: " + url_components.host + "\r\n"
		"Content-Type: " + content_type + "\r\n"
		"Content-Length: " + toString(post_content.size()) + "\r\n" +
		((!user_agent.empty()) ? (std::string("User-Agent: ") + user_agent + "\r\n") : std::string("")) + // Write user_agent header if set.
		additional_header_lines +
		"Connection: " + (keepalive_socket ? std::string("Keep-Alive") : std::string("Close")) + "\r\n"
		"\r\n";

	this->socket->writeData(request_header.data(), request_header.size());
	this->socket->writeData(post_content.data(),   post_content.size());

	const size_t CRLFCRLF_end_i = readUntilCRLFCRLF(/*scan_start_index=*/0);

	return handleResponse(CRLFCRLF_end_i, RequestType_Post, /*num_redirects_done=*/0, response_data_handler);
}


HTTPClient::ResponseInfo HTTPClient::downloadFile(const std::string& url, StreamingDataHandler& response_data_handler) // Throws glare::Exception on failure.
{
	return doDownloadFile(url, /*num_redirects_done=*/0, response_data_handler);
}


HTTPClient::ResponseInfo HTTPClient::downloadFile(const std::string& url, std::vector<uint8>& data_out)
{
	StreamToStringDataHandler handler(data_out, max_data_size);

	return doDownloadFile(url, /*num_redirects_done=*/0, handler);
}


HTTPClient::ResponseInfo HTTPClient::downloadFile(const std::string& url, std::string& data_out)
{
	std::vector<uint8> data_vec;
	StreamToStringDataHandler handler(data_vec, max_data_size);

	ResponseInfo response = doDownloadFile(url, /*num_redirects_done=*/0, handler);

	data_out.assign((const char*)data_vec.data(), data_vec.size());

	return response;
}


HTTPClient::ResponseInfo HTTPClient::doDownloadFile(const std::string& url, int num_redirects_done, StreamingDataHandler& response_data_handler)
{
	socket_buffer.clear();
	if(num_redirects_done >= 10)
		throw glare::Exception("too many redirects.");

	const URL url_components = URL::parseURL(url);

	checkSchemeValid(url_components.scheme);

	if(this->socket.isNull() || (this->connected_scheme != url_components.scheme) || (this->connected_hostname != url_components.host) || (this->connected_port != url_components.port)) // If not connected yet, or connected to wrong host:
		connect(url_components.scheme, url_components.host, url_components.port);
		
	const std::string path_and_query = url_components.path + (!url_components.query.empty() ? ("?" + url_components.query) : "");

	// Send request
	std::string request;
	request.reserve(2048);
	request += "GET ";
	request += path_and_query;
	request += " HTTP/1.1\r\n";
	request += "Host: ";
	request += url_components.host;
	request += "\r\n";
	for(size_t i=0; i<additional_headers.size(); ++i)
	{
		request += additional_headers[i];
		request += "\r\n";
	}
	request += "Connection: ";
	request += keepalive_socket ? "Keep-Alive" : "Close";
	request += "\r\n";
	request += "\r\n";

	this->socket->writeData(request.data(), request.size());

	const size_t CRLFCRLF_end_i = readUntilCRLFCRLF(/*scan_start_index=*/0);

	return handleResponse(CRLFCRLF_end_i, RequestType_Get, num_redirects_done, response_data_handler);
}


void HTTPClient::kill()
{
	SocketInterfaceRef socket_ = socket;
	if(socket_.nonNull())
		socket_->ungracefulShutdown();
}


#if BUILD_TESTS


#include "../networking/TestSocket.h"
#include "../utils/TestUtils.h"
#include "../utils/MyThread.h"


//========================== Fuzzing ==========================
#if 0
// Command line:
// C:\fuzz_corpus\http_client C:\code\glare-core\testfiles\fuzz_seeds\http_client

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
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

		HTTPClient client;
		client.max_data_size = 1 << 16;
		client.max_socket_buffer_size = 1 << 16;
		client.test_socket = test_socket;
		std::string resdata;
		client.downloadFile("https://someurl", resdata);
	}
	catch(glare::Exception& /*e*/)
	{
		//conPrint("Excep: " + e.what());
	}

	return 0; // Non-zero return values are reserved for future use.
}
#endif
//========================== End fuzzing ==========================


class HTTPClientTestThread : public MyThread
{
public:
	HTTPClientTestThread(const std::string& path_) : path(path_) {}
	~HTTPClientTestThread() {}

	virtual void run()
	{
		conPrint("HTTPClientTestThread::run()");
		for(int i=0; i<100; ++i)
		{
			try
			{
				HTTPClient client;
				client.setAsNotIndependentlyHeapAllocated();
				std::vector<uint8> data;
				client.downloadFile(path, data);
				//conPrint(data);
				conPrintStr(".");
			}
			catch(glare::Exception& e)
			{
				failTest(e.what());
			}
		}
	}

	std::string path;
};


void HTTPClient::test()
{
	// Test downloading a file
	try
	{
		// TODO: test with lower case header keys and chunked encoding.
		
		{
			HTTPClient client;
			client.setAsNotIndependentlyHeapAllocated();
			std::vector<uint8> data;
			client.downloadFile("http://www.cbloom.com/rants.html", data);
			//conPrint(data);
		}
		{
			HTTPClient client;
			client.setAsNotIndependentlyHeapAllocated();
			std::vector<uint8> data;
			client.downloadFile("https://forwardscattering.org/post/3", data);
			//conPrint(data);

			std::string string_data;
			client.downloadFile("https://forwardscattering.org/post/3", string_data);
			conPrint(string_data);
		}

		{
			HTTPClient client;
			client.setAsNotIndependentlyHeapAllocated();
			std::vector<uint8> data;
			client.downloadFile("https://docs.python.org/3/library/urllib.parse.html", data);
			//conPrint(data);
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}

	/*std::vector<Reference<HTTPClientTestThread> > threads;
	for(int i=0; i<30; ++i)
	{
		threads.push_back(new HTTPClientTestThread("https://forwardscattering.org/post/3"));
		threads.back()->launch();
	}

	for(size_t i=0; i<threads.size(); ++i)
		threads[i]->join();*/
}


#endif // BUILD_TESTS
