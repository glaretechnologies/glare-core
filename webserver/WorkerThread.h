/*=====================================================================
WorkerThread.h
--------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include <MessageableThread.h>
#include <Platform.h>
#include <MyThread.h>
#include <EventFD.h>
#include <ThreadManager.h>
#include <SocketInterface.h>
#include <set>
#include <string>
#include <vector>
#include <string_view.h>
class WorkUnit;
class PrintOutput;
class ThreadMessageSink;
class DataStore;


namespace web
{


class RequestInfo;
class RequestHandler;
class Range;


/*=====================================================================
WorkerThread
------------
Webserver worker thread
=====================================================================*/
class WorkerThread : public MessageableThread
{
public:
	// May throw glare::Exception from constructor if EventFD init fails.
	WorkerThread(int thread_id, const Reference<SocketInterface>& socket, 
		const Reference<RequestHandler>& request_handler, bool tls_connection);

	virtual ~WorkerThread();

	virtual void doRun();

	friend class WorkerThreadTests;


	void enqueueDataToSend(const std::string& data); // threadsafe

	
	void doRunMainLoop();
private:
	bool handleWebsocketConnection(RequestInfo& request_info); // return true if connection handled by handler
	
	// Returns if should keep connection alive
	enum HandleRequestResult
	{
		HandleRequestResult_KeepAlive,
		HandleRequestResult_Finished,
		HandleRequestResult_ConnectionHandledElsewhere
	};
	HandleRequestResult handleSingleRequest(size_t request_header_size);
public:
	static void parseRanges(const string_view field_value, std::vector<web::Range>& ranges_out); // Just public for testing
private:

	ThreadSafeQueue<std::string> data_to_send;

	int thread_id;
	
	Reference<SocketInterface> socket;
	
	std::vector<char> socket_buffer;
	Reference<RequestHandler> request_handler;
	size_t request_start_index; // Start index of request that we current processing.
	bool is_websocket_connection;

	bool tls_connection;
	
	EventFD event_fd;
};


} // end namespace web
