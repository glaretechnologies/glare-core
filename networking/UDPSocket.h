/*=====================================================================
UDPSocket.h
-----------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#if defined(_WIN32)
// Stop windows.h from defining the min() and max() macros
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#endif

#include "Port.h"
#include "../utils/Mutex.h"
#include <string>
class IPAddress;
class Packet;


class UDPSocketExcep
{
public:
	UDPSocketExcep(){}
	UDPSocketExcep(const std::string& message_) : message(message_) {}
	~UDPSocketExcep(){}

	const std::string& what() const { return message; }
private:
	std::string message;
};


/*=====================================================================
UDPSocket
---------
Provides an interface to the net's connectionless service

NOTE: Still basically IPv4 only.
=====================================================================*/
class UDPSocket
{
public:
	UDPSocket();
	~UDPSocket();

	void bindToPort(const Port& port); // listen to/send from a particular port

	const Port& getThisEndPort() const { return thisend_port; }

	void sendPacket(const Packet& packet, const IPAddress& dest_ip, const Port& destport);
	void sendPacket(const char* data, int datalen, const IPAddress& dest_ip, const Port& destport);

	// Returns num bytes read.  If the socket has been set to non-blocking mode, returns 0 if there are no packets to read.
	int readPacket(char* buf, int buflen, IPAddress& sender_ip_out, Port& senderport_out);

	void setBlocking(bool blocking);

	void enableBroadcast();

private:
#if defined(_WIN32) || defined(_WIN64)
	typedef SOCKET SOCKETHANDLE_TYPE;
#else
	typedef int SOCKETHANDLE_TYPE;
#endif

	bool isSockHandleValid(SOCKETHANDLE_TYPE handle);

	SOCKETHANDLE_TYPE socket_handle;
	Port thisend_port;
};
