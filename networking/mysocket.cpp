/*=====================================================================
mysocket.cpp
------------
Copyright Glare Technologies Limited 2015 -
File created by ClassTemplate on Wed Apr 17 14:43:14 2002
=====================================================================*/
#include "mysocket.h"


#include "networking.h"
#include "fractionlistener.h"
#include "../maths/mathstypes.h"
#include "../utils/StringUtils.h"
#include "../utils/MyThread.h"
#include "../utils/PlatformUtils.h"
#include "../utils/Timer.h"
#include "../utils/EventFD.h"
#include "../utils/ConPrint.h"
#include "../utils/BitUtils.h"
#include <vector>
#include <string.h>
#include <algorithm>
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Mstcpip.h>
#else
#include <netinet/in.h>
#include <netinet/tcp.h> // For TCP_NODELAY
#include <unistd.h> // for close()
#include <sys/time.h> // fdset
#include <sys/types.h> // fdset
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <poll.h>
#endif


#if defined(_WIN32)
typedef int SOCKLEN_TYPE;
#else
typedef socklen_t SOCKLEN_TYPE;

static const int SOCKET_ERROR = -1;
#endif


static int closeSocket(MySocket::SOCKETHANDLE_TYPE sockethandle)
{
#if defined(_WIN32)
	return closesocket(sockethandle);
#else
	return ::close(sockethandle);
#endif
}


MySocket::MySocket(const std::string& hostname, int port)
{
	assert(Networking::isInited());
	if(!Networking::isInited())
		throw MySocketExcep("Networking not inited or destroyed.");

	init();
	
	try
	{
		connect(hostname, port);
	}
	catch(NetworkingExcep& e)
	{
		throw MySocketExcep("DNS Lookup failed: " + std::string(e.what()));
	}
	catch(MySocketExcep& e)
	{
		// Connect failed.
		// When throwing an exception from a constructor, the destructor is not called, ( http://www.parashift.com/c++-faq-lite/exceptions.html#faq-17.10 )
		// so we have to close the socket here.
		closeSocket(sockethandle);
		throw e;
	}
}


MySocket::MySocket(const IPAddress& ipaddress, int port)
{
	assert(Networking::isInited());

	init();

	try
	{
		connect(
			ipaddress, 
			"", // hostname
			port
		);
	}
	catch(MySocketExcep& e)
	{
		// Connect failed.
		// When throwing an exception from a constructor, the destructor is not called, ( http://www.parashift.com/c++-faq-lite/exceptions.html#faq-17.10 )
		// so we have to close the socket here.
		closeSocket(sockethandle);
		throw e;
	}
}


MySocket::MySocket(SOCKETHANDLE_TYPE sockethandle_)
{
	assert(Networking::isInited());

	sockethandle = sockethandle_;

	otherend_port = -1;
	connected = false;
	do_graceful_disconnect = true;

	// Due to a bug with Windows XP, we can't use a large buffer size for reading to and writing from the socket.
	// See http://support.microsoft.com/kb/201213 for more details on the bug.
	this->max_buffersize = PlatformUtils::isWindowsXPOrEarlier() ? 1024 : (1024 * 1024 * 8);
}


MySocket::MySocket()
{
	init();
}


void MySocket::init()
{
	otherend_port = -1;
	sockethandle = nullSocketHandle();
	connected = false;
	do_graceful_disconnect = true;

	// Due to a bug with Windows XP, we can't use a large buffer size for reading to and writing from the socket.
	// See http://support.microsoft.com/kb/201213 for more details on the bug.
	this->max_buffersize = PlatformUtils::isWindowsXPOrEarlier() ? 1024 : (1024 * 1024 * 8);

	// Create socket
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
}


/*static void setBlocking(MySocket::SOCKETHANDLE_TYPE sockethandle, bool blocking)
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
}*/


/*static void setDebug(MySocket::SOCKETHANDLE_TYPE sockethandle, bool enable_debug)
{
#if defined(_WIN32)	
	const BOOL debug = enable_debug ? 1 : 0;
	const int result = setsockopt(sockethandle, SOL_SOCKET, SO_DEBUG, (const char*)&debug, sizeof(debug));
	assert(result == 0);
#endif
}*/


