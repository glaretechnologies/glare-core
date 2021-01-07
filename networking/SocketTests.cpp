/*=====================================================================
SocketTests.cpp
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-01-30 13:47:58 +0000
=====================================================================*/
#include "SocketTests.h"


#include "MySocket.h"
#include "MyThread.h"
#include "Networking.h"
#include "../indigo/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/SocketBufferOutStream.h"
#include <cstring>


#if BUILD_TESTS


static const std::vector<int32> makeBuffer()
{
	std::vector<int32> b;
	b.push_back(1);
	b.push_back(2);
	b.push_back(3);
	return b;
}



class TestServerSocketThread : public MyThread
{
public:
	TestServerSocketThread(MySocketRef socket_) : socket(socket_) {}
	~TestServerSocketThread() {}

	virtual void run()
	{
		// conPrint("TestServerSocketThread::run()");
		try
		{
			// Read Uint32
			testAssert(socket->readUInt32() == 1);
			testAssert(socket->readUInt32() == 2);
			testAssert(socket->readUInt32() == 3);

			// Read int32
			testAssert(socket->readInt32() == 1);
			testAssert(socket->readInt32() == -2);
			testAssert(socket->readInt32() == 3);

			// Read Uint64
			testAssert(socket->readUInt64() == 1);
			testAssert(socket->readUInt64() == 2);
			testAssert(socket->readUInt64() == 3);
			testAssert(socket->readUInt64() == 0x1234567800112233ULL);

			// Read strings
			testAssert(socket->readStringLengthFirst(/*max string length=*/10000) == "hello");
			testAssert(socket->readStringLengthFirst(/*max string length=*/10000) == "world");

			// Read double
			testAssert(socket->readDouble() == 0.0);
			testAssert(socket->readDouble() == 1.23456789112233445566);

			//=========== Test data from SocketBufferOutStream ===========

			// Read Uint32
			testAssert(socket->readUInt32() == 1);
			testAssert(socket->readUInt32() == 2);
			testAssert(socket->readUInt32() == 3);

			// Read int32
			testAssert(socket->readInt32() == 1);
			testAssert(socket->readInt32() == -2);
			testAssert(socket->readInt32() == 3);

			// Read Uint64
			testAssert(socket->readUInt64() == 1);
			testAssert(socket->readUInt64() == 2);
			testAssert(socket->readUInt64() == 3);
			testAssert(socket->readUInt64() == 0x1234567800112233ULL);

			// Read strings
			testAssert(socket->readStringLengthFirst(/*max string length=*/10000) == "hello");
			testAssert(socket->readStringLengthFirst(/*max string length=*/10000) == "world");

			// Read double
			testAssert(socket->readDouble() == 0.0);
			testAssert(socket->readDouble() == 1.23456789112233445566);


			//=========== End Test data from SocketBufferOutStream ===========

			// Read strings
			testAssert(socket->readString(1000) == "hello1");
			testAssert(socket->readString(1000) == "world1");

			// Read a buffer of 3 int32s
			std::vector<int32> buf(3);
			socket->readData(&buf[0], sizeof(int32) * buf.size());
			testAssert(buf == makeBuffer());

			// Keep reading until we fail to read any more
			socket->waitForGracefulDisconnect();
		}
		catch(MySocketExcep& e)
		{
			failTest("TestServerSocketThread excep: " + e.what());
		}
	}

	MySocketRef socket;
};


class TestClientThread : public MyThread
{
public:
	TestClientThread(const std::string& server_hostname_, int port_) : server_hostname(server_hostname_), port(port_) {}
	~TestClientThread() {}

