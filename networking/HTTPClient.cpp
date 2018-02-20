/*=====================================================================
HTTPClient.cpp
--------------
Copyright Glare Technologies Limited 2018 -
Generated at 2018-02-01 23:26:27 +1300
=====================================================================*/
#include "HTTPClient.h"


#if USING_LIBRESSL


#include "mysocket.h"
#include "url.h"
#include "TLSSocket.h"
#include "../utils/ConPrint.h"
#include "../utils/Parser.h"
#include "../utils/Exception.h"
#include "../utils/Base64.h"
#include "../utils/Clock.h"
#include <tls.h>


HTTPClient::HTTPClient()
{}


HTTPClient::~HTTPClient()
{}


// Handle a single HTTP request.
// The request header is in [socket_buffer[request_start_index], socket_buffer[request_start_index + request_header_size])
// Returns if should keep connection alive.
// Advances this->request_start_index. to index after the current request (e.g. will be at the beginning of the next request)
void HTTPClient::handleResponse(size_t request_header_size, std::string& data_out, ResponseInfo& file_info)
{
	// conPrint(std::string(&socket_buffer[request_start_index], &socket_buffer[request_start_index] + request_header_size));

	assert(request_header_size > 0);

	// Parse request
	assert(request_header_size <= socket_buffer.size());
	Parser parser(&socket_buffer[0], (unsigned int)request_header_size);


	//------------- Parse HTTP version ---------------
	if(!parser.parseString("HTTP/"))
		throw Indigo::Exception("Failed to parse HTTP version");

	uint32 major_version;
	if(!parser.parseUnsignedInt(major_version))
		throw Indigo::Exception("Failed to parse HTTP major version");

	// Parse '.'
	if(!parser.parseChar('.'))
		throw Indigo::Exception("Parser error");

	uint32 minor_version;
	if(!parser.parseUnsignedInt(minor_version))
		throw Indigo::Exception("Failed to parse HTTP minor version");

	// Parse space(s)
	while(parser.currentIsChar(' '))
		parser.advance();

	//-------------- Parse reponse code --------------
	int code;
	if(!parser.parseInt(code))
		throw Indigo::Exception("Failed to parse response code");

	//-------------- Parse response message --------------
	string_view response_msg;
	if(!parser.parseToChar('\r', response_msg))
		throw Indigo::Exception("Failed to parse response message");

	parser.advance(); // Advance past \r
	if(!parser.parseChar('\n')) // Advance past \n
		throw Indigo::Exception("Parse error");


	// Print out response:
	conPrint("HTTP version: HTTP/" + toString(major_version) + "." + toString(minor_version));
	conPrint("Response code: " + toString(code));
	conPrint("Response message: '" + response_msg.to_string() + "'");
	
	file_info.response_code = code;
	file_info.response_message = response_msg.to_string();

	// Parse response headers:
	int content_length = -1;
	while(1)
	{
		if(parser.eof())
			throw Indigo::Exception("Parser error while parsing header fields");
		if(parser.current() == '\r')
			break;

		// Parse header field name
		string_view field_name;
		if(!parser.parseToChar(':', field_name))
			throw Indigo::Exception("Parser error while parsing header fields");
		parser.advance(); // Advance past ':'

		// If there is a space, consume it
		if(parser.currentIsChar(' '))
			parser.advance();

		// Parse the field value
		string_view field_value;
		if(!parser.parseToChar('\r', field_value))
			throw Indigo::Exception("Parser error while parsing header fields");

		parser.advance(); // Advance past \r
		if(!parser.parseChar('\n')) // Advance past \n
			throw Indigo::Exception("Parse error");

		//TEMP:
		conPrint(field_name + ": " + field_value);

		if(field_name == "Content-Length")
		{
			try
			{
				content_length = stringToInt(field_value.to_string());
			}
			catch(StringUtilsExcep& e)
			{
				throw Indigo::Exception("Failed to parse content length: " + e.what());
			}
		}
		else if(field_name == "Content-Type")
		{
			file_info.mime_type = field_value.to_string();
		}
	}

	parser.advance(); // Advance past \r
	if(!parser.parseChar('\n')) // Advance past \n
		throw Indigo::Exception("Parse error");


	if(content_length >= 0) // If the server sent a valid content-length:
	{
		// Read reply body from socket
		const size_t total_msg_size = request_header_size + (size_t)content_length;
		const size_t required_buffer_size = total_msg_size;
		if(socket_buffer.size() < required_buffer_size) // If we haven't read the entire post body yet
		{
			// Read remaining data
			const size_t current_amount_read = socket_buffer.size(); // NOTE: this correct?
			socket_buffer.resize(required_buffer_size);

			socket->readData(&socket_buffer[current_amount_read], required_buffer_size - current_amount_read);
		}
		
		// Copy to data_out
		data_out.resize(content_length);
		if(content_length > 0)
			std::memcpy(&data_out[0], &socket_buffer[request_header_size], (size_t)content_length);
	}
	else
	{
		// Server didn't send a valid content length, so just keep reading until server closes the connection
		while(1)
		{
			const size_t READ_SIZE = 64 * 1024;
			const size_t write_pos = socket_buffer.size();
			socket_buffer.resize(write_pos + READ_SIZE);
			const size_t amount_read = socket->readSomeBytes(&socket_buffer[write_pos], READ_SIZE);
			socket_buffer.resize(write_pos + amount_read);
			if(amount_read == 0) // Connection was gracefully closed
				break;
		}
	}
}


