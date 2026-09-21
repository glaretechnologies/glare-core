/*=====================================================================
ResponseUtils.h
---------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "../utils/string_view.h"
#include "../utils/Platform.h"
#include <string>


/*=====================================================================
ResponseUtils
-------------------

=====================================================================*/
namespace web
{


class RequestInfo;
class ReplyInfo;


namespace ResponseUtils
{
	void writeRawString(ReplyInfo& reply_info, const std::string& s);

	// Returns false if the value contains characters that would let it break out of its header line, in particular CR and LF.
	// Any value that is derived from a request (a URL, a cookie, a filename etc.) should be checked with this before being written into a header.
	bool isValidHeaderValue(const string_view value);

	// Writes a "name: value" header line.  Throws glare::Exception if the value is not a valid header value (see isValidHeaderValue()).
	// NOTE: writes directly to the socket, so a throw here leaves a partially-written response.  Callers that build a response from
	// several attacker-influenced values should check them with isValidHeaderValue() up front instead.
	void writeHeader(ReplyInfo& reply_info, const string_view name, const string_view value);

	// Text
	void writeHTTPOKHeaderAndData(ReplyInfo& reply_info, const void* data, size_t datalen);

	//void writeHTTPResponseHeaderAndData(ReplyInfo& reply_info, const void* data, size_t datalen);

	// With given content type
	void writeHTTPOKHeaderAndData(ReplyInfo& reply_info, const void* data, size_t datalen, const string_view content_type);
	void writeHTTPOKHeaderAndData(ReplyInfo& reply_info, const std::string& s);
	void writeHTTPOKHeaderAndDataWithCacheMaxAge(ReplyInfo& reply_info, const void* data, size_t datalen, const string_view content_type, int max_age_s);
	void writeHTTPOKHeaderAndDataWithCacheControl(ReplyInfo& reply_info, const void* data, size_t datalen, const string_view content_type, const string_view cache_control);
	void writeHTTPOKHeaderWithCacheMaxAgeAndContentEncoding(ReplyInfo& reply_info, const void* data, size_t datalen, const string_view content_type, const string_view content_encoding, int max_age_s);
	void writeHTTPOKHeaderWithCacheControlAndContentEncoding(ReplyInfo& reply_info, const void* data, size_t datalen, const string_view content_type, const string_view cache_control, const string_view content_encoding);

	void writeHTTPNotFoundHeaderAndData(ReplyInfo& reply_info, const std::string& s);
	void writeHTTPUnauthorizedHeaderAndData(ReplyInfo& reply_info, const std::string& s);


	void writeRedirectTo(ReplyInfo& reply_info, const std::string& url);


	void writeWebsocketTextMessage(ReplyInfo& reply_info, const std::string& s);
	void writeWebsocketBinaryMessage(ReplyInfo& reply_info, const uint8* data, size_t size);
	void writeWebsocketPongMessage(ReplyInfo& reply_info, const std::string& s);

	std::string getContentTypeForPath(const std::string& path);

	std::string getPrefixWithStrippedTags(const std::string& s, size_t max_len);

	void test();
}
}