	// socket should be connected
	void doReadingAndWriting(MySocket& socket)
	{
		try
		{
			// Write Uint32
			//socket.writeUInt32(1); // already done
			socket.writeUInt32(2);
			socket.writeUInt32(3);

			// Write Int32
			socket.writeInt32(1);
			socket.writeInt32(-2);
			socket.writeInt32(3);

			// Write Uint64
			socket.writeUInt64(1);
			socket.writeUInt64(2);
			socket.writeUInt64(3);
			socket.writeUInt64(0x1234567800112233ULL);

			// Write some strings with writeStringLengthFirst()
			socket.writeStringLengthFirst("hello");
			socket.writeStringLengthFirst("world");

			socket.writeDouble(0.0);
			socket.writeDouble(1.23456789112233445566);

			//=========== Test SocketBufferOutStream ===========
			SocketBufferOutStream buffer(SocketBufferOutStream::DoUseNetworkByteOrder);
			buffer.writeUInt32(1);
			buffer.writeUInt32(2);
			buffer.writeUInt32(3);

			// Write Int32
			buffer.writeInt32(1);
			buffer.writeInt32(-2);
			buffer.writeInt32(3);


			// Write Uint64
			buffer.writeUInt64(1);
			buffer.writeUInt64(2);
			buffer.writeUInt64(3);
			buffer.writeUInt64(0x1234567800112233ULL);

			// Write some strings with writeStringLengthFirst()
			buffer.writeStringLengthFirst("hello");
			buffer.writeStringLengthFirst("world");

			buffer.writeDouble(0.0);
			buffer.writeDouble(1.23456789112233445566);

			// write the buffer contents to the socket
			socket.writeData(&buffer.buf[0], buffer.buf.size());
			//=========== End Test SocketBufferOutStream ===========

			// Write some strings with null termination
			socket.writeString("hello1");
			socket.writeString("world1");

			// Write a buffer of 3 int32s
			const std::vector<int32> buf = makeBuffer();
			socket.writeData(&buf[0], sizeof(int32) * buf.size());
		}
		catch(MySocketExcep& e)
		{
			failTest("TestClientThread: " + e.what());
		}
	}

	virtual void run()
	{
		// conPrint("TestClientThread::run()");
		bool succeeded = false;
		for(int i=0; i<10; ++i)
		{
			try
			{
				conPrint("TestClientThread: Connecting to " + IPAddress::formatIPAddressAndPort(server_hostname, port));

				MySocket socket(server_hostname, port);

				// Try enabling TCP keepalive
				socket.enableTCPKeepAlive(5.0);

				// Write Uint32.
				// On Linux an error connecting will show up on the first write due to use of non-blocking mode.
				// So write the first uint here.
				socket.writeUInt32(1);

				conPrint("TestClientThread: mysocket connected!");
				conPrint("TestClientThread: other end IP: " + socket.getOtherEndIPAddress().toString());

				doReadingAndWriting(socket);

				succeeded = true;
				break;
			}
			catch(MySocketExcep& )
			{
				conPrint("Exception occurred.. waiting");
				// Server socket may not be accepting connections yet, so wait a while
				PlatformUtils::Sleep(100);
			}
		}

		if(!succeeded)
			failTest("TestClientThread: failed to connect to server");
	}

	std::string server_hostname;
	int port;
};



class TestListenerThread : public MyThread
{
public:
	TestListenerThread(int port_) : port(port_) {}
	~TestListenerThread() {}

	virtual void run()
	{
		conPrint("TestListenerThread::run() (port: " + toString(port) + ")");
		try
		{
			// Create listener socket
			MySocket listener;
			listener.bindAndListen(port);
			
		
			// Accept connection
			MySocketRef server_socket = listener.acceptConnection();

			Reference<TestServerSocketThread> server_thread = new TestServerSocketThread(server_socket);
			server_thread->launch();

			// Wait for server thread
			server_thread->join();

			// Terminate thread.
		}
		catch(MySocketExcep& e)
		{
			failTest("TestListenerThread excep: " + e.what());
		}
	}

	int port;
};


static void doTestWithHostname(const std::string& hostname, int port)
{
	Reference<TestListenerThread> listener_thread = new TestListenerThread(port);
	listener_thread->launch();

	Reference<TestClientThread> client_thread = new TestClientThread(hostname, port);
	client_thread->launch();

	listener_thread->join();
	client_thread->join();
}







