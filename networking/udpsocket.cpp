/*=====================================================================
udpsocket.cpp
-------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "udpsocket.h"


#include "networking.h"
#include "ipaddress.h"
#include "packet.h"
#include <assert.h>
#include "../utils/Lock.h"
#include "../utils/StringUtils.h"
#include <string.h> // for memset()
#if !defined(_WIN32)
#include <netinet/in.h>
#include <unistd.h> // for close()
#include <sys/time.h> // fdset
#include <sys/types.h> // fdset
#include <sys/select.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#endif


#if defined(_WIN32) || defined(_WIN64)
static bool isSocketError(int result)
{
	return result == SOCKET_ERROR;
}
#else
static bool isSocketError(ssize_t result)
{
	return result == -1;
}
#endif


#if defined(_WIN32) || defined(_WIN64)
typedef int SOCKLEN_TYPE;
#else
typedef socklen_t SOCKLEN_TYPE;
#endif


UDPSocket::UDPSocket() // create outgoing socket
{
	thisend_port = 0;
	socket_handle = 0;

	assert(Networking::isInited());
	if(!Networking::isInited())
		throw UDPSocketExcep("Networking not inited or destroyed.");

	//-----------------------------------------------------------------
	// create a UDP socket
	//-----------------------------------------------------------------
	const int DEFAULT_PROTOCOL = 0; // use default protocol

	socket_handle = socket(PF_INET, SOCK_DGRAM, DEFAULT_PROTOCOL);

	if(!isSockHandleValid(socket_handle))
		throw UDPSocketExcep("could not create a socket");
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
		assert(result == 0);

		//-----------------------------------------------------------------
		// close socket
		//-----------------------------------------------------------------
#if defined(_WIN32) || defined(_WIN64)
		result = closesocket(socket_handle);
#else
		result = ::close(socket_handle);
#endif	
		assert(result == 0);
	}
}


// listen to/send from a particular port
void UDPSocket::bindToPort(const Port& port) 
{
	//-----------------------------------------------------------------
	// bind the socket to a specific port
	//-----------------------------------------------------------------
	struct sockaddr_in my_addr; // my address information

	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(port.getPort());
	my_addr.sin_addr.s_addr = INADDR_ANY; // accept on any network interface
	memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct

	if(bind(socket_handle, (struct sockaddr*)&my_addr, sizeof(my_addr)) != 0) 
	{
		throw UDPSocketExcep("Error binding socket to port " + port.toString() + ": " + Networking::getInstance().getError());
	}

	thisend_port = port;
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


void UDPSocket::sendPacket(const Packet& packet, const IPAddress& dest_ip, const Port& destport)
{
	sendPacket(packet.getData(), packet.getPacketSize(), dest_ip, destport);
}


void UDPSocket::sendPacket(const char* data, int datalen, const IPAddress& dest_ip, const Port& destport)
{
	//-----------------------------------------------------------------
	// create the destination address structure
	//-----------------------------------------------------------------
	sockaddr_storage dest_address;
	dest_ip.fillOutSockAddr(dest_address, destport.getPort());

	//-----------------------------------------------------------------
	// send it!
	//-----------------------------------------------------------------
	const int numbytessent = ::sendto(socket_handle, data, datalen, 0, 
							(struct sockaddr*)&dest_address, sizeof(sockaddr));
	
	if(isSocketError(numbytessent))
	{
		throw UDPSocketExcep("error while sending bytes over socket: " + Networking::getError());
	}

	if(numbytessent < datalen)
	{
		// NOTE: FIXME this really an error?
		throw UDPSocketExcep("error: could not get all bytes in one packet.");
	}
}


// Returns num bytes read.  If the socket has been set to non-blocking mode, returns 0 if there are no packets to read.
int UDPSocket::readPacket(char* buf, int buflen, IPAddress& sender_ip_out, Port& senderport_out)
{
	struct sockaddr from_address; // senders's address information
	SOCKLEN_TYPE from_add_size = sizeof(from_address);

	//-----------------------------------------------------------------
	// get one packet.
	//-----------------------------------------------------------------
	const int numbytesrcvd = ::recvfrom(socket_handle, buf, buflen, /*flags=*/0, &from_address, &from_add_size);

	if(isSocketError(numbytesrcvd))
	{
#if defined(_WIN32) || defined(_WIN64)
		if(WSAGetLastError() == WSAEWOULDBLOCK) // The socket is marked as nonblocking and the recvfrom operation would block.  (In other words there is no incoming data available)
		{
			return 0;
		}
		else
		{
			const std::string errorstring = Networking::getError();
			if(errorstring == "WSAECONNRESET")
			{
				// socket is dead thanks to stupid xp... so reopen it
				// recommended solution is to just use a different socket for sending UDP datagrams.

				closesocket(socket_handle);

				//-----------------------------------------------------------------
				//open socket
				//-----------------------------------------------------------------
				const int DEFAULT_PROTOCOL = 0;			// use default protocol

				socket_handle = socket(PF_INET, SOCK_DGRAM, DEFAULT_PROTOCOL);

				if(socket_handle == INVALID_SOCKET)
				{
					throw UDPSocketExcep("could not RE-create a socket after WSAECONNRESET");
				}

				//-----------------------------------------------------------------
				//bind to port it was on before
				//-----------------------------------------------------------------
				assert(thisend_port.getPort() != 0);

				struct sockaddr_in my_addr;    // my address information

				my_addr.sin_family = AF_INET;         
				my_addr.sin_port = htons(thisend_port.getPort());     
				my_addr.sin_addr.s_addr = INADDR_ANY; // accept on any network interface
				memset(&(my_addr.sin_zero), '\0', 8); // zero the rest of the struct

				if(bind(socket_handle, (struct sockaddr*)&my_addr, sizeof(struct sockaddr)) == -1) 
				{
 					throw UDPSocketExcep("error RE-binding socket to port " + thisend_port.toString() + " after WSAECONNRESET");
				}
				
				// now just throw an excep and ignore the current packet, UDP is unreliable anyway..
			}

			throw UDPSocketExcep("error while reading from socket: error code == " + errorstring);
		}
#else
		if(errno == EAGAIN || errno == EWOULDBLOCK)
			return 0; // Then socket was marked as non-blocking and there was no data to be read.
		
		throw UDPSocketExcep("error while reading from socket.  Error code == " + Networking::getError());
#endif
	}

	//-----------------------------------------------------------------
	// get the sender IP and port
	//-----------------------------------------------------------------
	sender_ip_out = IPAddress(from_address);
	senderport_out = (unsigned short)Networking::getPortFromSockAddr(from_address);

	return numbytesrcvd;
}


void UDPSocket::setBlocking(bool blocking)
{
#if defined(_WIN32) || defined(_WIN64)

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
	if(isSocketError(result))
		throw UDPSocketExcep("setBlocking() failed. Error code: " + Networking::getError());
}


bool UDPSocket::isSockHandleValid(SOCKETHANDLE_TYPE handle)
{
#if defined(_WIN32) || defined(_WIN64)
	return handle != INVALID_SOCKET;
#else
	return handle >= 0;
#endif
}
