/*=====================================================================
WorkerThread.cpp
----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "WorkerThread.h"


#include <ResponseUtils.h>
#include "RequestInfo.h"
#include "WebsiteExcep.h"
#include "Escaping.h"
#include "Log.h"
#include "RequestHandler.h"
#include <ConPrint.h>
#include <Clock.h>
#include <AESEncryption.h>
#include <SHA256.h>
#include <Base64.h>
#include <Exception.h>
#include <MySocket.h>
#include <Lock.h>
#include <StringUtils.h>
#include <PlatformUtils.h>
#include <KillThreadMessage.h>
#include <Parser.h>
#include <MemMappedFile.h>
#include <RuntimeCheck.h>
#include <openssl/err.h>


namespace web
{


static const std::string CRLF = "\r\n";
static const bool VERBOSE = false;


WorkerThread::WorkerThread(int thread_id_, const Reference<SocketInterface>& socket_, 
	const Reference<RequestHandler>& request_handler_, bool tls_connection_)
:	thread_id(thread_id_),
	socket(socket_),
	request_handler(request_handler_),
	tls_connection(tls_connection_)
{
}


WorkerThread::~WorkerThread()
{
}


// NOTE: not handling "<unit>=-<suffix-length>" form for now
void WorkerThread::parseRanges(const string_view field_value, std::vector<web::Range>& ranges_out)
{
	ranges_out.clear();

	Parser parser(field_value.data(), field_value.size());

	// Parse unit.  Just accept 'bytes' for now
	if(!parser.parseCString("bytes="))
		return;

	while(!parser.eof())
	{
		web::Range range;
		if(!parser.parseInt64(range.start))
			return;
		if(range.start < 0)
			return;
		if(!parser.parseChar('-'))
			return;
		if(parser.eof())
		{
			// "<unit>=<range-start>-" form
			range.end_incl = -1;
			ranges_out.push_back(range);
		}
		else
		{
			if(!parser.parseInt64(range.end_incl))
				return;
			if(range.end_incl < 0)
				return;
			ranges_out.push_back(range);

			parser.parseWhiteSpace(); // NOTE: this needed?
			if(parser.currentIsChar(','))
			{
				parser.consume(',');
				parser.parseWhiteSpace();
			}
			else
				break;
		}
	}
}


void WorkerThread::parseAcceptEncodings(const string_view field_value, bool& deflate_accept_encoding_out, bool& zstd_accept_encoding_out)
{
	deflate_accept_encoding_out = false;
	zstd_accept_encoding_out = false;

	// Parse accept-encodings (compression methods browser can handle)
	// Has some quality value/weight junk that we want to ignore: https://www.rfc-editor.org/rfc/rfc9110#field.accept-encoding
	
	// e.g. Accept-Encoding: gzip, deflate, br, zstd
	
	Parser parser(field_value.data(), field_value.size());

	while(1)
	{
		string_view value;
		if(parser.parseAlphaToken(value))
		{
			if(value == "deflate")
				deflate_accept_encoding_out = true;
			else if(value == "zstd")
				zstd_accept_encoding_out = true;

			// Advance past next ',' or return if EOF
			while(1)
			{
				if(parser.eof())
					return;
				else if(parser.currentIsChar(','))
				{
					parser.consume(','); // Advance past ','
					break;
				}
				else
					parser.advance();
			}

			// Consume any trailing whitespace after comma
			parser.parseWhiteSpace();
		}
		else
			return;
	}
}


// Handle a single HTTP request.
// The request header is in [socket_buffer[request_start_index], socket_buffer[request_start_index + request_header_size])
// Returns if should keep connection alive.
// Advances this->request_start_index. to index after the current request (e.g. will be at the beginning of the next request)
WorkerThread::HandleRequestResult WorkerThread::handleSingleRequest(size_t request_header_size)
{
	// conPrint(std::string(&socket_buffer[request_start_index], &socket_buffer[request_start_index] + request_header_size));

	bool keep_alive = true;

	// Parse request
	runtimeCheck(request_start_index <= socket_buffer.size());
	runtimeCheck(request_start_index + request_header_size <= socket_buffer.size());
	Parser parser(socket_buffer.data() + request_start_index, request_header_size);

	// Parse HTTP verb (GET, POST etc..)
	RequestInfo request_info;
	request_info.tls_connection = tls_connection;
	request_info.client_ip_address = socket->getOtherEndIPAddress();

	string_view verb;
	if(!parser.parseAlphaToken(verb))
		throw WebsiteExcep("Failed to parse HTTP verb");
	request_info.verb = toString(verb);
	if(!parser.parseChar(' '))
		throw WebsiteExcep("Parse error");

	// Parse URI
	string_view URI;
	if(!parser.parseNonWSToken(URI))
		throw WebsiteExcep("Failed to parse request URI");
	if(!parser.parseChar(' '))
		throw WebsiteExcep("Parse error");

	//------------- Parse HTTP version ---------------
	if(!parser.parseCString("HTTP/"))
		throw WebsiteExcep("Failed to parse HTTP version");
		
	uint32 major_version;
	if(!parser.parseUnsignedInt(major_version))
		throw WebsiteExcep("Failed to parse HTTP major version");

	// Parse '.'
	if(!parser.parseChar('.'))
		throw WebsiteExcep("Parser error");

	uint32 minor_version;
	if(!parser.parseUnsignedInt(minor_version))
		throw WebsiteExcep("Failed to parse HTTP minor version");

	if(major_version == 1 && minor_version == 0)
		keep_alive = false;

	//------------------------------------------------

	// Parse CRLF at end of request header
	if(!parser.parseString(CRLF))
		throw WebsiteExcep("Failed to parse CRLF at end of request header");

	// Print out request args:
	/*conPrint("HTTP Verb: '" + verb + "'");
	conPrint("URI: '" + URI + "'");
	conPrint("HTTP version: HTTP/" + toString(major_version) + "." + toString(minor_version));
	*/
	
	// Read header fields

	std::string websocket_key;
	std::string websocket_protocol;
	std::string encoded_websocket_reply_key;

	int content_length = -1;
	while(1)
	{
		if(parser.eof())
			throw WebsiteExcep("Parser error while parsing header fields");
		if(parser.current() == '\r')
			break;

		// Parse header field name
		string_view field_name;
		if(!parser.parseToChar(':', field_name))
			throw WebsiteExcep("Parser error while parsing header fields");
		parser.advance(); // Advance past ':'

		// If there is a space, consume it
		parser.parseChar(' ');
		
		// Parse the field value
		string_view field_value;
		if(!parser.parseToChar('\r', field_value))
			throw WebsiteExcep("Parser error while parsing header fields");

		parser.advance(); // Advance past \r
		if(!parser.parseChar('\n')) // Advance past \n
			throw WebsiteExcep("Parse error");

		Header header;
		header.key = field_name;
		header.value = field_value;
		request_info.headers.push_back(header);

		//conPrint(field_name + ": " + field_value);

		if(StringUtils::equalCaseInsensitive(field_name, "content-length"))
		{
			try
			{
				content_length = stringToInt(toString(field_value));
			}
			catch(StringUtilsExcep& e)
			{
				throw WebsiteExcep("Failed to parse content length: " + e.what());
			}
		}
		else if(StringUtils::equalCaseInsensitive(field_name, "accept-encoding"))
		{
			parseAcceptEncodings(field_value, request_info.deflate_accept_encoding, request_info.zstd_accept_encoding);
		}
		else if(StringUtils::equalCaseInsensitive(field_name, "cookie"))
		{
			// Parse cookies
			Parser cookie_parser(field_value.data(), field_value.size());
			// e.g. Cookie: CookieTestKey=CookieTestValue; email-address=nick@indigorenderer.com

			while(1)
			{
				request_info.cookies.resize(request_info.cookies.size() + 1);
				
				// Parse cookie key
				string_view key;
				if(!cookie_parser.parseToChar('=', key))
					throw WebsiteExcep("Parser error while parsing cookies");
				request_info.cookies.back().key = toString(key);
				cookie_parser.consume('='); // Advance past '='

				// Parse cookie value
				string_view value;
				cookie_parser.parseToCharOrEOF(';', value);
				request_info.cookies.back().value = toString(value);
						
				if(cookie_parser.currentIsChar(';'))
				{
					cookie_parser.consume(';'); // Advance past ';'
					cookie_parser.parseWhiteSpace();
				}
				else
				{
					break; // Finish parsing cookies.
				}
			}
		}
		else if(StringUtils::equalCaseInsensitive(field_name, "sec-websocket-key"))
		{
			websocket_key = toString(field_value);

			const std::string magic_key = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"; // From https://tools.ietf.org/html/rfc6455, section 1.3 (page 6)
			const std::string combined_key = websocket_key + magic_key;

			std::vector<unsigned char> digest;
			SHA256::SHA1Hash((const unsigned char*)combined_key.c_str(), (const unsigned char*)combined_key.c_str() + combined_key.size(), digest);

			Base64::encode(digest.data(), digest.size(), encoded_websocket_reply_key);
		}
		else if(StringUtils::equalCaseInsensitive(field_name, "sec-websocket-protocol"))
		{
			websocket_protocol = toString(field_value);
		}
		else if(StringUtils::equalCaseInsensitive(field_name, "upgrade"))
		{
			// For websockets:
			if(field_value == "websocket")
			{
				
			}
		}
		else if(StringUtils::equalCaseInsensitive(field_name, "range"))
		{
			// Parse ranges, e.g. Range: bytes=167116800-167197941  
			parseRanges(field_value, request_info.ranges);
		}
		else if(StringUtils::equalCaseInsensitive(field_name, "connection"))
		{
			if(field_value == "close")
				keep_alive = false; // Sender has indicated this is the last request if will make before closing the connection: https://datatracker.ietf.org/doc/html/rfc2616#section-14.10
		}
	}

	parser.consume('\r'); // Advance past \r
	if(!parser.parseChar('\n')) // Advance past \n
		throw WebsiteExcep("Parse error");

	// Do websockets handshake
	if(!encoded_websocket_reply_key.empty())
	{
		const std::string response = ""
			"HTTP/1.1 101 Switching Protocols\r\n"
			"Upgrade: websocket\r\n"
			"Connection: Upgrade\r\n"
			"Sec-WebSocket-Accept: " + encoded_websocket_reply_key + "\r\n"
			"Sec-WebSocket-Protocol: " + websocket_protocol + "\r\n"
			"Cache-Control: no-cache\r\n"
			"Pragma:no-cache\r\n"
			"\r\n";

		socket->writeData(response.c_str(), response.size());

		// Advance request_start_index to point to after end of this post body.
		request_start_index += request_header_size; // TODO: skip over content as well (if content length > 0)?

		handleWebsocketConnection(request_info); // May throw exception

		return HandleRequestResult_ConnectionHandledElsewhere;
	}

		
	// Get part of URI before CGI string (before ?)
	Parser uri_parser(URI.data(), URI.size());
	
	string_view path;
	uri_parser.parseToCharOrEOF('?', path);
	request_info.path = toString(path);

	uri_parser.parseChar('?'); // Advance past '?' if present.

	// Parse URL parameters (stuff after '?')
	while(!uri_parser.eof())
	{
		request_info.URL_params.resize(request_info.URL_params.size() + 1);

		// Parse key
		string_view escaped_key;
		if(!uri_parser.parseToChar('=', escaped_key))
			throw WebsiteExcep("Parser error while parsing URL params");
		request_info.URL_params.back().key = Escaping::URLUnescape(toString(escaped_key));
		uri_parser.consume('='); // Advance past '='

		// Parse value
		string_view escaped_value;
		uri_parser.parseToCharOrEOF('&', escaped_value);
		request_info.URL_params.back().value = Escaping::URLUnescape(toString(escaped_value));

		if(uri_parser.currentIsChar('&'))
			uri_parser.advance(); // Advance past '&'
		else
			break; // Finish parsing URL params.
	}

	ReplyInfo reply_info;
	reply_info.socket = socket.getPointer();

	if(request_info.verb == "GET")
	{
		//conPrint("thread_id " + toString(thread_id) + ": got GET request, path: " + path); //TEMP

		request_handler->handleRequest(request_info, reply_info);

		// Advance request_start_index to point to after end of this post body.
		request_start_index += request_header_size;
	}
	else if(request_info.verb == "POST")
	{
		// From HTTP 1.1 RFC: "Any Content-Length greater than or equal to zero is a valid value."  (http://www.w3.org/Protocols/rfc2616/rfc2616-sec14.html#sec14.13)
		if(content_length < 0)
			throw WebsiteExcep("Invalid content length");

		const size_t MAX_CONTENT_LENGTH = 10000000; // 10 MB or so.
		if(content_length > (int)MAX_CONTENT_LENGTH)
			throw WebsiteExcep("Invalid content length: " + toString(content_length));

		const size_t total_msg_size = request_header_size + (size_t)content_length;

		if(content_length > 0)
		{
			const size_t required_buffer_size = request_start_index + total_msg_size;
			if(socket_buffer.size() < required_buffer_size) // If we haven't read the entire post body yet
			{
				// Read remaining data
				const size_t current_buf_size = socket_buffer.size();
				socket_buffer.resize(required_buffer_size);

				socket->readData(&socket_buffer[current_buf_size], required_buffer_size - current_buf_size);
			}

			// Read post content from socket
			request_info.post_content.resize(content_length);
			runtimeCheck((request_start_index + request_header_size + (size_t)content_length) <= socket_buffer.size());
			std::memcpy(&request_info.post_content[0], &socket_buffer[request_start_index + request_header_size], content_length);

			//conPrint("Read content:");
			//conPrint(post_content);

			// Parse form data.  NOTE: We don't always want to do this presumably, we're not always handling form posts.
			Parser form_parser(request_info.post_content.c_str(), request_info.post_content.size());

			while(!form_parser.eof())
			{
				request_info.post_fields.resize(request_info.post_fields.size() + 1);

				// Parse key
				string_view escaped_key;
				if(!form_parser.parseToChar('=', escaped_key))
					throw WebsiteExcep("Parser error while parsing URL params");
				request_info.post_fields.back().key = Escaping::URLUnescape(toString(escaped_key));

				form_parser.consume('='); // Advance past '='

				// Parse value
				string_view escaped_value;
				form_parser.parseToCharOrEOF('&', escaped_value);
				request_info.post_fields.back().value = Escaping::URLUnescape(toString(escaped_value));
						
				if(form_parser.currentIsChar('&'))
					form_parser.consume('&'); // Advance past '&'
				else
					break; // Finish parsing URL params.
			}
		}

		request_handler->handleRequest(request_info, reply_info);


		// Advance request_start_index to point to after end of this post body.
		request_start_index += total_msg_size;
	}
	else
	{
		throw WebsiteExcep("Unhandled verb " + request_info.verb);
	}

	return keep_alive ? HandleRequestResult_KeepAlive : HandleRequestResult_Finished;
}