//==============================================================================================================


class TestServerSocketReadWriteThread : public MyThread
{
public:
	TestServerSocketReadWriteThread(MySocketRef socket_) : socket(socket_) {}

	virtual void run()
	{
		conPrint("TestServerSocketReadWriteThread::run()");
		try
		{
			socket->readUInt32();
			//socket->readUInt32();
			//socket->readUInt32();

			socket->writeUInt32(1);
			for(int i=0; i<1000000; ++i)
			{
				//conPrint("server writing uint " + toString(i) + "...");
				char buf[1024];
				std::memset(buf, 0, sizeof(buf));
				socket->writeData(buf, sizeof(buf)); // This should block eventually, when the send buffer is full.
			}
			/*socket->writeUInt32(1);
			socket->writeUInt32(2);
			socket->writeUInt32(3);*/

			// Keep reading until we fail to read any more
			socket->waitForGracefulDisconnect();
		}
		catch(MySocketExcep& e)
		{
			conPrint("TestServerSocketReadWriteThread MySocketExcep: " + e.what());
		}
	}

	MySocketRef socket;
};


class TestReadClientThread : public MyThread
{
public:
	TestReadClientThread(const std::string& hostname_, int port_) : hostname(hostname_), port(port_)
	{
		try
		{
			socket = new MySocket(hostname, port); 
		}
		catch(MySocketExcep& e)
		{
			failTest(e.what());
		}
	}

	virtual void run()
	{
		try
		{
			socket->readUInt32(); // This should block, as server thread doesn't send data yet.
			failTest("Shouldn't get here."); // Shouln't get here as there should be an exception thrown from the readUInt32 call above. 
		}
		catch(MySocketExcep& e)
		{
			conPrint("TestReadClientThread MySocketExcep: " + e.what());
		}
	}

	std::string hostname;
	int port;
	MySocketRef socket;
};


class TestBlockingConnectThread : public MyThread
{
public:
	TestBlockingConnectThread(const std::string& hostname_, int port_) : hostname(hostname_), port(port_)
	{
		try
		{
			socket = new MySocket();
		}
		catch(MySocketExcep& e)
		{
			conPrint(e.what());
		}
	}

	virtual void run()
	{
		try
		{
			conPrint("TestBlockingConnectThread Connecting...");
			socket->connect(hostname, port);
			conPrint("TestBlockingConnectThread connected.");

			socket->readUInt32(); // This should block, as server thread doesn't send data yet.
		}
		catch(MySocketExcep& e)
		{
			conPrint("TestReadClientThread MySocketExcep: " + e.what());
		}
	}

	std::string hostname;
	int port;
	MySocketRef socket;
};


class TestWriteClientThread : public MyThread
{
public:
	TestWriteClientThread(int port)
	{
		try
		{
			socket = new MySocket("localhost", port);
		}
		catch(MySocketExcep& e)
		{
			failTest(e.what());
		}
	}

	virtual void run()
	{
		try
		{
			for(int i=0; i<1000000; ++i)
			{
				//conPrint("client writing uint " + toString(i) + "...");
				char buf[1024];
				std::memset(buf, 0, sizeof(buf));
				socket->writeData(buf, sizeof(buf)); // This should block eventually, when the send buffer is full.
				//socket->writeUInt32(1); // This should block eventually, when the send buffer is full.
			}
			failTest("Shouldn't get here."); // Shouln't get here as there should be an exception thrown from one of the 1000000 calls above. 
		}
		catch(MySocketExcep& e)
		{
			conPrint("TestWriteClientThread MySocketExcep: " + e.what());
		}
	}

	MySocketRef socket;
};


// Lanuches TestServerSocketReadWriteThread to handle client connections.
class TestListenerThread2 : public MyThread
{
public:
	TestListenerThread2(int port_) : port(port_)
	{
		listener_sock = new MySocket();
	}

