/*=====================================================================
MySocket.cpp
------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "MySocket.h"


#include "Networking.h"
#include "FractionListener.h"
#include "../maths/mathstypes.h"
#include "../utils/StringUtils.h"
#include "../utils/MyThread.h"
#include "../utils/PlatformUtils.h"
#include "../utils/Timer.h"
#include "../utils/EventFD.h"
#include "../utils/ConPrint.h"
#include "../utils/BitUtils.h"
#include "../utils/RuntimeCheck.h"
#include <vector>
#include <string.h>
#include <algorithm>
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Mstcpip.h>
#else
#include <netdb.h> // getaddrinfo
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
typedef int SockLenType;
#else
typedef socklen_t SockLenType;

static const int SOCKET_ERROR = -1;
#endif


static size_t MAX_READ_OR_WRITE_SIZE = 1024 * 1024 * 8;


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
	assert(Networking::isInitialised());
	if(!Networking::isInitialised())
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
	assert(Networking::isInitialised());

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
	assert(Networking::isInitialised());

	init();

	sockethandle = sockethandle_;
}


MySocket::MySocket()
{
	init();
}


void MySocket::init()
{
	otherend_port = -1;
	sockethandle = nullSocketHandle();
	use_network_byte_order = true;
	use_IPv4_only = false;
}


MySocket::~MySocket()
{
	closeSocket(sockethandle);
}


void MySocket::createClientSideSocket()
{
	assert(!isSockHandleValid(sockethandle));

	// Create socket
	sockethandle = socket(
		AF_INET6, SOCK_STREAM,
		0 // protocol - default
	);

	if(!isSockHandleValid(sockethandle))
	{
#if !defined(_WIN32)
		// Try falling back to creating an IPv4 socket.  May help when IPv6 is not supported.
		// Don't do this on Windows - "Applications are encouraged to use AF_INET6 for the af parameter and create a dual-mode socket that can be used with both IPv4 and IPv6." (https://learn.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-socket)
		sockethandle = socket(
			AF_INET, SOCK_STREAM,
			0 // protocol - default
		);
		if(!isSockHandleValid(sockethandle))
			throw MySocketExcep("Could not create a socket: " + Networking::getError());

		this->use_IPv4_only = true; // When falling back to an IPv4 socket, don't use any IPv6 stuff afterwards.
#else
		throw MySocketExcep("Could not create a socket: " + Networking::getError());
#endif
	}

	// Turn off IPV6_V6ONLY so that we can receive IPv4 connections as well.  Only do this if we have an IPv6 socket.
	if(!use_IPv4_only)
	{
		int no = 0;
		if(setsockopt(sockethandle, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&no, sizeof(no)) != 0)
		{
			assert(0);
			//conPrint("Warning: setsockopt failed.");
		}
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
		const std::vector<IPAddress> serverips = Networking::doDNSLookup(hostname);
		runtimeCheck(!serverips.empty());

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

	if(sockethandle == nullSocketHandle())
		createClientSideSocket();

	//-----------------------------------------------------------------
	//Fill out server address structure
	//-----------------------------------------------------------------
	sockaddr_storage server_address;
	if(use_IPv4_only)
		ipaddress.fillOutSockAddr(server_address, port);
	else
		ipaddress.fillOutIPV6SockAddr(server_address, port);

	//-----------------------------------------------------------------
	//Connect to server
	//-----------------------------------------------------------------
	if(::connect(sockethandle, (sockaddr*)&server_address, sizeof(server_address)) != 0)
	{
#if defined(_WIN32)
		if(WSAGetLastError() != WSAEWOULDBLOCK)
#else
		if(errno != EINPROGRESS)
#endif
		{
			const std::string error_str = Networking::getError(); // Get error before we call anything else, which may overwrite it.
			if(hostname.empty())
				throw MySocketExcep("Could not connect to " + IPAddress::formatIPAddressAndPort(ipaddress, port) + ": " + error_str, MySocketExcep::ExcepType_ConnectionFailed);
			else
				throw MySocketExcep("Could not connect to '" + hostname + "' (" + IPAddress::formatIPAddressAndPort(ipaddress, port) + "): " + error_str, MySocketExcep::ExcepType_ConnectionFailed);
		}
	}

	otherend_port = port;
}


void MySocket::bindAndListen(int port, bool reuse_address)
{
	Timer timer;

	assert(Networking::isInitialised());

	// Get the local IP address on which to listen.  This should allow supporting both IPv4 only and IPv4+IPv6 systems.
	// See http://www.microhowto.info/howto/listen_for_and_accept_tcp_connections_in_c.html
	addrinfo hints;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	hints.ai_protocol = 0;
	hints.ai_flags = AI_PASSIVE; // "Setting the AI_PASSIVE flag indicates the caller intends to use the returned socket address structure in a call to the bind function": https://msdn.microsoft.com/en-gb/library/windows/desktop/ms738520(v=vs.85).aspx
#if !defined(_WIN32) 
	hints.ai_flags |= AI_ADDRCONFIG; // On Linux, some users were having issues with getaddrinfo returning IPv6 addresses which were not valid (IPv6 support was disabled).
	// AI_ADDRCONFIG will hopefully prevent invalid IPv6 addresses being returned by getaddrinfo.
#endif

	struct addrinfo* results = NULL;

	const char* nodename = NULL;
	const std::string portname = toString(port);
	const int errval = getaddrinfo(nodename, portname.c_str(), &hints, &results);
	if(errval != 0)
		throw MySocketExcep("getaddrinfo failed: " + Networking::getError());

	if(!results)
		throw MySocketExcep("getaddrinfo did not return a result.");
		
	// We want to use the first IPv6 address, if there is one, otherwise the first address overall.
	struct addrinfo* first_ipv6_addr = NULL;
	for(struct addrinfo* cur = results; cur != NULL ; cur = cur->ai_next)
	{
		// conPrint("address: " + IPAddress(*cur->ai_addr).toString() + "...");
		if(cur->ai_family == AF_INET6 && first_ipv6_addr == NULL)
			first_ipv6_addr = cur;
	}
	
	struct addrinfo* addr_to_use = (first_ipv6_addr != NULL) ? first_ipv6_addr : results;

	// Create socket, now that we know the correct address family to use.
	if(isSockHandleValid(sockethandle))
		closeSocket(sockethandle);

	sockethandle = socket(addr_to_use->ai_family, addr_to_use->ai_socktype, addr_to_use->ai_protocol);
	if(!isSockHandleValid(sockethandle))
		throw MySocketExcep("Could not create a socket: " + Networking::getError());

	// Turn off IPV6_V6ONLY so that we can receive IPv4 connections as well.
	int no = 0;     
	if(setsockopt(sockethandle, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&no, sizeof(no)) != 0)
	{
		assert(0);
		//conPrint("!!!!!!!!!!!! Warning: setsockopt IPV6_V6ONLY failed.");
	}

	if(reuse_address)
		this->setAddressReuseEnabled(true);

	// conPrint("binding to " + IPAddress(*addr_to_use->ai_addr).toString() + "...");

	if(::bind(sockethandle, addr_to_use->ai_addr, (int)addr_to_use->ai_addrlen) == SOCKET_ERROR)
	{
		const std::string msg = "Failed to bind to address " + IPAddress(*addr_to_use->ai_addr).toString() + ", port " + toString(port) + ": " + Networking::getError();
		freeaddrinfo(results); // free the linked list
		throw MySocketExcep(msg);
	}

	const int backlog = 10;

	if(::listen(sockethandle, backlog) == SOCKET_ERROR)
	{
		freeaddrinfo(results); // free the linked list
		throw MySocketExcep("listen failed: " + Networking::getError());
	}

	freeaddrinfo(results); // free the linked list


	/*
	Old code that doesn't use getaddrinfo():

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
		throw MySocketExcep("listen failed: " + Networking::getError());*/
}


