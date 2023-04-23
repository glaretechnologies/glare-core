/*=====================================================================
TLSSocketTests.cpp
------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "TLSSocketTests.h"


#if BUILD_TESTS && USING_LIBRESSL


#include "TLSSocket.h"
#include "MyThread.h"
#include "Networking.h"
#include "../utils/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/SocketBufferOutStream.h"
#include <cstring>
#include <tls.h>
#include <openssl/err.h>


class TestServerTLSSocketThread : public MyThread
{
public:
	TestServerTLSSocketThread(TLSSocketRef socket_) : socket(socket_) {}
	~TestServerTLSSocketThread() {}

	virtual void run()
	{
		// conPrint("TestServerTLSSocketThread::run()");
		try
		{
			// Read Uint32
			testAssert(socket->readUInt32() == 1);
			testAssert(socket->readUInt32() == 2);

			ERR_remove_thread_state(/*thread id=*/NULL); // Check calling this early doesn't cause a crash

			testAssert(socket->readUInt32() == 3);

			// Keep reading until we fail to read any more
			socket->waitForGracefulDisconnect();
		}
		catch(MySocketExcep& e)
		{
			failTest("TestServerSocketThread excep: " + e.what());
		}

		// Remove thread-local error state, to avoid leakin git.
		// NOTE: have to destroy socket first, before calling ERR_remove_thread_state()
		socket = NULL;
		ERR_remove_thread_state(/*thread id=*/NULL); // Set thread ID to null to use current thread.
	}

	TLSSocketRef socket;
};


class TLSTestClientThread : public MyThread
{
public:
	TLSTestClientThread(const std::string& server_hostname_, int port_) : server_hostname(server_hostname_), port(port_) {}
	~TLSTestClientThread() {}

	virtual void run()
	{
		// conPrint("TestClientThread::run()");
		bool succeeded = false;
		for(int i=0; i<10; ++i)
		{
			try
			{
				conPrint("TestClientThread: Connecting to " + IPAddress::formatIPAddressAndPort(server_hostname, port));

				MySocketRef plain_socket = new MySocket(server_hostname, port);

				TLSConfig client_tls_config;

				// Need this otherwise LibreSSL complains about self-signed certs, and that localhost isn't on cert.
				tls_config_insecure_noverifycert(client_tls_config.config);
				tls_config_insecure_noverifyname(client_tls_config.config);

				TLSSocketRef tls_socket = new TLSSocket(plain_socket, client_tls_config.config, server_hostname);

				conPrint("TestClientThread: mysocket connected!");
				conPrint("TestClientThread: other end IP: " + plain_socket->getOtherEndIPAddress().toString());

				tls_socket->writeUInt32(1);
				tls_socket->writeUInt32(2);
				tls_socket->writeUInt32(3);

				succeeded = true;
				break;
			}
			catch(MySocketExcep& e)
			{
				conPrint("TLSTestClientThread: Exception occurred: " + e.what());
				// Server socket may not be accepting connections yet, so wait a while
				PlatformUtils::Sleep(100);
			}
		}

		if(!succeeded)
			failTest("TestClientThread: failed to connect to server");

		ERR_remove_thread_state(NULL);
	}

	std::string server_hostname;
	int port;
};


class TLSTestListenerThread : public MyThread
{
public:
	TLSTestListenerThread(int port_) : loop(false), port(port_) {}
	~TLSTestListenerThread() {}