void MySocket::connect(const std::string& hostname,
						 int port)
{
	try
	{
		//-----------------------------------------------------------------
		//Do DNS lookup to get server host IP
		//-----------------------------------------------------------------
		const std::vector<IPAddress> serverips = Networking::getInstance().doDNSLookup(hostname);
		assert(!serverips.empty());
		if(serverips.empty())
			throw MySocketExcep("could not lookup host IP with DNS");

		//-----------------------------------------------------------------
		//Do the connect using the first looked-up IP address
		//-----------------------------------------------------------------
		connect(serverips[0], hostname, port);
	}
	catch(NetworkingExcep& e)
	{
		throw MySocketExcep("DNS Lookup failed: " + std::string(e.what()));
	}
}


void MySocket::connect(const IPAddress& ipaddress, 
						 const std::string& hostname, // Just for printing out in exceptions.  Can be empty string.
						 int port)
{
	otherend_ipaddr = ipaddress; // Remember ip of other end

	assert(port >= 0 && port <= 65536);

	//-----------------------------------------------------------------
	//Fill out server address structure
	//-----------------------------------------------------------------
	sockaddr_storage server_address;
	ipaddress.fillOutIPV6SockAddr(server_address, port);

	//-----------------------------------------------------------------
	//Connect to server
	//-----------------------------------------------------------------
	if(::connect(sockethandle, (sockaddr*)&server_address, sizeof(sockaddr_in6)) != 0)
	{
#if defined(_WIN32)
		if(WSAGetLastError() != WSAEWOULDBLOCK)
#else
		if(errno != EINPROGRESS)
#endif
		{
			const std::string error_str = Networking::getError(); // Get error before we call anything else, which may overwrite it.
			if(hostname.empty())
				throw MySocketExcep("Could not make a TCP connection to server " + IPAddress::formatIPAddressAndPort(ipaddress, port) + ", Error code: " + error_str);
			else
				throw MySocketExcep("Could not make a TCP connection to server '" + hostname + "' (" + IPAddress::formatIPAddressAndPort(ipaddress, port) + "), Error code: " + error_str);
		}
	}

	otherend_port = port;

	connected = true;
}


MySocket::~MySocket()
{
	shutdown();

	closeSocket(sockethandle);
}


void MySocket::bindAndListen(int port)
{
	Timer timer;

	assert(Networking::isInited());

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


	if(::bind(sockethandle, (const sockaddr*)&server_addr, sizeof(sockaddr_in6)) == SOCKET_ERROR)
		throw MySocketExcep("Failed to bind to port " + toString(port) + ": " + Networking::getError());


	const int backlog = 10;

	if(::listen(sockethandle, backlog) == SOCKET_ERROR)
		throw MySocketExcep("listen failed: " + Networking::getError());
}


MySocketRef MySocket::acceptConnection() // throw (MySocketExcep)
{
	assert(Networking::isInited());

	sockaddr_storage client_addr; // Data struct to get the client IP
	SOCKLEN_TYPE length = sizeof(client_addr);
	memset(&client_addr, 0, length);
	SOCKETHANDLE_TYPE newsockethandle = ::accept(sockethandle, (sockaddr*)&client_addr, &length);

	if(!isSockHandleValid(newsockethandle))
		throw MySocketExcep("accept failed: " + PlatformUtils::getLastErrorString());

	//-----------------------------------------------------------------
	//copy data over to new socket that will do actual communicating
	//-----------------------------------------------------------------
	MySocketRef new_socket = new MySocket(newsockethandle);

	//-----------------------------------------------------------------
	// Get other end ip and port
	//-----------------------------------------------------------------
	new_socket->otherend_ipaddr = IPAddress((const sockaddr&)client_addr);
	new_socket->otherend_port = Networking::getPortFromSockAddr((const sockaddr&)client_addr);

	new_socket->connected = true;

	return new_socket;
}


void MySocket::shutdown()
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
			::shutdown(sockethandle,  1); // 1 == SD_SEND

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

		//closeSocket(sockethandle);
	}

	//sockethandle = nullSocketHandle();
}


