/*=====================================================================
UDPSocket.cpp
-------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "UDPSocket.h"


#include "Networking.h"
#include "IPAddress.h"
#include "Packet.h"
#include "MySocket.h" // for MySocketExcep
#include <assert.h>
#include "../utils/Lock.h"
#include "../utils/StringUtils.h"
#include <string.h> // for memset()
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#include <Mstcpip.h>
#else
#include <netdb.h> // getaddrinfo
#include <netinet/in.h>
#include <unistd.h> // for close()
#include <sys/time.h> // fdset
#include <sys/types.h> // fdset
#include <sys/select.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <errno.h>
#include <fcntl.h>
//#include <poll.h>
#endif
#include <RuntimeCheck.h>


#if defined(_WIN32) || defined(_WIN64)
typedef int SOCKLEN_TYPE;
#else
typedef socklen_t SOCKLEN_TYPE;

static const int SOCKET_ERROR = -1;
#endif


UDPSocket::UDPSocket() // create outgoing socket
{
	socket_handle = 0;

	assert(Networking::isInitialised());
	if(!Networking::isInitialised())
		throw MySocketExcep("Networking not inited or destroyed.");
}


static int doCloseSocket(UDPSocket::SOCKETHANDLE_TYPE sockethandle)
{
#if defined(_WIN32)
	return closesocket(sockethandle);
#else
	return ::close(sockethandle);
#endif
}


UDPSocket::~UDPSocket()
{
	//-----------------------------------------------------------------
	// close socket
	//-----------------------------------------------------------------
	if(socket_handle)
	{
		//------------------------------------------------------------------------
		// try shutting down the socket
		//------------------------------------------------------------------------
		int result = shutdown(socket_handle, 1);
		assertOrDeclareUsed(result == 0);

		//-----------------------------------------------------------------
		// close socket
		//-----------------------------------------------------------------
		result = doCloseSocket(socket_handle);
		assert(result == 0);
	}
}


void UDPSocket::createClientSocket(bool use_IPv6)
{
	//-----------------------------------------------------------------
	// create a UDP socket
	//-----------------------------------------------------------------
	const int DEFAULT_PROTOCOL = 0; // use default protocol

	socket_handle = socket(use_IPv6 ? AF_INET6 : AF_INET, SOCK_DGRAM, DEFAULT_PROTOCOL);

	if(!isSockHandleValid(socket_handle))
		throw MySocketExcep("could not create a socket: " + Networking::getError());

	if(use_IPv6)
	{
		int no = 0;
		if(setsockopt(socket_handle, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&no, sizeof(no)) != 0)
		{
			assert(0);
			//conPrint("Warning: setsockopt failed.");
		}
	}
}


void UDPSocket::closeSocket()
{
	if(socket_handle)
	{
		doCloseSocket(socket_handle);
		socket_handle = nullSocketHandle();
	}
}


// listen to/send from a particular port
void UDPSocket::bindToPort(int port, bool reuse_address) 
{
	// Get the local IP address on which to listen.  This should allow supporting both IPv4 only and IPv4+IPv6 systems.
	// See http://www.microhowto.info/howto/listen_for_and_accept_tcp_connections_in_c.html
	addrinfo hints;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
	hints.ai_socktype = SOCK_DGRAM; // UDP
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
	if(isSockHandleValid(socket_handle))
		doCloseSocket(socket_handle);

	socket_handle = socket(addr_to_use->ai_family, addr_to_use->ai_socktype, addr_to_use->ai_protocol);
	if(!isSockHandleValid(socket_handle))
		throw MySocketExcep("Could not create a socket: " + Networking::getError());

	// Turn off IPV6_V6ONLY so that we can receive IPv4 connections as well.
	int no = 0;     
	if(setsockopt(socket_handle, IPPROTO_IPV6, IPV6_V6ONLY, (const char*)&no, sizeof(no)) != 0)
	{
		assert(0);
		//conPrint("!!!!!!!!!!!! Warning: setsockopt IPV6_V6ONLY failed.");
	}

	if(reuse_address)
		this->setAddressReuseEnabled(true);

	// conPrint("binding to " + IPAddress(*addr_to_use->ai_addr).toString() + "...");

	if(::bind(socket_handle, addr_to_use->ai_addr, (int)addr_to_use->ai_addrlen) == SOCKET_ERROR)
	{
		const std::string msg = "Failed to bind to address " + IPAddress(*addr_to_use->ai_addr).toString() + ", port " + toString(port) + ": " + Networking::getError();
		freeaddrinfo(results); // free the linked list
		throw MySocketExcep(msg);
	}

	freeaddrinfo(results); // free the linked list
}


const int UDPSocket::getThisEndPort() const
{
	struct sockaddr_storage sock_addr;
	SOCKLEN_TYPE namelen = sizeof(sock_addr);
	const int res = getsockname(socket_handle, (sockaddr*)&sock_addr, &namelen);
	if(res != 0)
		throw MySocketExcep("getsockname failed: " + Networking::getError());

	return Networking::getPortFromSockAddr(sock_addr);
}


void UDPSocket::enableBroadcast()
{
	const int optval = 1;
	const int result = ::setsockopt(
		socket_handle,
		SOL_SOCKET, // level
		SO_BROADCAST, // optname
		(const char*)&optval, // optval
		sizeof(optval) // optlen
	);

	assertOrDeclareUsed(result == 0);
}


void UDPSocket::sendPacket(const Packet& packet, const IPAddress& dest_ip, int destport)
{
	sendPacket((const unsigned char*)packet.getData(), packet.getPacketSize(), dest_ip, destport);
}


void UDPSocket::sendPacket(const void* data, size_t datalen, const IPAddress& dest_ip, int destport)
{
	assert(isSockHandleValid(socket_handle));

	//-----------------------------------------------------------------
	// create the destination address structure
	//-----------------------------------------------------------------
	sockaddr_storage dest_address;
	dest_ip.fillOutSockAddr(dest_address, destport);

	//-----------------------------------------------------------------
	// send it!
	//-----------------------------------------------------------------
	const int numbytessent = ::sendto(socket_handle, (const char*)data, (int)datalen, 0, 
							(struct sockaddr*)&dest_address, sizeof(sockaddr_storage));
	
	if(numbytessent == SOCKET_ERROR)
		throw makeMySocketExcepFromLastErrorCode("error while writing to UDP socket");

	if(numbytessent < (int)datalen)
	{
		// NOTE: FIXME this really an error?
		throw MySocketExcep("error: could not get all bytes in one packet.");
	}
}


// Returns num bytes read.  If the socket has been set to non-blocking mode, returns 0 if there are no packets to read.
size_t UDPSocket::readPacket(unsigned char* buf, size_t buflen, IPAddress& sender_ip_out, int& senderport_out)
{
	struct sockaddr_storage from_address; // senders's address information
	SOCKLEN_TYPE from_add_size = (SOCKLEN_TYPE)sizeof(from_address);

	const int numbytesrcvd = ::recvfrom(socket_handle, (char*)buf, (int)buflen, /*flags=*/0, (sockaddr*)&from_address, &from_add_size);

	if(numbytesrcvd == SOCKET_ERROR)
	{
#if defined(_WIN32)
		if(WSAGetLastError() == WSAEWOULDBLOCK) // The socket is marked as nonblocking and the recvfrom operation would block.  (In other words there is no incoming data available)
			return 0;
		else
			throw makeMySocketExcepFromLastErrorCode("error while reading from UDP socket");
#else
		if(errno == EAGAIN || errno == EWOULDBLOCK)
			return 0; // Then socket was marked as non-blocking and there was no data to be read.
		
		throw MySocketExcep("error while reading from socket.  Error code == " + Networking::getError());
#endif
	}

	//-----------------------------------------------------------------
	// get the sender IP and port
	//-----------------------------------------------------------------
	sender_ip_out = IPAddress(from_address);
	senderport_out = Networking::getPortFromSockAddr(from_address);

	runtimeCheck(numbytesrcvd >= 0);

	return numbytesrcvd;
}


