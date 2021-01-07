/*=====================================================================
HTTPClient.cpp
--------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "HTTPClient.h"


#if USING_LIBRESSL


#include "MySocket.h"
#include "URL.h"
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


// Handle the HTTP response.
// The response header is in [socket_buffer[request_start_index], socket_buffer[request_start_index + response_header_size])
HTTPClient::ResponseInfo HTTPClient::handleResponse(size_t response_header_size, int num_redirects_done, std::string& data_out)
{
	// conPrint(std::string(&socket_buffer[request_start_index], &socket_buffer[request_start_index] + request_header_size));

	assert(response_header_size > 0);

	// Parse request
	assert(response_header_size <= socket_buffer.size());
	Parser parser(&socket_buffer[0], (unsigned int)response_header_size);


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
	//conPrint("HTTP version: HTTP/" + toString(major_version) + "." + toString(minor_version));
	//conPrint("Response code: " + toString(code));
	//conPrint("Response message: '" + response_msg.to_string() + "'");
	
	HTTPClient::ResponseInfo file_info;
	file_info.response_code = code;
	file_info.response_message = response_msg.to_string();

	// Parse response headers:
	uint64 content_length = 0;
	bool have_content_length = false;
	bool chunked = false;
	string_view location;
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

		// conPrint(field_name + ": " + field_value);

		if(field_name == "Content-Length")
		{
			try
			{
				content_length = stringToUInt64(field_value.to_string());
				have_content_length = true;
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
		else if(field_name == "Transfer-Encoding")
		{
			if(field_value == "chunked")
				chunked = true;
		}
		else if(field_name == "Location")
		{
			location = field_value;
		}
	}

	parser.advance(); // Advance past \r
	if(!parser.parseChar('\n')) // Advance past \n
		throw Indigo::Exception("Parse error");

	if(code == 301 || code == 302)
	{
		if(location.empty())
			throw Indigo::Exception("Redirect location was empty");

		// conPrint("Redirecting to '" + location + "'...");
		return doDownloadFile(location.to_string(), num_redirects_done + 1, data_out);
	}

	if(have_content_length) // If the server sent a valid content-length:
	{
		data_out.resize(content_length);

		// Copy any body data we have from socket_buffer to data_out.
		const size_t body_data_read = socket_buffer.size() - response_header_size;
		if(body_data_read > 0)
			std::memcpy(&data_out[0], &socket_buffer[response_header_size], body_data_read);

		// Now read rest of data from socket, and write to data_out
		const size_t remaining_data = content_length - body_data_read;
		if(remaining_data > 0)
			socket->readData(&data_out[body_data_read], remaining_data);
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
				assert(chunk_line_end_index >= chunk_line_start_index + 2); // Should have at least the CRLF

				// Parse the chunk description line
				Parser size_parser(&socket_buffer[chunk_line_start_index], chunk_line_end_index - chunk_line_start_index);
				string_view size_str;
				size_parser.parseToCharOrEOF(';', size_str);
				size_t chunk_size;
				try
				{
					if(size_parser.eof())
					{
						assert(size_str.size() >= 2);
						chunk_size = ::hexStringToUInt64(string_view(size_str.data(), size_str.size() - 2).to_string()); // trim off CRLF
					}
					else
						chunk_size = ::hexStringToUInt64(size_str.to_string());
				}
				catch(StringUtilsExcep& e)
				{
					throw Indigo::Exception(e.what());
				}

				// Download chunk (and CRLF after it) and copy chunk to data_out
				if(chunk_size > 0)
				{
					const size_t chunk_and_crlf_size = chunk_size + 2;
					const size_t required_buffer_size = chunk_line_end_index + chunk_and_crlf_size;
					if(socket_buffer.size() < required_buffer_size) // If we haven't read enough data yet:
					{
						// Read remaining data
						const size_t current_amount_read = socket_buffer.size();
						socket_buffer.resize(required_buffer_size);
						socket->readData(&socket_buffer[current_amount_read], required_buffer_size - current_amount_read);
					}

					// Copy chunk to data_out
					const size_t data_out_write_i = data_out.size();
					data_out.resize(data_out_write_i + chunk_size);
					std::memcpy(&data_out[data_out_write_i], &socket_buffer[chunk_line_end_index], chunk_size);

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
			
			// Copy any body data we have from socket_buffer to data_out.
			const size_t body_data_read = socket_buffer.size() - response_header_size;
			data_out.resize(body_data_read);
			if(body_data_read > 0)
				std::memcpy(&data_out[0], &socket_buffer[response_header_size], body_data_read);

			while(1)
			{
				// Read up to READ_SIZE additional bytes, append to end of data_out.
				const size_t READ_SIZE = 64 * 1024;
				const size_t write_pos = data_out.size();
				data_out.resize(write_pos + READ_SIZE);
				const size_t amount_read = socket->readSomeBytes(&data_out[write_pos], READ_SIZE);
				data_out.resize(write_pos + amount_read); // Trim down based on amount actually read.

				if(amount_read == 0) // Connection was gracefully closed
					break;
			}
		}
	}

	return file_info;
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
		socket_buffer.resize(old_socket_buffer_size + read_chunk_size);
		const size_t num_bytes_read = socket->readSomeBytes(&socket_buffer[old_socket_buffer_size], read_chunk_size); // Read up to 'read_chunk_size' bytes.
		if(num_bytes_read == 0) // if connection was closed gracefully:
			throw Indigo::Exception("Failed to find a CRLF before socket was closed."); // Connection was closed before we got to a CRLF.
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
		socket_buffer.resize(old_socket_buffer_size + read_chunk_size);
		const size_t num_bytes_read = socket->readSomeBytes(&socket_buffer[old_socket_buffer_size], read_chunk_size); // Read up to 'read_chunk_size' bytes.
		if(num_bytes_read == 0) // if connection was closed gracefully:
			throw Indigo::Exception("Failed to find a CRLF before socket was closed."); // Connection was closed before we got to a CRLF.
		socket_buffer.resize(old_socket_buffer_size + num_bytes_read); // Trim the buffer down so it only extends to what we actually read.
	}
}


HTTPClient::ResponseInfo HTTPClient::downloadFile(const std::string& url, std::string& data_out)
{
	return doDownloadFile(url, /*num_redirects_done=*/0, data_out);
}


