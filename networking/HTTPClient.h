/*=====================================================================
HTTPClient.h
------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "SocketInterface.h"
#include <string>
#include <vector>


/*=====================================================================
HTTPClient
----------
Downloads a file with HTTP, or makes a HTTP post.
Can do HTTPS as well.
Can handle redirects.
=====================================================================*/
class HTTPClient
{
public:
	HTTPClient();
	~HTTPClient();

	enum RequestType
	{
		RequestType_Get,
		RequestType_Post
	};

	struct ResponseInfo
	{
		int response_code;
		std::string response_message;
		std::string mime_type;
	};

	std::string user_agent;

	std::vector<std::string> additional_headers; // Such as "X-CC-Api-Key: YOUR_API_KEY".  Don't include CRLF in the header.

	size_t max_data_size;
	size_t max_socket_buffer_size;

	ResponseInfo sendPost(const std::string& url, const std::string& post_content, const std::string& content_type, std::string& data_out); // Throws glare::Exception on failure.

	ResponseInfo downloadFile(const std::string& url, std::string& data_out); // Throws glare::Exception on failure.

	void kill(); // Interrupt download.  Can be called from another thread.

	static void test();

private:
	ResponseInfo doDownloadFile(const std::string& url, int num_redirects_done, std::string& data_out); // Throws glare::Exception on failure.

	size_t readUntilCRLF(size_t scan_start_index);
	size_t readUntilCRLFCRLF(size_t scan_start_index);
	HTTPClient::ResponseInfo handleResponse(size_t request_header_size, RequestType request_type, int num_redirects_done, std::string& data_out);
	std::vector<char> socket_buffer;

	SocketInterfaceRef socket;
};
