/*=====================================================================
mysocket.cpp
------------
Copyright Glare Technologies Limited 2013 -
File created by ClassTemplate on Wed Apr 17 14:43:14 2002
=====================================================================*/
#include "mysocket.h"


#include "networking.h"
#include "fractionlistener.h"
#include "../maths/mathstypes.h"
#include "../utils/stringutils.h"
#include "../utils/mythread.h"
#include "../utils/platformutils.h"
#include "../utils/timer.h"
#include <vector>
#include <string.h>
#include <algorithm>
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <unistd.h> // for close()
#include <sys/time.h> // fdset
#include <sys/types.h> // fdset
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#endif


#if defined(_WIN32)
typedef int SOCKLEN_TYPE;
#else
typedef socklen_t SOCKLEN_TYPE;

const int SOCKET_ERROR = -1;
#endif


// Block period.  This is how long any socket operations will block for, before calling the 'Should Abort' call-back.
// Must be < 1.
const double BLOCK_DURATION = 0.5; // in seconds.


MySocket::MySocket(const std::string& hostname, int port, StreamShouldAbortCallback* should_abort_callback)
{
	init();

	assert(Networking::isInited());
	if(!Networking::isInited())
		throw MySocketExcep("Networking not inited or destroyed.");

	//-----------------------------------------------------------------
	//Do DNS lookup to get server host IP
	//-----------------------------------------------------------------
	try
	{
		const std::vector<IPAddress> serverips = Networking::getInstance().doDNSLookup(hostname);

		assert(!serverips.empty());
		if(serverips.empty())
			throw MySocketExcep("could not lookup host IP with DNS");

		//-----------------------------------------------------------------
		//Do the connect using the first looked-up IP address
		//-----------------------------------------------------------------
		doConnect(serverips[0], hostname, port, should_abort_callback);
	}
	catch(NetworkingExcep& e)
	{
		throw MySocketExcep("DNS Lookup failed: " + std::string(e.what()));
	}
}


MySocket::MySocket(const IPAddress& ipaddress, int port, StreamShouldAbortCallback* should_abort_callback)
{
	init();

	assert(Networking::isInited());

	doConnect(
		ipaddress, 
		"", // hostname
		port, 
		should_abort_callback
	);
}


MySocket::MySocket()
{
	init();
}


void MySocket::init()
{
	thisend_port = -1;
	otherend_port = -1;
	sockethandle = nullSocketHandle();
	connected = false;
	do_graceful_disconnect = true;


	// Due to a bug with Windows XP, we can't use a large buffer size for reading to and writing from the socket.
	// See http://support.microsoft.com/kb/201213 for more details on the bug.
	this->max_buffersize = PlatformUtils::isWindowsXPOrEarlier() ? 1024 : (1024 * 1024 * 8);
}


static int closeSocket(MySocket::SOCKETHANDLE_TYPE sockethandle)
{
#if defined(_WIN32)
	return closesocket(sockethandle);
#else
	return ::close(sockethandle);
#endif
}


static void setBlocking(MySocket::SOCKETHANDLE_TYPE sockethandle, bool blocking)
{
#if defined(_WIN32)
	u_long nonblocking = blocking ? 0 : 1;
	const int result = ioctlsocket(sockethandle, FIONBIO, &nonblocking);
#else
	u_long nonblocking = blocking ? 0 : 1;
	const int result = ioctl(sockethandle, FIONBIO, &nonblocking);
#endif
	assert(result == 0);
	if(result != 0)
	{
		// std::cout << "ERROR: setBlocking failed." << std::endl;
	}
}