MySocketExcep makeMySocketExcepFromLastErrorCode(const std::string& msg)
{
	// Get error code, depending on platform:
	// Note that in practice, WSAGetLastError seems to be just an alias for GetLastError: http://stackoverflow.com/questions/15586224/is-wsagetlasterror-just-an-alias-for-getlasterror
#if defined(_WIN32) 
	const DWORD error_code = GetLastError();
#else
	const int error_code = errno;
#endif

	// Translate to human-readable message
	const std::string error_code_msg = PlatformUtils::getErrorStringForCode(error_code);

	// Determine correct ExcepType:
	MySocketExcep::ExcepType excep_type = MySocketExcep::ExcepType_Other;
#if defined(_WIN32) 
	if(error_code == WSAEINTR) // A blocking operation was interrupted by a call to WSACancelBlockingCall.
		excep_type = MySocketExcep::ExcepType_BlockingCallCancelled;
	else if(error_code == WSAENOTSOCK) // Socket operation on nonsocket.  (Will be caused by killing the socket from another thread)
		excep_type = MySocketExcep::ExcepType_NotASocket;
#endif

	return MySocketExcep(msg + ": " + error_code_msg, excep_type);
}


MySocketRef MySocket::acceptConnection() // throw (MySocketExcep)
{
	assert(Networking::isInitialised());

	sockaddr_storage client_addr; // Data struct to get the client IP
	SockLenType length = sizeof(client_addr);
	memset(&client_addr, 0, length);
	SOCKETHANDLE_TYPE newsockethandle = ::accept(sockethandle, (sockaddr*)&client_addr, &length);

	if(!isSockHandleValid(newsockethandle))
		throw makeMySocketExcepFromLastErrorCode("accept failed");

	//-----------------------------------------------------------------
	//copy data over to new socket that will do actual communicating
	//-----------------------------------------------------------------
	MySocketRef new_socket = new MySocket(newsockethandle);

	//-----------------------------------------------------------------
	// Get other end ip and port
	//-----------------------------------------------------------------
	new_socket->otherend_ipaddr = IPAddress((const sockaddr&)client_addr);
	new_socket->otherend_port = Networking::getPortFromSockAddr((const sockaddr&)client_addr);

	return new_socket;
}