	virtual void run()
	{
		conPrint("TestListenerThread::run() (port: " + toString(port) + ")");
		try
		{
			// Create server TLS configuration
			TLSConfig server_tls_config;

			if(tls_config_set_cert_file(server_tls_config.config, (TestUtils::getTestReposDir() + "/testfiles/tls/cert.pem").c_str()) != 0)
				throw MySocketExcep("tls_config_set_cert_file failed: " + getTLSConfigErrorString(server_tls_config.config));

			if(tls_config_set_key_file(server_tls_config.config, (TestUtils::getTestReposDir() + "/testfiles/tls/key.pem").c_str()) != 0)
				throw MySocketExcep("tls_config_set_key_file failed: " + getTLSConfigErrorString(server_tls_config.config));

			struct tls* tls_context = tls_server();
			if(!tls_context)
				throw MySocketExcep("Failed to create tls_context.");
			if(tls_configure(tls_context, server_tls_config.config) == -1)
				throw MySocketExcep("tls_configure failed: " + getTLSErrorString(tls_context));



			// Create listener socket
			MySocket listener;
			listener.bindAndListen(port);
			
		
			do
			{
				// Accept connection
				MySocketRef plain_worker_sock = listener.acceptConnection();

				struct tls* worker_tls_context = NULL;
				if(tls_accept_socket(tls_context, &worker_tls_context, (int)plain_worker_sock->getSocketHandle()) != 0)
					throw MySocketExcep("tls_accept_socket failed: " + getTLSErrorString(tls_context));

				TLSSocketRef worker_tls_socket = new TLSSocket(plain_worker_sock, worker_tls_context);

				Reference<TestServerTLSSocketThread> server_thread = new TestServerTLSSocketThread(worker_tls_socket);
				server_thread->launch();

				if(!loop)
					server_thread->join(); // Wait for server thread
			}
			while(loop);

			tls_free(tls_context);
		}
		catch(MySocketExcep& e)
		{
			failTest("TestListenerThread excep: " + e.what());
		}
	}

	bool loop;
	int port;
};


static void doTestWithHostname(const std::string& hostname, int port)
{
	Reference<TLSTestListenerThread> listener_thread = new TLSTestListenerThread(port);
	listener_thread->launch();

	Reference<TLSTestClientThread> client_thread = new TLSTestClientThread(hostname, port);
	client_thread->launch();

	listener_thread->join();
	client_thread->join();
}



//==============================================================================================================

// Does repeated connections to a server to see if any memory is leaked.
// NOTE: this test doesn't terminate currently due to listener_thread->join().
#if 0
static void doMemLeakTest()
{
	Reference<TLSTestListenerThread> listener_thread = new TLSTestListenerThread(/*port=*/5000);
	listener_thread->loop = true;
	listener_thread->launch();

	const int N = 1000;
	for(int i=0; i<N; ++i)
	{
		Reference<TLSTestClientThread> client_thread = new TLSTestClientThread("localhost", /*port=*/5000);
		client_thread->launch();

		PlatformUtils::Sleep(100);
	}

	listener_thread->join();
	//client_thread->join();
}
#endif

//==============================================================================================================



class TLSTestUseConfigThread : public MyThread
{
public:
	TLSTestUseConfigThread(struct tls_config* config_) : config(config_) {}

	virtual void run()
	{
		struct tls* tls_context = tls_client();
		testAssert(tls_context); 

		if(tls_configure(tls_context, config) != 0)
			failTest("tls_configure failed.");

		tls_free(tls_context);
	}

	struct tls_config* config;
};


static void doMultiThreadedConfigTest()
{
	struct tls_config* config = tls_config_new();

	tls_config_insecure_noverifycert(config);
	tls_config_insecure_noverifyname(config);

	std::vector<Reference<TLSTestUseConfigThread>> threads;
	for(size_t i=0; i<100; ++i)
	{
		Reference<TLSTestUseConfigThread> thread = new TLSTestUseConfigThread(config);
		threads.push_back(thread);
	}

	for(size_t i=0; i<threads.size(); ++i)
		threads[i]->launch();

	for(size_t i=0; i<threads.size(); ++i)
		threads[i]->join();

	// At this point the config should have recount == 1.
	tls_config_free(config);
}



void TLSSocketTests::test()
{
	conPrint("TLSSocketTests::test()");

	testAssert(Networking::isInitialised());

	// doMemLeakTest(); // doesn't terminate

	doMultiThreadedConfigTest();

	doTestWithHostname("localhost", /*port=*/5000);

	conPrint("TLSSocketTests::test(): done.");
}


#endif // BUILD_TESTS