	virtual void run()
	{
		conPrint("TestListenerThread2::run() (port: " + toString(port) + ")");
		try
		{
			listener_sock->bindAndListen(port);
			MySocketRef server_socket = listener_sock->acceptConnection();
			Reference<TestServerSocketReadWriteThread> server_thread = new TestServerSocketReadWriteThread(server_socket);
			server_thread->launch();
			server_thread->join(); // Wait for server thread
		}
		catch(MySocketExcep& e)
		{
			conPrint("TestListenerThread2 MySocketExcep: " + e.what());
		}
	}
	MySocketRef listener_sock;
	int port;
};


//==============================================================================================================


void SocketTests::test()
{
	conPrint("SocketTests::test()");

	testAssert(Networking::isNonNull());

	const int port = 5000;



	//==================== Test interruption of a blocking read call. ========================
	{
		Reference<TestListenerThread2> listener_thread = new TestListenerThread2(port);
		listener_thread->launch();
    
		// Wait for a while until the listener thread has called bindAndListen()
		PlatformUtils::Sleep(500);

		// Launch client thread, should block on socket read call
		Reference<TestReadClientThread> client_thread = new TestReadClientThread("localhost", port);
		client_thread->launch();

		// We need to wait a little while, until the read call in the TestReadClientThread has been started.
		PlatformUtils::Sleep(500);

		// Shutdown the client socket, which should unblock it from the read call.
		client_thread->socket->ungracefulShutdown();

		listener_thread->join();
		client_thread->join();
	}

	//==================== Test interruption of a blocking write call. ========================
	{
		Reference<TestListenerThread2> listener_thread = new TestListenerThread2(port);
		listener_thread->launch();
    
		// Wait for a while until the listener thread has called bindAndListen()
		PlatformUtils::Sleep(500);

		// Launch client thread, should block on socket write call
		Reference<TestWriteClientThread> client_thread = new TestWriteClientThread(port);
		client_thread->launch();

		// We need to wait a little while, until the write call in the TestReadClientThread has been started.
		PlatformUtils::Sleep(500);

		// Shutdown the client socket, which should unblock it from the write call.
		client_thread->socket->ungracefulShutdown();

		listener_thread->join();
		client_thread->join();
	}


	//===================== Test interruption of a blocking acceptConnection() call. ==========================
	{
		Reference<TestListenerThread2> listener_thread = new TestListenerThread2(port);
		listener_thread->launch();

		// We need to wait a little while, until the accept call has been started.
		PlatformUtils::Sleep(500);

		listener_thread->listener_sock->ungracefulShutdown();

		listener_thread->join();
	}

	//===================== Test interruption of a blocking connect() call. ==========================
	{
		// Launch client thread, should block on socket read call
		Reference<TestBlockingConnectThread> client_thread = new TestBlockingConnectThread("192.168.1.50", 80);
		client_thread->launch();

		// We need to wait a little while, until the read call in the TestReadClientThread has been started.
		PlatformUtils::Sleep(500);

		// Shutdown the client socket, which should unblock it from the read call.
		client_thread->socket->ungracefulShutdown();

		//listener_thread->join();
		client_thread->join();
	}

	
	//===================== Do read/write tests ==========================
	
	doTestWithHostname("127.0.0.1", port);

	// Test with IPv6 address
	doTestWithHostname("::1", port);

	// Test with IPv6 address
	doTestWithHostname("localhost", port);

	
	//===================== Test handling of bindAndListen() failure - bind to an invalid port ==========================
	// NOTE: how to test this? binding to ports like -1 and 100000 seems to work fine on Windows.  WTF?
	/*try
	{
		MySocketRef socket = new MySocket();
		socket->bindAndListen(100000);

		failTest("Shouldn't get here.");
	}
	catch(MySocketExcep&)
	{}*/
	
	
	conPrint("SocketTests::test(): done.");
}


#endif