static void setLinger(MySocket::SOCKETHANDLE_TYPE sockethandle, bool linger)
{
#if defined(_WIN32)	
	const BOOL dont_linger = linger ? 0 : 1;
	const int result = setsockopt(sockethandle, SOL_SOCKET, SO_DONTLINGER, (const char*)&dont_linger, sizeof(dont_linger));
#else
	// TODO: test this
//TEMP HACK	linger lin;
//	lin.l_onoff = linger ? 1 : 0;
//	lin.l_linger = 1; // Linger time

//	const int result = setsockopt(sockethandle, SOL_SOCKET, SO_LINGER, (const char*)&lin, sizeof(lin));
	const int result = 0;
#endif
	assert(result == 0);
	if(result != 0)
	{
		// std::cout << "ERROR: setLinger failed." << std::endl;
	}
}


/*static void setDebug(MySocket::SOCKETHANDLE_TYPE sockethandle, bool enable_debug)
{
#if defined(_WIN32)	
	const BOOL debug = enable_debug ? 1 : 0;
	const int result = setsockopt(sockethandle, SOL_SOCKET, SO_DEBUG, (const char*)&debug, sizeof(debug));
	assert(result == 0);
#endif
}*/


void MySocket::doConnect(const IPAddress& ipaddress, 
						 const std::string& hostname, // Just for printing out in exceptions.  Can be empty string.
						 int port, 
						 StreamShouldAbortCallback* should_abort_callback)
{
	otherend_ipaddr = ipaddress; // Remember ip of other end

	assert(port >= 0 && port <= 65536);

	//-----------------------------------------------------------------
	//Create the socket
	//-----------------------------------------------------------------
	const int DEFAULT_PROTOCOL = 0; // Use default protocol
	const int address_family = ipaddress.getVersion() == IPAddress::Version_4 ? PF_INET : PF_INET6;
	sockethandle = socket(address_family, SOCK_STREAM, DEFAULT_PROTOCOL);

	if(!isSockHandleValid(sockethandle))
		throw MySocketExcep("Could not create a socket.  Error code == " + Networking::getError());

	//-----------------------------------------------------------------
	//Fill out server address structure
	//-----------------------------------------------------------------
	sockaddr_storage server_address;
	memset(&server_address, 0, sizeof(server_address)); // Clear struct.
	ipaddress.fillOutSockAddr((sockaddr&)server_address, port);

	//-----------------------------------------------------------------
	//Connect to server
	//-----------------------------------------------------------------

	// Turn off blocking while connecting
	setBlocking(sockethandle, false);

	if(connect(sockethandle, (sockaddr*)&server_address, ipaddress.getVersion() == IPAddress::Version_4 ? sizeof(sockaddr_in) : sizeof(sockaddr_in6)) != 0)
	{
#if defined(_WIN32)
		if(WSAGetLastError() != WSAEWOULDBLOCK)
#else
		if(errno != EINPROGRESS)
#endif
		{
			const std::string error_str = Networking::getError(); // Get error before we call closeSocket(), which may overwrite it.

			// Because we are about to throw an exception, and because doConnect() is always called only from a constructor, we are about to throw an exception from a constructor.
			// When throwing an exception from a constructor, the destructor is not called, ( http://www.parashift.com/c++-faq-lite/exceptions.html#faq-17.10 )
			// so we have to close the socket here.
			closeSocket(sockethandle);
			sockethandle = nullSocketHandle();

			if(hostname.empty())
				throw MySocketExcep("Could not make a TCP connection to server " + ipaddress.toString() + ":" + ::toString(port) + ", Error code: " + error_str);
			else
				throw MySocketExcep("Could not make a TCP connection to server '" + hostname + "' (" + ipaddress.toString() + ":" + ::toString(port) + "), Error code: " + error_str);
		}
	}

	// While we are not either connected or in an error state...
	while(1)
	{
		timeval wait_period;
		wait_period.tv_sec = 0;
		wait_period.tv_usec = (long)(BLOCK_DURATION * 1000000.0f);

		fd_set write_sockset;
		initFDSetWithSocket(write_sockset, sockethandle);
		fd_set error_sockset;
		initFDSetWithSocket(error_sockset, sockethandle);

		if(should_abort_callback && should_abort_callback->shouldAbort())
		{
			do_graceful_disconnect = false;
			setLinger(sockethandle, false);
			closeSocket(sockethandle);
			sockethandle = nullSocketHandle();
			throw AbortedMySocketExcep();
		}

		select((int)(sockethandle + SOCKETHANDLE_TYPE(1)), NULL, &write_sockset, &error_sockset, &wait_period);

		if(should_abort_callback && should_abort_callback->shouldAbort())
		{
			do_graceful_disconnect = false;
			setLinger(sockethandle, false);
			closeSocket(sockethandle);
			sockethandle = nullSocketHandle();
			throw AbortedMySocketExcep();
		}

		// If socket is writeable, then the connect has succeeded.
		if(FD_ISSET(sockethandle, &write_sockset))
			 break;

		if(FD_ISSET(sockethandle, &error_sockset))
		{
			closeSocket(sockethandle);
			sockethandle = nullSocketHandle();
			
			//throw MySocketExcep("Could not make a TCP connection to server " + ipaddress.toString() + ":" + ::toString(port));
			
			if(hostname.empty())
				throw MySocketExcep("Could not make a TCP connection to server " + ipaddress.toString() + ":" + ::toString(port));
			else
				throw MySocketExcep("Could not make a TCP connection to server '" + hostname + "' (" + ipaddress.toString() + ":" + ::toString(port) + ")");
		}
	}

	// Return to normal blocking mode.
	setBlocking(sockethandle, true);

	otherend_port = port;

	//-----------------------------------------------------------------
	//get the interface address of this host used for the connection
	//-----------------------------------------------------------------
	sockaddr_storage interface_addr;
	SOCKLEN_TYPE length = sizeof(interface_addr);
	memset(&interface_addr, 0, length);
	const int result = getsockname(sockethandle, (sockaddr*)&interface_addr, &length);

	if(result != SOCKET_ERROR)
	{
		thisend_ipaddr = IPAddress((const sockaddr&)interface_addr);
		thisend_port = Networking::getPortFromSockAddr((const sockaddr&)interface_addr);
	}

	connected = true;
}


