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
Downloads a file with HTTP.
Can do HTTPS as well.
Can handle redirects.
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

	ResponseInfo downloadFile(const std::string& url, std::string& data_out); // Throws glare::Exception on failure.

	void kill(); // Interrupt download.  Can be called from another thread.

	static void test();

private:
	ResponseInfo doDownloadFile(const std::string& url, int num_redirects_done, std::string& data_out); // Throws glare::Exception on failure.

	size_t readUntilCRLF(size_t scan_start_index);
	size_t readUntilCRLFCRLF(size_t scan_start_index);
	HTTPClient::ResponseInfo handleResponse(size_t request_header_size, int num_redirects_done, std::string& data_out);
	std::vector<char> socket_buffer;

	SocketInterfaceRef socket;
};
