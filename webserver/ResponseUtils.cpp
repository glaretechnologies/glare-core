/*=====================================================================
ResponseUtils.cpp
-----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "ResponseUtils.h"


#include <ConPrint.h>
#include <Clock.h>
#include <Exception.h>
#include <networking/MySocket.h>
#include <Lock.h>
#include <StringUtils.h>
#include <PlatformUtils.h>
#include "RequestInfo.h"
#include "Escaping.h"
#include "../maths/mathstypes.h"
#include <cstring>


namespace web
{
namespace ResponseUtils
{


const std::string CRLF = "\r\n";


void writeRawString(ReplyInfo& reply_info, const std::string& s)
{
	reply_info.socket->writeData(s.c_str(), s.size());
}


void writeData(ReplyInfo& reply_info, const void* data, size_t datalen)
{
	reply_info.socket->writeData(data, datalen);
}


// Text
void writeHTTPOKHeaderAndData(ReplyInfo& reply_info, const void* data, size_t datalen)
{
	const std::string response = 
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/html; charset=utf-8\r\n"
		"Connection: Keep-Alive\r\n"
		"Content-Length: " + toString(datalen) + "\r\n"
		"\r\n";

	writeRawString(reply_info, response);
	writeData(reply_info, data, datalen);
}


// With given content type
void writeHTTPOKHeaderAndData(ReplyInfo& reply_info, const void* data, size_t datalen, const string_view content_type)
{
	const std::string response = 
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: " + toString(content_type) + "\r\n"
		"Connection: Keep-Alive\r\n"
		"Content-Length: " + toString(datalen) + "\r\n"
		"\r\n";

	writeRawString(reply_info, response);
	writeData(reply_info, data, datalen);
}


void writeHTTPOKHeaderAndData(ReplyInfo& reply_info, const std::string& s)
{
	writeHTTPOKHeaderAndData(reply_info, s.c_str(), s.size());
}


void writeHTTPOKHeaderAndDataWithCacheMaxAge(ReplyInfo& reply_info, const void* data, size_t datalen, const string_view content_type, int max_age_s)
{
	const std::string response = 
		"HTTP/1.1 200 OK\r\n"
		"Cache-Control: max-age=" + toString(max_age_s) + "\r\n"
		"Content-Type: " + toString(content_type) + "\r\n"
		"Cross-Origin-Opener-Policy: same-origin\r\n" // To enable SharedArrayBuffer in js, see https://web.dev/articles/cross-origin-isolation-guide.  For webclient.
		"Cross-Origin-Embedder-Policy: require-corp\r\n"
		"Connection: Keep-Alive\r\n"
		"Content-Length: " + toString(datalen) + "\r\n"
		"\r\n";

	writeRawString(reply_info, response);
	writeData(reply_info, data, datalen);
}


void writeHTTPOKHeaderAndDataWithCacheControl(ReplyInfo& reply_info, const void* data, size_t datalen, const string_view content_type, const string_view cache_control)
{
	const std::string response = 
		"HTTP/1.1 200 OK\r\n"
		"Cache-Control: " + toString(cache_control) + "\r\n"
		"Content-Type: " + toString(content_type) + "\r\n"
		"Cross-Origin-Opener-Policy: same-origin\r\n" // To enable SharedArrayBuffer in js, see https://web.dev/articles/cross-origin-isolation-guide.  For webclient.
		"Cross-Origin-Embedder-Policy: require-corp\r\n"
		"Connection: Keep-Alive\r\n"
		"Content-Length: " + toString(datalen) + "\r\n"
		"\r\n";

	writeRawString(reply_info, response);
	writeData(reply_info, data, datalen);
}


void writeHTTPOKHeaderWithCacheMaxAgeAndContentEncoding(ReplyInfo& reply_info, const void* data, size_t datalen, const string_view content_type, const string_view content_encoding, int max_age_s)
{
	const std::string response = 
		"HTTP/1.1 200 OK\r\n"
		"Cache-Control: max-age=" + toString(max_age_s) + "\r\n"
		"Content-Type: " + toString(content_type) + "\r\n"
		"Cross-Origin-Opener-Policy: same-origin\r\n" // To enable SharedArrayBuffer in js, see https://web.dev/articles/cross-origin-isolation-guide.  For webclient.
		"Cross-Origin-Embedder-Policy: require-corp\r\n"
		"Connection: Keep-Alive\r\n"
		"Content-Encoding: " + toString(content_encoding) + "\r\n"
		"Content-Length: " + toString(datalen) + "\r\n"
		"\r\n";

	writeRawString(reply_info, response);
	writeData(reply_info, data, datalen);
}


void writeHTTPOKHeaderWithCacheControlAndContentEncoding(ReplyInfo& reply_info, const void* data, size_t datalen, const string_view content_type, const string_view cache_control, const string_view content_encoding)
{
	const std::string response = 
		"HTTP/1.1 200 OK\r\n"
		"Cache-Control: " + toString(cache_control) + "\r\n"
		"Content-Type: " + toString(content_type) + "\r\n"
		"Cross-Origin-Opener-Policy: same-origin\r\n" // To enable SharedArrayBuffer in js, see https://web.dev/articles/cross-origin-isolation-guide.  For webclient.
		"Cross-Origin-Embedder-Policy: require-corp\r\n"
		"Connection: Keep-Alive\r\n"
		"Content-Encoding: " + toString(content_encoding) + "\r\n"
		"Content-Length: " + toString(datalen) + "\r\n"
		"\r\n";

	writeRawString(reply_info, response);
	writeData(reply_info, data, datalen);
}


void writeRedirectTo(ReplyInfo& reply_info, const std::string& url)
{
	const std::string response = 
		"HTTP/1.1 302 Redirect\r\n"
		"Location: " + url + "\r\n"
		"Content-Length: 0\r\n"
		"\r\n";

	writeRawString(reply_info, response);
}


void writeHTTPNotFoundHeaderAndData(ReplyInfo& reply_info, const std::string& s)
{
	const std::string response = 
		"HTTP/1.1 404 Not Found\r\n"
		"Content-Length: " + toString(s.length()) + "\r\n"
		"\r\n";

	writeRawString(reply_info, response);
	writeData(reply_info, s.c_str(), s.size());
}


void writeHTTPUnauthorizedHeaderAndData(ReplyInfo& reply_info, const std::string& s)
{
	const std::string response = 
		"HTTP/1.1 401 Unauthorized\r\n"
		"Content-Length: " + toString(s.length()) + "\r\n"
		"\r\n";

	writeRawString(reply_info, response);
	writeData(reply_info, s.c_str(), s.size());
}


void writeWebsocketTextMessage(ReplyInfo& reply_info, const std::string& s)
{
	const uint32 fin = 0x80;
	const uint32 opcode = 0x1;

	// TODO: handle frames > 2^16 bytes.

	const size_t data_size = (s.size() <= 125) ? 2 + s.size() : 4 + s.size();
	std::vector<unsigned char> data(data_size);
	data[0] = fin | opcode;

	// Write payload len
	if(s.size() <= 125)
	{
		data[1] = (unsigned char)s.size();

		std::memcpy(&data[2], s.c_str(), s.size()); // Append s to data
	}
	else
	{
		data[1] = 126;
		data[2] = (unsigned char)(s.size() >> 8);
		data[3] = (unsigned char)(s.size() & 0xFF);

		std::memcpy(&data[4], s.c_str(), s.size()); // Append s to data
	}

	// Write data to the socket
	writeData(reply_info, data.data(), data_size);
}


void writeWebsocketBinaryMessage(ReplyInfo& reply_info, const uint8* data, size_t size)
{
	const uint32 fin = 0x80;
	const uint32 opcode = 0x2;

	// TODO: handle frames > 2^16 bytes.
	assert(size < 65536);

	const size_t encoded_data_size = (size <= 125) ? 2 + size : 4 + size;
	std::vector<unsigned char> encoded_data(encoded_data_size);
	encoded_data[0] = fin | opcode;

	// Write payload len
	if(size <= 125)
	{
		encoded_data[1] = (unsigned char)size;

		std::memcpy(&encoded_data[2], data, size); // Append s to data
	}
	else
	{
		encoded_data[1] = 126;
		encoded_data[2] = (unsigned char)(size >> 8);
		encoded_data[3] = (unsigned char)(size & 0xFF);

		std::memcpy(&encoded_data[4], data, size); // Append s to data
	}

	// Write data to the socket
	writeData(reply_info, encoded_data.data(), encoded_data_size);
}


void writeWebsocketPongMessage(ReplyInfo& reply_info, const std::string& s)
{
	const uint32 fin = 0x80;
	const uint32 opcode = 0xA;

	// TODO: handle frames > 2^16 bytes.

	const size_t data_size = (s.size() <= 125) ? 2 + s.size() : 4 + s.size();
	std::vector<unsigned char> data(data_size);
	data[0] = fin | opcode;

	// Write payload len
	if(s.size() <= 125)
	{
		data[1] = (unsigned char)s.size();

		std::memcpy(&data[2], s.c_str(), s.size()); // Append s to data
	}
	else
	{
		data[1] = 126;
		data[2] = (unsigned char)(s.size() >> 8);
		data[3] = (unsigned char)(s.size() & 0xFF);

		std::memcpy(&data[4], s.c_str(), s.size()); // Append s to data
	}

	// Write data to the socket
	writeData(reply_info, data.data(), data_size);
}


std::string getContentTypeForPath(const std::string& path)
{
	if(::hasExtension(path, "jpg") || ::hasExtension(path, "jpeg"))
		return "image/jpeg";
	else if(::hasExtension(path, "png"))
		return "image/png";
	else if(::hasExtension(path, "gif"))
		return "image/gif";
	else if(::hasExtension(path, "pdf"))
		return "application/pdf";
	else if(::hasExtension(path, "mp4"))
		return "video/mp4";
	else if(::hasExtension(path, "css"))
		return "text/css";
	else if(::hasExtension(path, "js"))
		return "text/javascript";
	else if(::hasExtension(path, "wasm"))
		return "application/wasm";
	else if(::hasExtension(path, "html"))
		return "text/html; charset=UTF-8";
	else if(::hasExtension(path, "exr"))
		return "image/x-exr";
	else if(::hasExtension(path, "data"))
		return "application/octet-stream";
	else
		return "text/plain"; // Unknown, just return as text.

}


std::string getPrefixWithStrippedTags(const std::string& s, size_t max_len)
{
	const size_t s_size = s.size();
	std::string res;
	res.reserve(myMin(s_size, max_len));
	size_t res_len = 0;

	bool in_tag = false;
	for(size_t i=0; (i < s_size) && (res_len < max_len); ++i)
	{
		const char s_i = s[i];
		if(s_i == '<')
			in_tag = true;
		else if(s_i == '>')
			in_tag = false;
		else
		{
			if(!in_tag)
			{
				res.push_back(s_i);
				res_len++;
			}
		}
	}
	return res;
}


} // end namespace ResponseUtils
} // end namespace web


#if BUILD_TESTS


#include "../utils/TestUtils.h"


void web::ResponseUtils::test()
{
	testAssert(getPrefixWithStrippedTags("", /*max len=*/0) == "");
	testAssert(getPrefixWithStrippedTags("", /*max len=*/100) == "");

	testAssert(getPrefixWithStrippedTags("abc", /*max len=*/0) == "");
	testAssert(getPrefixWithStrippedTags("abc", /*max len=*/100) == "abc");

	testAssert(getPrefixWithStrippedTags("<a>", /*max len=*/0) == "");
	testAssert(getPrefixWithStrippedTags("<a>", /*max len=*/100) == "");
	
	testAssert(getPrefixWithStrippedTags("<a>bc", /*max len=*/0) == "");
	testAssert(getPrefixWithStrippedTags("<a>bc", /*max len=*/100) == "bc");
	testAssert(getPrefixWithStrippedTags("<a>bc", /*max len=*/1) == "b");

	testAssert(getPrefixWithStrippedTags("hello<a>bc<a>there", /*max len=*/0) == "");
	testAssert(getPrefixWithStrippedTags("hello<a>bc<a>there", /*max len=*/100) == "hellobcthere");
	
	testAssert(getPrefixWithStrippedTags("hello<a there", /*max len=*/100) == "hello");
}


#endif