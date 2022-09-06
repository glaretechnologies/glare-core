/*=====================================================================
RequestHandler.h
----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "RequestInfo.h"
#include <SocketInterface.h>
#include <string>
#include <ThreadSafeRefCounted.h>
class WorkerThread;
class ReplyInfo;


namespace web
{


class RequestHandler;


/*=====================================================================
SharedRequestHandler
--------------------
One of these is passed to the ListenerThread.
For each incoming connection, it is in charge of making or returning a 
reference to a RequestHandler with getOrMakeRequestHandler().
=====================================================================*/
class SharedRequestHandler : public ThreadSafeRefCounted
{
public:
	virtual ~SharedRequestHandler(){}

	virtual Reference<RequestHandler> getOrMakeRequestHandler() = 0; // Factory method for request handler.
};


/*=====================================================================
RequestHandler
--------------

=====================================================================*/
class RequestHandler : public ThreadSafeRefCounted
{
public:
	RequestHandler();
	virtual ~RequestHandler();


	virtual void handleRequest(const RequestInfo& request_info, ReplyInfo& reply_info) = 0;

	// Returns true if the handler wants to completely handle the connection.
	virtual bool handleWebSocketConnection(const RequestInfo& request_info, Reference<SocketInterface>& socket) { return false; }

	virtual void handleWebsocketTextMessage(const std::string& msg, web::ReplyInfo& reply_info, const Reference<WorkerThread>& worker_thread) {}

	virtual void handleWebsocketBinaryMessage(const uint8* data, size_t len, web::ReplyInfo& reply_info, const Reference<WorkerThread>& worker_thread) {}

	virtual void websocketConnectionClosed(Reference<SocketInterface>& socket, const Reference<WorkerThread>& worker_thread) {}

private:

};


} // end namespace web