/*
moveToFrontOfBuffer():

Move bytes [request_start, socket_buffer.size()) to [0, socket_buffer.size() - request_start),
then resize buffer to socket_buffer.size() - request_start.



|         req 0                    |         req 1            |                req 2
|----------------------------------|--------------------------|--------------------------------
															  ^               ^               ^
										   request start index    double_crlf_scan_position   socket_buffer.size()

=>


				   |                req 2
				   |--------------------------------
				   ^               ^               ^
request start index    double_crlf_scan_position   socket_buffer.size()

*/
static void moveToFrontOfBuffer(std::vector<char>& socket_buffer, size_t request_start)
{
	assert(request_start < socket_buffer.size());
	if(request_start < socket_buffer.size())
	{
		const size_t len = socket_buffer.size() - request_start; // num bytes to copy
		for(size_t i=0; i<len; ++i)
			socket_buffer[i] = socket_buffer[request_start + i];

		socket_buffer.resize(len);
	}
}


void WorkerThread::handleWebsocketConnection(RequestInfo& request_info)
{
	if(VERBOSE) print("WorkerThread: Connection upgraded to websocket connection.");

	socket->setNoDelayEnabled(true); // For websocket connections, we will want to send out lots of little packets with low latency.  So disable Nagle's algorithm, e.g. send coalescing.
	socket->enableTCPKeepAlive(30.0f); // Keep alive the connection.

	this->request_handler->handleWebSocketConnection(request_info, socket);
}