MySocket::~MySocket()
{
	close();
}


void MySocket::bindAndListen(int port)
{
	Timer timer;

	assert(Networking::isInited());

	//-----------------------------------------------------------------
	//If socket already exists, destroy it
	//-----------------------------------------------------------------
	if(isSockHandleValid(sockethandle))
	{
		assert(0);
		throw MySocketExcep("Socket already created.");
	}

	//-----------------------------------------------------------------
	//Set up address struct for this host, the server.
	//-----------------------------------------------------------------
	sockaddr_storage server_addr;
	memset(&server_addr, 0, sizeof(server_addr));

	// Use IPv6 'any address'.  Will allow IPv4 connections as well.
	sockaddr_in6* ipv6_addr = (sockaddr_in6*)&server_addr;
	ipv6_addr->sin6_family = AF_INET6;
	ipv6_addr->sin6_addr = in6addr_any; // Accept on any interface
	ipv6_addr->sin6_port = htons((uint16_t)port);

	//-----------------------------------------------------------------
	//Create the socket
	//-----------------------------------------------------------------
	sockethandle = socket(
		PF_INET6, SOCK_STREAM,
		0 // protocol - default
	);

	if(!isSockHandleValid(sockethandle))
		throw MySocketExcep("Could not create a socket: " + Networking::getError());


	// Turn off IPV6_V6ONLY so that we can receive IPv4 connections as well.
	int no = 0;     
	if(setsockopt(sockethandle, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&no, sizeof(no)) != 0)
	{
		assert(0);
		//conPrint("Warning: setsockopt failed.");
	}

	if(::bind(sockethandle, (const sockaddr*)&server_addr, sizeof(sockaddr_in6)) == SOCKET_ERROR)
		throw MySocketExcep("Failed to bind to port " + toString(port) + ": " + Networking::getError());


	const int backlog = 10;

	if(::listen(sockethandle, backlog) == SOCKET_ERROR)
		throw MySocketExcep("listen failed: " + Networking::getError());

	thisend_port = port;
}


