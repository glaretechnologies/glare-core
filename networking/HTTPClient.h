/*=====================================================================
HTTPClient.h
------------
Copyright Glare Technologies Limited 2018 -
Generated at 2018-02-01 23:26:27 +1300
=====================================================================*/
#pragma once


#include "SocketInterface.h"
#include <string>
#include <vector>


/*=====================================================================
HTTPClient
----------
Downloads a file with HTTP.
=====================================================================*/
class HTTPClient
{
public:
	HTTPClient();
	~HTTPClient();


	struct ResponseInfo
	{
		int response_code;
		std::string response_message;
		std::string mime_type;
	};

	ResponseInfo downloadFile(const std::string& url, std::string& data_out); // Throws Indigo::Exception on failure.

	static void test();

private:
	void handleResponse(size_t request_header_size, std::string& data_out, ResponseInfo& file_info);
	std::vector<char> socket_buffer;

	SocketInterfaceRef socket;
};