HTTPClient::ResponseInfo HTTPClient::doDownloadFile(const std::string& url, int num_redirects_done, std::string& data_out)
{
	socket_buffer.clear();
	if(num_redirects_done >= 10)
		throw Indigo::Exception("too many redirects.");

	try
	{
		const URL url_components = URL::parseURL(url);

		if(url_components.scheme == "https")
		{
			MySocketRef plain_socket = new MySocket();
			this->socket = plain_socket; // Store in this->socket so we can interrupt in kill().
			plain_socket->connect(url_components.host, (url_components.port == -1) ? 443 : url_components.port);

			TLSConfig client_tls_config;

			tls_config_insecure_noverifycert(client_tls_config.config); // TEMP: try and work out how to remove this call.

			this->socket = new TLSSocket(plain_socket, client_tls_config.config, url_components.host);
		}
		else
		{
			// Assume http (non-TLS)
			this->socket = new MySocket(url_components.host, (url_components.port == -1) ? 80 : url_components.port);;
		}
		
		const std::string path_and_query = url_components.path + (!url_components.query.empty() ? ("?" + url_components.query) : "");

		// Send request
		const std::string request = "GET " + path_and_query + " HTTP/1.1\r\n"
			"Host: " + url_components.host + "\r\n"
			"Connection: close\r\n"
			"\r\n";
		this->socket->writeData(request.data(), request.size());

		const size_t CRLFCRLF_end_i = readUntilCRLFCRLF(/*scan_start_index=*/0);

		return handleResponse(CRLFCRLF_end_i, num_redirects_done, data_out);
	}
	catch(MySocketExcep& e)
	{
		throw Indigo::Exception(e.what());
	}
}


void HTTPClient::kill()
{
	SocketInterfaceRef socket_ = socket;
	if(socket_.nonNull())
		socket_->ungracefulShutdown();
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/MyThread.h"


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
				std::string data;
				client.downloadFile(path, data);
				//conPrint(data);
				conPrintStr(".");
			}
			catch(Indigo::Exception& e)
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


#endif // USING_LIBRESSL