MySocketRef MySocket::acceptConnection(StreamShouldAbortCallback* should_abort_callback) // throw (MySocketExcep)
{
	assert(Networking::isInited());

	Timer accept_timer;

	// Wait until the accept() will succeed.
	while(1)
	{
		timeval wait_period;
		wait_period.tv_sec = 0; // seconds
		wait_period.tv_usec = (long)(BLOCK_DURATION * 1000000.0); // and microseconds

		// Create the file descriptor set
		fd_set sockset;
		initFDSetWithSocket(sockset, sockethandle);
		fd_set error_sockset;
		initFDSetWithSocket(error_sockset, sockethandle);

		if(should_abort_callback && should_abort_callback->shouldAbort())
		{
			do_graceful_disconnect = false;
			throw AbortedMySocketExcep();
		}

		const int num_ready = select(
			(int)(sockethandle + SOCKETHANDLE_TYPE(1)), // nfds: range of file descriptors to test
			&sockset, // read fds
			NULL, // write fds
			&error_sockset, // error fds
			&wait_period // timeout
			);

		if(FD_ISSET(sockethandle, &error_sockset))
		{
			// Error occured
			throw MySocketExcep("Error while accepting connection.");
		}

		if(should_abort_callback && should_abort_callback->shouldAbort())
		{
			do_graceful_disconnect = false;
			throw AbortedMySocketExcep();
		}

		if(num_ready != 0)
			break;
	}

	// Now this should accept immediately .... :)

	sockaddr_storage client_addr; // Data struct to get the client IP
	SOCKLEN_TYPE length = sizeof(client_addr);
	memset(&client_addr, 0, length);
	SOCKETHANDLE_TYPE newsockethandle = ::accept(sockethandle, (sockaddr*)&client_addr, &length);

	if(!isSockHandleValid(newsockethandle))
		throw MySocketExcep("accept failed");

	//-----------------------------------------------------------------
	//copy data over to new socket that will do actual communicating
	//-----------------------------------------------------------------
	MySocketRef new_socket(new MySocket());
	new_socket->sockethandle = newsockethandle;

	//-----------------------------------------------------------------
	// Get other end ip and port
	//-----------------------------------------------------------------
	new_socket->otherend_ipaddr = IPAddress((const sockaddr&)client_addr);
	new_socket->otherend_port = Networking::getPortFromSockAddr((const sockaddr&)client_addr);

	//-----------------------------------------------------------------
	// Get the interface address of this host used for the connection
	//-----------------------------------------------------------------
	sockaddr_storage interface_addr;
	length = sizeof(interface_addr);
	memset(&interface_addr, 0, length);
	const int result = getsockname(sockethandle, (sockaddr*)&interface_addr, &length);

	if(result != SOCKET_ERROR)
	{
		thisend_ipaddr = IPAddress((const sockaddr&)interface_addr);
		thisend_port = Networking::getPortFromSockAddr((const sockaddr&)interface_addr);
	}

	new_socket->connected = true;

	return new_socket;
}


void MySocket::close()
{
	// conPrint("---MySocket::close()---");
	if(isSockHandleValid(sockethandle))
	{
		//------------------------------------------------------------------------
		//try shutting down the socket
		//------------------------------------------------------------------------
		//int result = shutdown(sockethandle, 2); // 2 == SD_BOTH
		//conPrint("Shutdown result: " + toString(result));
		//result = closeSocket(sockethandle);
		//conPrint("closeSocket result: " + toString(result));
		// printVar(connected);

		// printVar(connected);

		if(connected)
		{
			// Initiate graceful shutdown.
			shutdown(sockethandle,  1); // 1 == SD_SEND

			// Wait for graceful shutdown
			if(do_graceful_disconnect)
			{
				while(1)
				{
					// conPrint("Waiting for graceful shutdown...");

					char buf[1024];
					const int numbytesread = ::recv(sockethandle, buf, sizeof(buf), 0);
					if(numbytesread == 0)
					{
						// Socket has been closed gracefully
						// conPrint("numbytesread == 0, socket closed gracefully.");
						break;
					}
					else if(numbytesread == SOCKET_ERROR)
					{
						// conPrint("numbytesread == SOCKET_ERROR, error: " + Networking::getError());//TEMP
						break;
					}
				}
			}
		}

		closeSocket(sockethandle);
	}

	sockethandle = nullSocketHandle();
}


