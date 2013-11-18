/*=====================================================================
SocketTests.cpp
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-01-30 13:47:58 +0000
=====================================================================*/
#include "SocketTests.h"


#include "mysocket.h"
#include "mythread.h"
#include "networking.h"
#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"
#include "../utils/stringutils.h"
#include "../utils/platformutils.h"
#include "../utils/SocketBufferOutStream.h"


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

			// Read Uint64
			testAssert(socket->readUInt64(NULL) == 1);
			testAssert(socket->readUInt64(NULL) == 2);
			testAssert(socket->readUInt64(NULL) == 3);
			testAssert(socket->readUInt64(NULL) == 0x1234567800112233ULL);

			// Read strings
			testAssert(socket->readStringLengthFirst() == "hello");
			testAssert(socket->readStringLengthFirst() == "world");

			// Read double
			testAssert(socket->readDouble() == 0.0);
			testAssert(socket->readDouble() == 1.23456789112233445566);

			//=========== Test data from SocketBufferOutStream ===========

			// Read Uint32
			testAssert(socket->readUInt32() == 1);
			testAssert(socket->readUInt32() == 2);
			testAssert(socket->readUInt32() == 3);

			// Read Uint64
			testAssert(socket->readUInt64(NULL) == 1);
			testAssert(socket->readUInt64(NULL) == 2);
			testAssert(socket->readUInt64(NULL) == 3);
			testAssert(socket->readUInt64(NULL) == 0x1234567800112233ULL);

			// Read strings
			testAssert(socket->readStringLengthFirst() == "hello");
			testAssert(socket->readStringLengthFirst() == "world");

			// Read double
			testAssert(socket->readDouble() == 0.0);
			testAssert(socket->readDouble() == 1.23456789112233445566);


			//=========== End Test data from SocketBufferOutStream ===========

			// Read strings
			testAssert(socket->readString(1000, NULL) == "hello1");
			testAssert(socket->readString(1000, NULL) == "world1");

			// Read a buffer of 3 int32s
			std::vector<int32> buf(3);
			socket->readData(&buf[0], sizeof(int32) * buf.size(), NULL);
			testAssert(buf == makeBuffer());

			// Keep reading until we fail to read any more
			socket->waitForGracefulDisconnect();
		}
		catch(MySocketExcep& e)
		{
			failTest(e.what());
		}
	}

	MySocketRef socket;
};


class TestClientThread : public MyThread
{
public:
	TestClientThread(const std::string& server_hostname_, int port_) : server_hostname(server_hostname_), port(port_) {}
	~TestClientThread() {}

	virtual void run()
	{
		// conPrint("TestClientThread::run()");
		try
		{
			conPrint("TestClientThread: Connecting to " + server_hostname + ":" + toString(port));

			MySocket socket(server_hostname, port, NULL);

			conPrint("TestClientThread: mysocket connected!");
			conPrint("TestClientThread: other end IP: " + socket.getOtherEndIPAddress().toString());

			// Write Uint32
			socket.writeUInt32(1);
			socket.writeUInt32(2);
			socket.writeUInt32(3);

			// Write Uint64
			socket.writeUInt64(1, NULL);
			socket.writeUInt64(2, NULL);
			socket.writeUInt64(3, NULL);
			socket.writeUInt64(0x1234567800112233ULL, NULL);

			// Write some strings with writeStringLengthFirst()
			socket.writeStringLengthFirst("hello");
			socket.writeStringLengthFirst("world");

			socket.writeDouble(0.0);
			socket.writeDouble(1.23456789112233445566);

			//=========== Test SocketBufferOutStream ===========
			SocketBufferOutStream buffer;
			buffer.writeUInt32(1);
			buffer.writeUInt32(2);
			buffer.writeUInt32(3);

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
			socket.writeData(&buffer.buf[0], buffer.buf.size(), NULL);
			//=========== End Test SocketBufferOutStream ===========

			// Write some strings with null termination
			socket.writeString("hello1", NULL);
			socket.writeString("world1", NULL);

			// Write a buffer of 3 int32s
			const std::vector<int32> buf = makeBuffer();
			socket.writeData(&buf[0], sizeof(int32) * buf.size(), NULL);
		}
		catch(MySocketExcep& e)
		{
			failTest(e.what());
		}
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
			MySocketRef server_socket = listener.acceptConnection(NULL);

			Reference<TestServerSocketThread> server_thread = new TestServerSocketThread(server_socket);
			server_thread->launch();

			// Wait for server thread
			server_thread->join();

			// Terminate thread.
		}
		catch(MySocketExcep& e)
		{
			failTest(e.what());
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


void SocketTests::test()
{
	conPrint("SocketTests::test()");

	testAssert(Networking::isNonNull());

	const int port = 5000;

	doTestWithHostname("127.0.0.1", port);

	// Test with IPv6 address
	// NOTE: This test is disabled because it fails under Valgrind (and *only* under Valgrind!)
	//doTestWithHostname("::1", port);

	// Test with IPv6 address
	doTestWithHostname("localhost", port);


	conPrint("SocketTests::test(): done.");
}


#endif
