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

			// Read strings
			testAssert(socket->readStringLengthFirst() == "hello");
			testAssert(socket->readStringLengthFirst() == "world");

			// Read strings
			testAssert(socket->readString(1000, NULL) == "hello1");
			testAssert(socket->readString(1000, NULL) == "world1");

			// Read a buffer of 3 int32s
			std::vector<int32> buf(3);
			socket->readData(&buf[0], sizeof(int32) * buf.size());
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
	TestClientThread(int port_) : port(port_) {}
	~TestClientThread() {}

	virtual void run()
	{
		// conPrint("TestClientThread::run()");
		try
		{
			MySocket socket("localhost", port, NULL);

			// Write Uint32
			socket.writeUInt32(1);
			socket.writeUInt32(2);
			socket.writeUInt32(3);

			// Write some strings with writeStringLengthFirst()
			socket.writeStringLengthFirst("hello");
			socket.writeStringLengthFirst("world");

			// Write some strings with null termination
			socket.writeString("hello1", NULL);
			socket.writeString("world1", NULL);

			// Write a buffer of 3 int32s
			const std::vector<int32> buf = makeBuffer();
			socket.writeData(&buf[0], sizeof(int32) * buf.size());
		}
		catch(MySocketExcep& e)
		{
			failTest(e.what());
		}
	}

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


void SocketTests::test()
{
	conPrint("SocketTests::test()");

	testAssert(Networking::isNonNull());

	const int port = 5000;

	// for(int i=0; i<1; ++i)
	{
		Reference<TestListenerThread> listener_thread = new TestListenerThread(port);
		listener_thread->launch();

		Reference<TestClientThread> client_thread = new TestClientThread(port);
		client_thread->launch();


		listener_thread->join();
		client_thread->join();
	}

	//delete listener_thread;
	//delete client_thread;
}


#endif