void MySocket::write(const void* data, size_t datalen, StreamShouldAbortCallback* should_abort_callback)
{
	write(data, datalen, NULL, should_abort_callback);
}


void MySocket::write(const void* data, size_t datalen, FractionListener* frac, StreamShouldAbortCallback* should_abort_callback)
{
	const size_t totalnumbytestowrite = datalen;

	while(datalen > 0)//while still bytes to write
	{
		const int numbytestowrite = (int)std::min(this->max_buffersize, datalen);
		assert(numbytestowrite > 0);

		//------------------------------------------------------------------------
		//NEWCODE: loop until either the prog is exiting or can write to socket
		//------------------------------------------------------------------------
		/*while(1)
		{
			timeval wait_period;
			wait_period.tv_sec = 0;
			wait_period.tv_usec = (long)(BLOCK_DURATION * 1000000.0f);

			fd_set sockset;
			initFDSetWithSocket(sockset, sockethandle);

			if(should_abort_callback && should_abort_callback->shouldAbort())
				throw AbortedMySocketExcep();

			// Get number of handles that are ready to write to
			const int num_ready = select(sockethandle + SOCKETHANDLE_TYPE(1), NULL, &sockset, NULL, &wait_period);

			if(should_abort_callback && should_abort_callback->shouldAbort())
				throw AbortedMySocketExcep();

			if(num_ready != 0)
				break;
		}*/

		const int numbyteswritten = send(sockethandle, (const char*)data, numbytestowrite, 0);

		if(numbyteswritten == SOCKET_ERROR)
			throw MySocketExcep("write failed.  Error code == " + Networking::getError());

		datalen -= numbyteswritten;
		data = (void*)((char*)data + numbyteswritten); // Advance data pointer

		if(frac)
			frac->setFraction((float)(totalnumbytestowrite - datalen) / (float)totalnumbytestowrite);

		if(should_abort_callback && should_abort_callback->shouldAbort())
		{
			do_graceful_disconnect = false;
			throw AbortedMySocketExcep();
		}
	}
}


size_t MySocket::readSomeBytes(void* buffer, size_t max_num_bytes)
{
	const int numbytesread = recv(sockethandle, (char*)buffer, (int)max_num_bytes, 0);

	if(numbytesread == SOCKET_ERROR) // Connection was reset/broken
		throw MySocketExcep("Read failed, error: " + Networking::getError());
	else if(numbytesread == 0) // Connection was closed gracefully
		throw MySocketExcep("Connection Closed.");

	return (size_t)numbytesread;
}


void MySocket::readTo(void* buffer, size_t readlen, StreamShouldAbortCallback* should_abort_callback)
{
	readTo(buffer, readlen, NULL, should_abort_callback);
}


/*class ReaderThread : public MyThread
{
public:
	ReaderThread(MySocket::SOCKETHANDLE_TYPE socket_handle_, char* buffer_, size_t num_bytes_, int* num_bytes_read_) : socket_handle(socket_handle_), buffer(buffer_), num_bytes(num_bytes_), num_bytes_read(num_bytes_read_) {}

	virtual void run()
	{
		*num_bytes_read = recv(socket_handle, buffer, num_bytes, 0);
	}

private:
	MySocket::SOCKETHANDLE_TYPE socket_handle;
	char* buffer;
	size_t num_bytes;
	int* num_bytes_read;
};*/


