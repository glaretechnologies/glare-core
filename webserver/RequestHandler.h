/*=====================================================================
RequestHandler.h
----------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "RequestInfo.h"
#include <networking/SocketInterface.h>
#include <string>
#include <ThreadSafeRefCounted.h>
#include <Exception.h>
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

	virtual void handleWebSocketConnection(const RequestInfo& /*request_info*/, Reference<SocketInterface>& /*socket*/) { throw glare::Exception("Not handling websocket connections"); }
};


} // end namespace web
