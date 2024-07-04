/*=====================================================================
UDPSocket.h
-----------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#if defined(_WIN32)
// Stop windows.h from defining the min() and max() macros
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#endif
#include <stddef.h> // for size_t

#include "../utils/ThreadSafeRefCounted.h"
class IPAddress;
class Packet;


/*=====================================================================
UDPSocket
---------
Provides an interface to the net's connectionless service

Methods throw MySocketExcep on failure.
=====================================================================*/
class UDPSocket : public ThreadSafeRefCounted
{
public:
	UDPSocket(); // Doesn't create socket
	~UDPSocket();

	void createClientSocket(bool use_IPv6);

	void closeSocket();

	// Calls shutdown on the socket, then closes the socket handle.
	// This will cause the socket to return from any blocking calls.
	void ungracefulShutdown();

	void bindToPort(int port, bool reuse_address = false); // listen to/send from a particular port.  Creates the socket.

	int getThisEndPort() const;

	// createClientSocket() should be called first()
	void sendPacket(const Packet& packet, const IPAddress& dest_ip, int destport);
	void sendPacket(const void* data, size_t datalen, const IPAddress& dest_ip, int destport);

	// Returns num bytes read.  If the socket has been set to non-blocking mode, returns 0 if there are no packets to read.
	size_t readPacket(unsigned char* buf, size_t buflen, IPAddress& sender_ip_out, int& senderport_out);

	void setBlocking(bool blocking);

	void enableBroadcast();


#if defined(_WIN32)
	typedef SOCKET SOCKETHANDLE_TYPE;
#else
	typedef int SOCKETHANDLE_TYPE;
#endif
private:
	void setAddressReuseEnabled(bool enabled);
	SOCKETHANDLE_TYPE nullSocketHandle();
	bool isSockHandleValid(SOCKETHANDLE_TYPE handle);

	SOCKETHANDLE_TYPE socket_handle;
};