void MySocket::readTo(void* buffer, size_t readlen, FractionListener* frac, StreamShouldAbortCallback* should_abort_callback)
{
	const size_t totalnumbytestoread = readlen;

	while(readlen > 0) // While still bytes to read
	{
		const int numbytestoread = (int)std::min(this->max_buffersize, readlen);
		assert(numbytestoread > 0);

		//------------------------------------------------------------------------
		//Loop until either the prog is exiting or have incoming data
		//------------------------------------------------------------------------
		/*while(1)
		{
			timeval wait_period;
			wait_period.tv_sec = 0;
			wait_period.tv_usec = (long)(BLOCK_DURATION * 1000000.0f);

			fd_set sockset;
			initFDSetWithSocket(sockset, sockethandle);

			if(should_abort_callback && should_abort_callback->shouldAbort())
				throw AbortedMySocketExcep();

			// Get number of handles that are ready to read from
			const int num_ready = select(sockethandle + SOCKETHANDLE_TYPE(1), &sockset, NULL, NULL, &wait_period);

			if(should_abort_callback && should_abort_callback->shouldAbort())
				throw AbortedMySocketExcep();

			if(num_ready != 0)
				break;
		}*/

		//------------------------------------------------------------------------
		//do the read
		//------------------------------------------------------------------------
		const int numbytesread = recv(sockethandle, (char*)buffer, numbytestoread, 0);

		/*int numbytesread = 0;
		ReaderThread* reader_thread = new ReaderThread(sockethandle, (char*)buffer, numbytestoread, &numbytesread);
		reader_thread->launch(
			false // auto delete
		);
		reader_thread->join();
		delete reader_thread;*/

		if(numbytesread == SOCKET_ERROR) // Connection was reset/broken
			throw MySocketExcep("Read failed, error: " + Networking::getError());
		else if(numbytesread == 0) // Connection was closed gracefully
			throw MySocketExcep("Connection Closed.");

		readlen -= numbytesread;
		buffer = (void*)((char*)buffer + numbytesread);

		if(frac)
			frac->setFraction((float)(totalnumbytestoread - readlen) / (float)totalnumbytestoread);

		if(should_abort_callback && should_abort_callback->shouldAbort())
		{
			do_graceful_disconnect = false;
			throw AbortedMySocketExcep();
		}
	}
}


void MySocket::waitForGracefulDisconnect()
{
	// conPrint("---------waitForGracefulDisconnect-------");
	while(1)
	{
		char buf[1024];
		const int numbytesread = recv(sockethandle, buf, sizeof(buf), 0);

		if(numbytesread == SOCKET_ERROR) // Connection was reset/broken
			throw MySocketExcep("Read failed, error: " + Networking::getError());
		else if(numbytesread == 0) // Connection was closed gracefully
		{
			// conPrint("\tConnection was closed gracefully.");
			return;
		}
	}
}


const std::string MySocket::readString(size_t max_string_length, StreamShouldAbortCallback* should_abort_callback) // Read null-terminated string.
{
	std::string s;
	while(1)
	{
		char c;
		this->readTo(&c, sizeof(c), should_abort_callback);
		if(c == 0) // If we just read the null terminator.
			return s;
		else
			s += std::string(1, c); // Append the character to the string. NOTE: maybe faster way to do this?

		if(s.size() > max_string_length)
			throw MySocketExcep("String too long");
	}
	return s;
}


void MySocket::writeInt32(int32 x, StreamShouldAbortCallback* should_abort_callback)
{
	union data
	{
		int signed_i;
		unsigned int unsigned_i;
	};
	data d;
	d.signed_i = x;
	const unsigned int i = htonl(d.unsigned_i);
	write(&i, sizeof(int), should_abort_callback);
}


void MySocket::writeUInt32(uint32 x, StreamShouldAbortCallback* should_abort_callback)
{
	const uint32 i = htonl(x); // Convert to network byte ordering.
	write(&i, sizeof(uint32), should_abort_callback);
}


