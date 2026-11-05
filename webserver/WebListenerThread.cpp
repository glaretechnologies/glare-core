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


WebListenerThread::WebListenerThread(int listenport_, Reference<SharedRequestHandler> shared_request_handler_, const Reference<CurrentTLSServerContextPointer>& current_tls_context_ptr_)
:	listenport(listenport_), shared_request_handler(shared_request_handler_), current_tls_context_ptr(current_tls_context_ptr_)
{
}


WebListenerThread::~WebListenerThread()
{
}


void WebListenerThread::doRun()
{
	PlatformUtils::setCurrentThreadName("WebListenerThread");

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
		
		while(!should_quit)
		{
			try
			{
				MySocketRef plain_worker_sock = sock->acceptConnection(); // Blocks

				plain_worker_sock->enableTCPKeepAlive(30.f); // Some connections seem to get stuck doing nothing for long periods, so enable keepalive to kill them.

				// Create TLSSocket (tls_context) for worker thread/socket if this is a TLS connection.

				SocketInterfaceRef use_socket = plain_worker_sock;
				TLSSocketRef worker_tls_socket;
				if(current_tls_context_ptr)
				{
					// Get the current TLSContext that should be used by the listener.  This may change occasionally when the TLS configuration changes (new keys etc.)
					TLSContextRef cur_listener_context = current_tls_context_ptr->getCurrentContext();

					TLSContextRef worker_tls_context = new TLSContext();
					if(tls_accept_socket(cur_listener_context->context, &worker_tls_context->context, (int)plain_worker_sock->getSocketHandle()) != 0)
						throw MySocketExcep("tls_accept_socket failed: " + getTLSErrorString(cur_listener_context->context));

					worker_tls_context->server_context = cur_listener_context;

					worker_tls_socket = new TLSSocket(plain_worker_sock, worker_tls_context);
					use_socket = worker_tls_socket;
				}

				//print("WebListenerThread: Client connected from " + IPAddress::formatIPAddressAndPort(use_socket->getOtherEndIPAddress(), use_socket->getOtherEndPort()));

				Reference<RequestHandler> request_handler = this->shared_request_handler->getOrMakeRequestHandler();

				Reference<WorkerThread> worker_thread = new WorkerThread(
					next_thread_id,
					use_socket,
					request_handler,
					current_tls_context_ptr.nonNull() // tls_connection
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
