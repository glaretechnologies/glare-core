/*=====================================================================
WebListenerThread.h
-------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include <MessageableThread.h>
#include <Reference.h>
#include <Platform.h>
#include <ThreadManager.h>
#include <AtomicInt.h>
class PrintOutput;
class ThreadMessageSink;
class MySocket;
class CurrentTLSServerContextPointer;


namespace web
{


class SharedRequestHandler;


/*=====================================================================
WebListenerThread
-----------------

=====================================================================*/
class WebListenerThread : public MessageableThread
{
public:
	WebListenerThread(int listenport, Reference<SharedRequestHandler> shared_request_handler, const Reference<CurrentTLSServerContextPointer>& current_tls_context_ptr);

	virtual ~WebListenerThread();

	virtual void doRun() override;

	virtual void kill() override;

private:
	int listenport;

	// Child threads are
	// * WorkerThread's
	ThreadManager thread_manager;

	Reference<SharedRequestHandler> shared_request_handler;

	Reference<CurrentTLSServerContextPointer> current_tls_context_ptr;

	Reference<MySocket> m_sock;
	glare::AtomicInt should_quit;
};


} // end namespace web