HTTPClient::ResponseInfo HTTPClient::downloadFile(const std::string& url, std::string& data_out)
{
	ResponseInfo file_info;
	file_info.response_code = 0;

	try
	{
		const URL url_components = URL::parseURL(url);

		if(url_components.scheme == "https")
		{
			MySocketRef plain_socket = new MySocket(url_components.host, (url_components.port == -1) ? 443 : url_components.port);

			TLSConfig client_tls_config;

			tls_config_insecure_noverifycert(client_tls_config.config); // TEMP: try and work out how to remove this call.

			this->socket = new TLSSocket(plain_socket, client_tls_config.config, url_components.host);
		}
		else
		{
			// Assume http (non-TLS)
			this->socket = new MySocket(url_components.host, (url_components.port == -1) ? 80 : url_components.port);;
		}
		

		// Send request
		const std::string request = "GET " + url_components.path + " HTTP/1.1\r\n"
			"Host: " + url_components.host + "\r\n"
			"\r\n";
		this->socket->writeData(request.data(), request.size());


		size_t double_crlf_scan_position = 0; // Index in socket_buffer of the last place we looked for a double CRLF.
		while(true)
		{
			// Read up to 'read_chunk_size' bytes of data from the socket.  Note that we may read multiple requests at once.
			const size_t old_socket_buffer_size = socket_buffer.size();
			const size_t read_chunk_size = 2048;
			socket_buffer.resize(old_socket_buffer_size + read_chunk_size);
			const size_t num_bytes_read = socket->readSomeBytes(&socket_buffer[old_socket_buffer_size], read_chunk_size); // Read up to 'read_chunk_size' bytes.
																														  //print("thread_id " + toString(thread_id) + ": read " + toString(num_bytes_read) + " bytes.");
			if(num_bytes_read == 0) // if connection was closed gracefully
				throw Indigo::Exception("Failed to get a complete response header."); // Connection was closed before we got to a double CRLF, so we didn't get a complete response header
			socket_buffer.resize(old_socket_buffer_size + num_bytes_read); // Trim the buffer down so it only extends to what we actually read.

			// Process any complete requests
			// Look for the double CRLF at the end of the request header.
			const size_t socket_buf_size = socket_buffer.size();
			if(socket_buf_size >= 4) // Make sure socket_buf_size is >= 4, to avoid underflow and wraparound when computing 'socket_buf_size - 4' below.
				for(; double_crlf_scan_position <= socket_buf_size - 4; ++double_crlf_scan_position)
					if(socket_buffer[double_crlf_scan_position] == '\r' && socket_buffer[double_crlf_scan_position+1] == '\n' && socket_buffer[double_crlf_scan_position+2] == '\r' && socket_buffer[double_crlf_scan_position+3] == '\n')
					{
						// We have found the CRLFCRLF at index 'double_crlf_scan_position'.
						const size_t request_header_end = double_crlf_scan_position + 4;

						// Process the response:
						const size_t request_header_size = request_header_end;
						handleResponse(request_header_size, data_out, file_info);
						return file_info;
					}
		}
	}
	catch(MySocketExcep& e)
	{
		throw Indigo::Exception(e.what());
	}

	return file_info;
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"


void HTTPClient::test()
{
	// Test downloading a file
	try
	{
		{
			HTTPClient client;
			std::string data;
			client.downloadFile("http://www.cbloom.com/rants.html", data);
			conPrint(data);
		}
		{
			HTTPClient client;
			std::string data;
			client.downloadFile("https://forwardscattering.org/post/3", data);
			conPrint(data);
		}
		{
			HTTPClient client;
			std::string data;
			client.downloadFile("https://docs.python.org/3/library/urllib.parse.html", data);
			conPrint(data);
		}
	}
	catch(Indigo::Exception& e)
	{
		conPrint("Test failed: " + e.what());
		assert(0);
	}
	

}


#endif // BUILD_TESTS


#endif // USING_LIBRESSL