void MySocket::ungracefulShutdown()
{
	if(isSockHandleValid(sockethandle))
	{
		::shutdown(sockethandle, 2); // 2 == SD_BOTH
		closeSocket(sockethandle); // Clost the socket.  This will cause the socket to return from any blocking calls.
		sockethandle = nullSocketHandle();
	}
}


void MySocket::write(const void* data, size_t datalen)
{
	write(data, datalen, NULL);
}


void MySocket::write(const void* data, size_t datalen, FractionListener* frac)
{
	const size_t totalnumbytestowrite = datalen;

	while(datalen > 0)//while still bytes to write
	{
		const int numbytestowrite = (int)std::min(this->max_buffersize, datalen);
		assert(numbytestowrite > 0);

		const int numbyteswritten = send(sockethandle, (const char*)data, numbytestowrite, 0);

		if(numbyteswritten == SOCKET_ERROR)
			throw MySocketExcep("write failed.  Error code == " + Networking::getError());

		datalen -= numbyteswritten;
		data = (void*)((char*)data + numbyteswritten); // Advance data pointer

		if(frac)
			frac->setFraction((float)(totalnumbytestowrite - datalen) / (float)totalnumbytestowrite);
	}
}


size_t MySocket::readSomeBytes(void* buffer, size_t max_num_bytes)
{
	const int numbytesread = recv(sockethandle, (char*)buffer, (int)max_num_bytes, 0);

	if(numbytesread == SOCKET_ERROR) // Connection was reset/broken
		throw MySocketExcep("Read failed, error: " + Networking::getError());

	return (size_t)numbytesread;
}


void MySocket::readTo(void* buffer, size_t readlen)
{
	readTo(buffer, readlen, NULL);
}


