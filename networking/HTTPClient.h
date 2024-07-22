/*=====================================================================
HTTPClient.h
------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "SocketInterface.h"
#include "../utils/Exception.h"
#include <string>
#include <vector>


class StreamingDataHandler
{
public:
	virtual void haveContentLength(uint64 /*content_length*/) {};
	virtual void handleData(ArrayRef<uint8> data) = 0;
};


class HTTPClientExcep : public glare::Exception
{
public:
	enum ExcepType
	{
		ExcepType_ConnectionClosedGracefully,
		ExcepType_Other
	};

	HTTPClientExcep(const std::string& message_, ExcepType excep_type_ = ExcepType_Other) : glare::Exception(message_), excep_type(excep_type_) {}

	ExcepType excepType() const { return excep_type; }
private:
	ExcepType excep_type;
};



/*=====================================================================
HTTPClient
----------
Downloads a file with HTTP 1.1, or makes a HTTP 1.1 post.
Can do HTTPS as well.
Can handle redirects.
=====================================================================*/
class HTTPClient : public ThreadSafeRefCounted
{
public:
	HTTPClient();
	~HTTPClient();

	void connectAndEnableKeepAlive(const std::string& protocol, const std::string& hostname, int port); // Port = -1 means use default port.
	void resetConnection();

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

	size_t max_data_size; // Maximum accepted size for content-length, also max size data_out will be resized to in sendPost() and downloadFile() functions.
	size_t max_socket_buffer_size; // Maximum size that the internal buffer socket_buffer can be resized to.  Chunked transfer encoding will 
	// fail with an error message if a chunk exceeds this size.

	SocketInterfaceRef test_socket;

	ResponseInfo sendPost(const std::string& url, const std::string& post_content, const std::string& content_type, StreamingDataHandler& response_data_handler); // Throws glare::Exception on failure.
	ResponseInfo sendPost(const std::string& url, const std::string& post_content, const std::string& content_type, std::vector<uint8>& data_out); // Throws glare::Exception on failure.
	

	ResponseInfo downloadFile(const std::string& url, StreamingDataHandler& response_data_handler); // Throws glare::Exception on failure.
	ResponseInfo downloadFile(const std::string& url, std::vector<uint8>& data_out); // Throws glare::Exception on failure.
	

	void kill(); // Interrupt download.  Can be called from another thread.

	static void test();

private:
	void connect(const std::string& protocol, const std::string& hostname, int port); // Port = -1 means use default port for protocol.
	ResponseInfo doDownloadFile(const std::string& url, int num_redirects_done, StreamingDataHandler& response_data_handler); // Throws glare::Exception on failure.

	size_t readUntilCRLF(size_t scan_start_index);
	size_t readUntilCRLFCRLF(size_t scan_start_index);
	HTTPClient::ResponseInfo handleResponse(size_t request_header_size, RequestType request_type, int num_redirects_done, StreamingDataHandler& response_data_handler);
	std::vector<uint8> socket_buffer;

	SocketInterfaceRef socket;

	std::string connected_scheme;
	std::string connected_hostname;
	int connected_port;

	bool keepalive_socket;
};
