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


	void writeRedirectTo(ReplyInfo& reply_info, const std::string& url);


	void writeWebsocketTextMessage(ReplyInfo& reply_info, const std::string& s);
	void writeWebsocketBinaryMessage(ReplyInfo& reply_info, const uint8* data, size_t size);
	void writeWebsocketPongMessage(ReplyInfo& reply_info, const std::string& s);

	std::string getContentTypeForPath(const std::string& path);

	std::string getPrefixWithStrippedTags(const std::string& s, size_t max_len);

	void test();
}
}