// Closes writing side of socket.  Tells sockets lib to send a FIN packet to the server.
// Initiate graceful shutdown.
// See 'Graceful Shutdown, Linger Options, and Socket Closure' - https://msdn.microsoft.com/en-us/library/ms738547
void MySocket::startGracefulShutdown()
{
	if(isSockHandleValid(sockethandle))
		::shutdown(sockethandle, 1); // Shutdown send operations.  (1 == SD_SEND)
}


void MySocket::ungracefulShutdown()
{
	if(isSockHandleValid(sockethandle))
	{
		::shutdown(sockethandle, 2); // 2 == SD_BOTH
		closeSocket(sockethandle); // Close the socket.  This will cause the socket to return from any blocking calls.
		sockethandle = nullSocketHandle();
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
			throw makeMySocketExcepFromLastErrorCode("Read failed");
		else if(numbytesread == 0) // Connection was closed gracefully
		{
			// conPrint("\tConnection was closed gracefully.");
			return;
		}
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
		const int numbytestowrite = (int)std::min(MAX_READ_OR_WRITE_SIZE, datalen);
		assert(numbytestowrite > 0);

		const int numbyteswritten = send(sockethandle, (const char*)data, numbytestowrite, 0);

		if(numbyteswritten == SOCKET_ERROR)
			throw makeMySocketExcepFromLastErrorCode("write failed");

		datalen -= numbyteswritten;
		data = (void*)((char*)data + numbyteswritten); // Advance data pointer

		if(frac)
			frac->setFraction((float)(totalnumbytestowrite - datalen) / (float)totalnumbytestowrite);
	}
}


// Read 1 or more bytes from the socket, up to a maximum of max_num_bytes.  Returns number of bytes read.
// Returns zero if connection was closed gracefully
size_t MySocket::readSomeBytes(void* buffer, size_t max_num_bytes)
{
	const int numbytesread = recv(sockethandle, (char*)buffer, (int)max_num_bytes, 0);

	if(numbytesread == SOCKET_ERROR) // Connection was reset/broken
		throw makeMySocketExcepFromLastErrorCode("Read failed");

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
		const int numbytestoread = (int)std::min(MAX_READ_OR_WRITE_SIZE, readlen);
		assert(numbytestoread > 0);

		//------------------------------------------------------------------------
		//do the read
		//------------------------------------------------------------------------
		const int numbytesread = recv(sockethandle, (char*)buffer, numbytestoread, 0);

		if(numbytesread == SOCKET_ERROR) // Connection was reset/broken
			throw makeMySocketExcepFromLastErrorCode("Read failed");
		else if(numbytesread == 0) // Connection was closed gracefully
			throw MySocketExcep("Connection Closed.", MySocketExcep::ExcepType_ConnectionClosedGracefully);

		readlen -= numbytesread;
		buffer = (void*)((char*)buffer + numbytesread);

		if(frac)
			frac->setFraction((float)(totalnumbytestoread - readlen) / (float)totalnumbytestoread);
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
	if(use_network_byte_order)
		x = bitCast<int32>(htonl(bitCast<uint32>(x)));

	write(&x, sizeof(int32));
}


