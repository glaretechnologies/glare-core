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
struct tls_config;


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
	WebListenerThread(int listenport, Reference<SharedRequestHandler> shared_request_handler, struct ::tls_config* tls_configuration);

	virtual ~WebListenerThread();

	virtual void doRun() override;

	virtual void kill() override;

private:
	int listenport;

	// Child threads are
	// * WorkerThread's
	ThreadManager thread_manager;

	Reference<SharedRequestHandler> shared_request_handler;

	struct tls_config* tls_configuration;

	Reference<MySocket> m_sock;
	glare::AtomicInt should_quit;
};


} // end namespace web