void MySocket::writeUInt64(uint64 x, StreamShouldAbortCallback* should_abort_callback)
{
	//NOTE: not sure if this byte ordering is correct.
	union data
	{
		uint32 i32[2];
		uint64 i64;
	};

	data d;
	d.i64 = x;
	writeUInt32(d.i32[0], should_abort_callback);
	writeUInt32(d.i32[1], should_abort_callback);
}


void MySocket::writeString(const std::string& s, StreamShouldAbortCallback* should_abort_callback) // Write null-terminated string.
{
	this->write(
		s.c_str(), 
		s.size() + 1, // num bytes: + 1 for null terminator.
		should_abort_callback
	);
}


int MySocket::readInt32(StreamShouldAbortCallback* should_abort_callback)
{
	union data
	{
		int si;
		uint32 i;
	};
	data d;
	readTo(&d.i, sizeof(uint32), should_abort_callback);
	d.i = ntohl(d.i);
	return d.si;
}


uint32 MySocket::readUInt32(StreamShouldAbortCallback* should_abort_callback)
{
	uint32 x;
	readTo(&x, sizeof(uint32), should_abort_callback);
	return ntohl(x);
}


uint64 MySocket::readUInt64(StreamShouldAbortCallback* should_abort_callback)
{
	//NOTE: not sure if this byte ordering is correct.
	union data
	{
		uint32 i32[2];
		uint64 i64;
	};

	data d;
	d.i32[0] = readUInt32(should_abort_callback);
	d.i32[1] = readUInt32(should_abort_callback);
	return d.i64;
}


bool MySocket::readable(double timeout_s)
{
	assert(timeout_s < 1.0);

	timeval wait_period;
	wait_period.tv_sec = 0;
	wait_period.tv_usec = (long)(timeout_s * 1000000.0);

	fd_set sockset;
	initFDSetWithSocket(sockset, sockethandle);

	// Get number of handles that are ready to read from
	const int num = select(
		(int)(sockethandle + 1), 
		&sockset, // Read fds
		NULL, // Read fds
		NULL, // Read fds
		&wait_period
		);

	return num > 0;
}


uint32 MySocket::readUInt32()
{
	return readUInt32(NULL);
}


void MySocket::readData(void* buf, size_t num_bytes, StreamShouldAbortCallback* should_abort_callback)
{
	readTo(buf, num_bytes, 
		NULL, // fraction listener
		should_abort_callback
	);
}


bool MySocket::endOfStream()
{
	return false;
}


void MySocket::writeUInt32(uint32 x)
{
	writeUInt32(x, NULL);
}


void MySocket::writeData(const void* data, size_t num_bytes, StreamShouldAbortCallback* should_abort_callback)
{
	write(data, num_bytes, 
		NULL, // fraction listener
		should_abort_callback
	);
}


MySocket::SOCKETHANDLE_TYPE MySocket::nullSocketHandle() const
{
#if defined(_WIN32)
	return INVALID_SOCKET;
#else
	return -1;
#endif
}


bool MySocket::isSockHandleValid(SOCKETHANDLE_TYPE handle)
{
#if defined(_WIN32)
	return handle != INVALID_SOCKET;
#else
	return handle >= 0;
#endif
}


void MySocket::setNagleAlgEnabled(bool enabled_)//on by default.
{
#if defined(_WIN32)
	BOOL enabled = enabled_;

	::setsockopt(sockethandle, //socket handle
		IPPROTO_TCP, //level
		TCP_NODELAY, //option name
		(const char*)&enabled,//value
		sizeof(BOOL));//size of value buffer
#else
	//int enabled = enabled_;
	//TODO
	assert(0);
#endif
}


void MySocket::initFDSetWithSocket(fd_set& sockset, SOCKETHANDLE_TYPE& sockhandle)
{
	FD_ZERO(&sockset);

	//FD_SET doesn't seem to work when targeting x64 in gcc.
#ifdef COMPILER_GCC
	//sockset.fds_bits[0] = sockhandle;
	FD_SET(sockhandle, &sockset);
#else
	FD_SET(sockhandle, &sockset);
#endif
}