void MySocket::writeUInt32(uint32 x)
{
	if(use_network_byte_order)
		x = htonl(x); // Convert to network byte ordering.

	write(&x, sizeof(uint32));
}


void MySocket::writeUInt64(uint64 x)
{
	if(use_network_byte_order)
	{
		uint32 i32[2];
		std::memcpy(i32, &x, sizeof(uint64));
		writeUInt32(i32[0]);
		writeUInt32(i32[1]);
	}
	else
		write(&x, sizeof(uint64));
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
	int32 i;
	readTo(&i, sizeof(int32));

	if(use_network_byte_order)
		i = bitCast<int32>(ntohl(bitCast<uint32>(i)));

	return i;
}


uint32 MySocket::readUInt32()
{
	uint32 x;
	readTo(&x, sizeof(uint32));

	if(use_network_byte_order)
		x = ntohl(x);

	return x;
}


uint64 MySocket::readUInt64()
{
	if(use_network_byte_order)
	{
		uint32 buf[2];
		buf[0] = readUInt32();
		buf[1] = readUInt32();
		uint64 x;
		std::memcpy(&x, buf, sizeof(uint64));
		return x;
	}
	else
	{
		uint64 x;
		readTo(&x, sizeof(uint64));
		return x;
	}
}


bool MySocket::readable(double timeout_s)
{
	timeval wait_period;
	wait_period.tv_sec = (long)timeout_s;
	const double frac_sec = timeout_s - (double)((long)timeout_s);
	wait_period.tv_usec = (long)(frac_sec * 1000000.0);

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
#if defined(_WIN32) || defined(__APPLE__)
	assert(0);
	return false;
#else
	
	struct pollfd poll_fds[2];
	poll_fds[0].fd = sockethandle;
	poll_fds[0].events = POLLIN | POLLRDHUP; // Listen for data to read (POLLIN) or peer clonnection close/shutdown call (POLLRDHUP).  See https://man7.org/linux/man-pages/man2/poll.2.html
	poll_fds[0].revents = 0;
	
	poll_fds[1].fd = event_fd.efd;
	poll_fds[1].events = POLLIN | POLLRDHUP; // NOTE: Do we want POLLRDHUP here?
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


MySocket::SOCKETHANDLE_TYPE MySocket::nullSocketHandle()
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
	const int enabled = enabled_ ? 1 : 0;
	if(::setsockopt(sockethandle, // socket handle
		IPPROTO_TCP, // level
		TCP_NODELAY, // option name
		(const char*)&enabled, // value
		sizeof(enabled) // size of value buffer
		) != 0)
		throw MySocketExcep("setsockopt failed, error: " + Networking::getError());
}


// Enable TCP Keep-alive, and set the period between keep-alive messages to 'period' seconds.
void MySocket::enableTCPKeepAlive(float period)
{
#if defined(_WIN32)
	assert(period <= (float)std::numeric_limits<ULONG>::max() / 1000.f); // Make sure period is representable in an ULONG.

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

#if defined(__APPLE__)
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


// Enables SO_REUSEADDR - allows a socket to bind to an address/port that is in the wait state.
void MySocket::setAddressReuseEnabled(bool enabled_)
{
	const int enabled = enabled_ ? 1 : 0;
	if(::setsockopt(sockethandle, // socket handle
		SOL_SOCKET, // level
		SO_REUSEADDR, // option name
		(const char*)&enabled, // value
		sizeof(enabled) // size of value buffer
	) != 0)
		throw MySocketExcep("setsockopt failed to set SO_REUSEADDR, error: " + Networking::getError());
}


void MySocket::initFDSetWithSocket(fd_set& sockset, SOCKETHANDLE_TYPE& sockhandle)
{
	FD_ZERO(&sockset);
	FD_SET(sockhandle, &sockset);
}