// This main loop code is in a separate function so we can easily return from it, while still excecuting the thread cleanup code in doRun().
void WorkerThread::doRunMainLoop()
{
	request_start_index = 0; // Index in socket_buffer of the start of the current request.
	size_t double_crlf_scan_position = 0; // Index in socket_buffer of the last place we looked for a double CRLF.
	// Loop to handle multiple requests (HTTP persistent connection)
	while(true)
	{
		// Read up to 'read_chunk_size' bytes of data from the socket.  Note that we may read multiple requests at once.
		const size_t old_socket_buffer_size = socket_buffer.size();
		const size_t read_chunk_size = 2048;
		socket_buffer.resize(old_socket_buffer_size + read_chunk_size);
		const size_t num_bytes_read = socket->readSomeBytes(&socket_buffer[old_socket_buffer_size], read_chunk_size); // Read up to 'read_chunk_size' bytes.
		//print("thread_id " + toString(thread_id) + ": read " + toString(num_bytes_read) + " bytes.");
		if(num_bytes_read == 0) // if connection was closed gracefully
			return;
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
							
					// Process the request:
					const size_t request_header_size = request_header_end - request_start_index;
					const HandleRequestResult result = handleSingleRequest(request_header_size); // Advances this->request_start_index. to index after the current request (e.g. will be at the beginning of the next request)

					double_crlf_scan_position = request_start_index;

					if(result != HandleRequestResult_KeepAlive)
					{
						// If result is HandleRequestResult_ConnectionHandledElsewhere, then another thread might be still using the socket.  So don't call startGracefulShutdown() on it.
						if(result == HandleRequestResult_Finished)
						{
							socket->startGracefulShutdown(); // Tell sockets lib to send a FIN packet to the client.
							socket->waitForGracefulDisconnect(); // Wait for a FIN packet from the client. (indicated by recv() returning 0).  We can then close the socket without going into a wait state.
						}
						return;
					}
				}

		assert(double_crlf_scan_position >= request_start_index);
			
		// If the latest request does not start at byte zero in the buffer,
		// And there is some data stored for the request to actually move,
		// then move the remaining data in the buffer to the start of the buffer.
		if(request_start_index > 0 && request_start_index < socket_buf_size)
		{
			const size_t old_request_start_index = request_start_index;
			moveToFrontOfBuffer(socket_buffer, request_start_index);
			request_start_index = 0;
			double_crlf_scan_position -= old_request_start_index;
		}
	}
}


void WorkerThread::doRun()
{
	try
	{
		doRunMainLoop();
	}
	catch(MySocketExcep& )
	{
		//print("thread_id " + toString(thread_id) + ": Socket error: " + e.what());
	}
	catch(WebsiteExcep& )
	{
		//print("WebsiteExcep: " + e.what());
	}
	catch(glare::Exception& )
	{
		//print("glare::Exception: " + e.what());
	}
	catch(std::exception& e) // catch std::bad_alloc etc..
	{
		print(std::string("Caught std::exception: ") + e.what());
	}

	// Remove thread-local OpenSSL error state, to avoid leaking it.
	// NOTE: have to destroy socket first, before calling ERR_remove_thread_state(), otherwise memory will just be reallocated.
	socket = NULL;
	ERR_remove_thread_state(/*thread id=*/NULL); // Set thread ID to null to use current thread.
}


} // end namespace web