void UDPSocket::setBlocking(bool blocking)
{
#if defined(_WIN32)

	unsigned long b = blocking ? 0 : 1;

	const int result = ioctlsocket(socket_handle, FIONBIO, &b);

	assert(result != SOCKET_ERROR);
#else
	const int current_flags = fcntl(socket_handle, F_GETFL);
	int new_flags;
	if(blocking)
		new_flags = current_flags & ~O_NONBLOCK;
	else
		new_flags = current_flags | O_NONBLOCK;

	const int result = fcntl(socket_handle, F_SETFL, new_flags);
	assert(result != -1);
#endif
	if(result == SOCKET_ERROR)
		throw MySocketExcep("setBlocking() failed. Error code: " + Networking::getError());
}


// Enables SO_REUSEADDR - allows a socket to bind to an address/port that is in the wait state.
void UDPSocket::setAddressReuseEnabled(bool enabled_)
{
	const int enabled = enabled_ ? 1 : 0;
	if(::setsockopt(socket_handle, // socket handle
		SOL_SOCKET, // level
		SO_REUSEADDR, // option name
		(const char*)&enabled, // value
		sizeof(enabled) // size of value buffer
	) != 0)
		throw MySocketExcep("setsockopt failed to set SO_REUSEADDR, error: " + Networking::getError());
}


UDPSocket::SOCKETHANDLE_TYPE UDPSocket::nullSocketHandle()
{
#if defined(_WIN32)
	return INVALID_SOCKET;
#else
	return -1;
#endif
}


bool UDPSocket::isSockHandleValid(SOCKETHANDLE_TYPE handle)
{
#if defined(_WIN32)
	return handle != INVALID_SOCKET;
#else
	return handle >= 0;
#endif
}
