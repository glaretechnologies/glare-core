/*=====================================================================
WebListenerThread.cpp
---------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "WebListenerThread.h"


#include "WebWorkerThread.h"
#include "RequestHandler.h"
#include <ConPrint.h>
#include <networking/MySocket.h>
#include <Lock.h>
#include <StringUtils.h>
#include <PlatformUtils.h>
#include <KillThreadMessage.h>
#include <Exception.h>
#include <tls.h>
#include <networking/TLSSocket.h>


namespace web
{


WebListenerThread::WebListenerThread(int listenport_, Reference<SharedRequestHandler> shared_request_handler_, struct tls_config* tls_configuration_)
:	listenport(listenport_), shared_request_handler(shared_request_handler_), tls_configuration(tls_configuration_)
{
}


WebListenerThread::~WebListenerThread()
{
}


void WebListenerThread::doRun()
{
	PlatformUtils::setCurrentThreadNameIfTestsEnabled("WebListenerThread");

	try
	{
		MySocketRef sock;

		const int MAX_NUM_ATTEMPTS = 600;
		bool bound = false;
		for(int i=0; (i<MAX_NUM_ATTEMPTS) && !should_quit; ++i)
		{
			// Create new socket
			sock = new MySocket();

			try
			{
				sock->bindAndListen(listenport, /*reuse_address=*/true);
				bound = true;
				break;
			}
			catch(MySocketExcep& e)
			{
				conPrint("bindAndListen failed: " + e.what() + ", waiting and retrying...");
				PlatformUtils::Sleep(5000);
			}
		}

		if(!bound)
			throw MySocketExcep("Failed to bind and listen.");

		conPrint("WebListenerThread: Bound socket to port " + toString(listenport) + " successfully.");

		m_sock = sock;

		int next_thread_id = 0;
		
		struct tls* tls_context = NULL;
		if(tls_configuration)
		{
			tls_context = tls_server();
			if(!tls_context)
				throw MySocketExcep("Failed to create tls_context.");
			if(tls_configure(tls_context, tls_configuration) == -1)
				throw MySocketExcep("tls_configure failed: " + getTLSErrorString(tls_context));
		}

		while(!should_quit)
		{
			try
			{
				MySocketRef plain_worker_sock = sock->acceptConnection(); // Blocks

				plain_worker_sock->enableTCPKeepAlive(30.f); // Some connections seem to get stuck doing nothing for long periods, so enable keepalive to kill them.

				// Create TLSSocket (tls_context) for worker thread/socket if this is a TLS connection.

				SocketInterfaceRef use_socket = plain_worker_sock;
				TLSSocketRef worker_tls_socket;
				if(tls_context)
				{
					struct tls* worker_tls_context = NULL;
					if(tls_accept_socket(tls_context, &worker_tls_context, (int)plain_worker_sock->getSocketHandle()) != 0)
						throw MySocketExcep("tls_accept_socket failed: " + getTLSErrorString(tls_context));

					worker_tls_socket = new TLSSocket(plain_worker_sock, worker_tls_context);
					use_socket = worker_tls_socket;
				}

				//print("WebListenerThread: Client connected from " + IPAddress::formatIPAddressAndPort(use_socket->getOtherEndIPAddress(), use_socket->getOtherEndPort()));

				Reference<RequestHandler> request_handler = this->shared_request_handler->getOrMakeRequestHandler();

				Reference<WorkerThread> worker_thread = new WorkerThread(
					next_thread_id,
					use_socket,
					request_handler,
					tls_context != NULL // tls_connection
				);

				next_thread_id++;
			
				thread_manager.addThread(worker_thread);
			}
			catch(glare::Exception& e)
			{
				// Will get this when thread creation fails.
				conPrint("WebListenerThread: caught exception: " + e.what());
			}
		}
	}
	catch(MySocketExcep& e)
	{
		conPrint("WebListenerThread: " + e.what());
	}
	catch(glare::Exception& e)
	{
		conPrint("WebListenerThread glare::Exception: " + e.what());
	}
	

	// Kill the child WorkerThread threads now
	thread_manager.killThreadsBlocking();

	conPrint("WebListenerThread terminating.");
}


void WebListenerThread::kill()
{
	should_quit = 1;

	MySocketRef sock = this->m_sock;
	if(sock)
		sock->ungracefulShutdown();
}


} // end namespace web