void MySocket::readTo(void* buffer, size_t readlen, FractionListener* frac)
{
	const size_t totalnumbytestoread = readlen;

	while(readlen > 0) // While still bytes to read
	{
		const int numbytestoread = (int)std::min(this->max_buffersize, readlen);
		assert(numbytestoread > 0);

		//------------------------------------------------------------------------
		//do the read
		//------------------------------------------------------------------------
		const int numbytesread = recv(sockethandle, (char*)buffer, numbytestoread, 0);

		//printVar(numbytesread);
		//printVar(readlen);

		if(numbytesread == SOCKET_ERROR) // Connection was reset/broken
			throw MySocketExcep("Read failed, error: " + Networking::getError());
		else if(numbytesread == 0) // Connection was closed gracefully
			throw MySocketExcep("Connection Closed.");

		readlen -= numbytesread;
		buffer = (void*)((char*)buffer + numbytesread);

		if(frac)
			frac->setFraction((float)(totalnumbytestoread - readlen) / (float)totalnumbytestoread);
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


const std::string MySocket::readString(size_t max_string_length) // Read null-terminated string.
{
	std::string s;
	while(1)
	{
		char c;
		this->readTo(&c, sizeof(c));
		if(c == 0) // If we just read the null terminator.
			return s;
		else
			s.push_back(c); // Append the character to the string.

		if(s.size() > max_string_length)
			throw MySocketExcep("String too long");
	}
	return s;
}


void MySocket::writeInt32(int32 x)
{
	const uint32 i = htonl(bitCast<uint32>(x));
	write(&i, sizeof(uint32));
}


void MySocket::writeUInt32(uint32 x)
{
	const uint32 i = htonl(x); // Convert to network byte ordering.
	write(&i, sizeof(uint32));
}


void MySocket::writeUInt64(uint64 x)
{
	uint32 i32[2];
	std::memcpy(i32, &x, sizeof(uint64));
	writeUInt32(i32[0]);
	writeUInt32(i32[1]);
}


void MySocket::writeString(const std::string& s) // Write null-terminated string.
{
	this->write(
		s.c_str(), 
		s.size() + 1 // num bytes: + 1 for null terminator.
	);
}


int MySocket::readInt32()
{
	uint32 i;
	readTo(&i, sizeof(uint32));
	i = ntohl(i);
	return bitCast<int32>(i);
}


uint32 MySocket::readUInt32()
{
	uint32 x;
	readTo(&x, sizeof(uint32));
	return ntohl(x);
}


uint64 MySocket::readUInt64()
{
	uint32 buf[2];
	buf[0] = readUInt32();
	buf[1] = readUInt32();
	uint64 x;
	std::memcpy(&x, buf, sizeof(uint64));
	return x;
}


bool MySocket::readable(double timeout_s)
{
	assert(timeout_s < 1.0);

	timeval wait_period;
	wait_period.tv_sec = 0;
	wait_period.tv_usec = (long)(timeout_s * 1000000.0);

	fd_set read_sockset;
	initFDSetWithSocket(read_sockset, sockethandle);

	fd_set error_sockset;
	initFDSetWithSocket(error_sockset, sockethandle);

	// Get number of handles that are ready to read from
	const int num = select(
		(int)(sockethandle + 1), 
		&read_sockset, // Read fds
		NULL, // Write fds
		&error_sockset, // Error fds
		&wait_period
	);

	if(num == SOCKET_ERROR)
		throw MySocketExcep("select failed: " + Networking::getError());

	if(FD_ISSET(sockethandle, &error_sockset))
		throw MySocketExcep(Networking::getError()); 

	return num > 0;
}


// Block until either the socket is readable or the event_fd is signalled (becomes readable). 
// Returns true if the socket was readable, false if the event_fd was signalled.
bool MySocket::readable(EventFD& event_fd)  
{	
#if defined(_WIN32) || defined(OSX)
	assert(0);
	return false;
#else
	
	// Make an array of 2 poll_fds
	struct pollfd poll_fds[2];
	poll_fds[0].fd = sockethandle;
	poll_fds[0].events = POLLIN | POLLRDHUP | POLLERR; // NOTE: Do we want POLLRDHUP here?
	poll_fds[0].revents = 0;
	
	poll_fds[1].fd = event_fd.efd;
	poll_fds[1].events = POLLIN | POLLRDHUP | POLLERR; // NOTE: Do we want POLLRDHUP here?
	poll_fds[1].revents = 0;
	
	const int num = poll(
		poll_fds,
		2, // num fds
		-1 // timeout: -1 = infinite.
	);
	
	if(num == SOCKET_ERROR)
		throw MySocketExcep("poll failed: " + Networking::getError());

	// Check for errors on either fd.
	if(poll_fds[0].revents & POLLERR)
		throw MySocketExcep(Networking::getError());
	if(poll_fds[1].revents & POLLERR)
		throw MySocketExcep(Networking::getError());
		
	//conPrint("poll_fds[0].revents & POLLIN:    " + toString((int)poll_fds[0].revents & POLLIN));
	//conPrint("poll_fds[0].revents & POLLRDHUP: " + toString((int)poll_fds[0].revents & POLLRDHUP));
	//conPrint("poll_fds[0].revents & POLLERR:   " + toString((int)poll_fds[0].revents & POLLERR));

	// Return if socket was readable.
	return (poll_fds[0].revents & POLLIN) || (poll_fds[0].revents & POLLRDHUP);
	
	// Make read socket set, add event_fd as well as the socket handle.
	/*fd_set read_sockset;
	FD_ZERO(&read_sockset);
	FD_SET(sockethandle, &read_sockset);
	FD_SET(event_fd.efd, &read_sockset);

	fd_set error_sockset;
	FD_ZERO(&error_sockset);
	FD_SET(sockethandle, &error_sockset);
	FD_SET(event_fd.efd, &error_sockset);

	// Get number of handles that are ready to read from
	const int num = select(
		myMax((int)sockethandle, (int)event_fd.efd) + 1, // 'nfds is the highest-numbered file descriptor in any of the three sets, plus 1.'
		&read_sockset, // Read fds
		NULL, // Write fds
		&error_sockset, // Error fds
		NULL // timout - use NULL to block indefinitely.
	);

	if(num == SOCKET_ERROR)
		throw MySocketExcep("select failed: " + Networking::getError());

	if(FD_ISSET(sockethandle, &error_sockset))
		throw MySocketExcep(Networking::getError()); 

	return FD_ISSET(sockethandle, &read_sockset);*/
#endif
}


void MySocket::readData(void* buf, size_t num_bytes)
{
	readTo(buf, num_bytes, 
		NULL // fraction listener
	);
}


bool MySocket::endOfStream()
{
	return false;
}


void MySocket::writeData(const void* data, size_t num_bytes)
{
	write(data, num_bytes, 
		NULL // fraction listener
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


void MySocket::setNoDelayEnabled(bool enabled_)
{
	int enabled = enabled_ ? 1 : 0;
	if(::setsockopt(sockethandle, // socket handle
		IPPROTO_TCP, // level
		TCP_NODELAY, // option name
		(const char*)&enabled, // value
		sizeof(enabled) // size of value buffer
		) != 0)
		throw MySocketExcep("setsockopt failed, error: " + Networking::getError());
}


void MySocket::enableTCPKeepAlive(float period)
{
#if defined(_WIN32)
	assert(period <= std::numeric_limits<ULONG>::max() / 1000.f); // Make sure period is representable in an ULONG.

	struct tcp_keepalive keepalive;
	keepalive.onoff = 1; // Turn on keepalive
	keepalive.keepalivetime = (ULONG)(period * 1000.0); // Convert to ms
	keepalive.keepaliveinterval = 1000; // 1s

	// See 'SIO_KEEPALIVE_VALS control code', (http://msdn.microsoft.com/en-us/library/windows/desktop/dd877220(v=vs.85).aspx)
	DWORD num_bytes_returned;
	const int result = WSAIoctl(
		sockethandle,		// descriptor identifying a socket
		SIO_KEEPALIVE_VALS,	// dwIoControlCode
		(void*)&keepalive,	// pointer to tcp_keepalive struct 
		(DWORD)sizeof(keepalive),		// length of input buffer 
		NULL,				// output buffer
		0,					// size of output buffer
		(LPDWORD)&num_bytes_returned,	// number of bytes returned
		NULL,				// OVERLAPPED structure
		NULL				// completion routine
	);
	if(result != 0)
		throw MySocketExcep("WSAIoctl failed, error: " + Networking::getError());
#else

	// Enable keepalive
	const int enabled = 1;
	if(::setsockopt(sockethandle, // socket handle
		SOL_SOCKET, // level
		SO_KEEPALIVE, // option name
		(const char*)&enabled, // value
		sizeof(enabled) // size of value buffer
		) != 0)
		throw MySocketExcep("setsockopt failed, error: " + Networking::getError());

#if defined(OSX)
	// OS X has TCP_KEEPALIVE instead of TCP_KEEPIDLE.
	// Set TCP_KEEPALIVE - the amount of time, in seconds, thatthe connection must be idle before keepalive probes (if enabled) are sent.  
	// (See https://developer.apple.com/library/mac/DOCUMENTATION/Darwin/Reference/ManPages/man4/tcp.4.html)
	const int keepalive = myMax(1, (int)period);
	if(::setsockopt(sockethandle, // socket handle
		IPPROTO_TCP, // level
		TCP_KEEPALIVE, // option name
		(const char*)&keepalive, // value
		sizeof(keepalive) // size of value buffer
		) != 0)
		throw MySocketExcep("setsockopt failed, error: " + Networking::getError());
#else
	// Else if Linux:
	// Set TCP_KEEPIDLE - the time (in seconds) the connection needs to remain idle before TCP starts sending keepalive probes.  (See http://linux.die.net/man/7/tcp)
	const int keepidle = myMax(1, (int)period);
	if(::setsockopt(sockethandle, // socket handle
		IPPROTO_TCP, // level
		TCP_KEEPIDLE, // option name
		(const char*)&keepidle, // value
		sizeof(keepidle) // size of value buffer
		) != 0)
		throw MySocketExcep("setsockopt failed, error: " + Networking::getError());

	// We will leave the TCP_KEEPINTVL unchanged at the default value.
#endif

#endif
}


void MySocket::initFDSetWithSocket(fd_set& sockset, SOCKETHANDLE_TYPE& sockhandle)
{
	FD_ZERO(&sockset);
	FD_SET(sockhandle, &sockset);
}
