/*=====================================================================
SocketInterface.h
-----------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "IPAddress.h"
#include "../utils/InStream.h"
#include "../utils/OutStream.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
class EventFD;


/*=====================================================================
SocketInterface
---------------
Abstract interface class for a socket.
MySocket and TLSSocket, and TestSocket will inherit from this class.
=====================================================================*/
class SocketInterface : public InStream, public OutStream, public ThreadSafeRefCounted
{
public:
	virtual ~SocketInterface();

	// Calls shutdown on the socket, then closes the socket handle.
	// This will cause the socket to return from any blocking calls.
	virtual void ungracefulShutdown() = 0;

	// Closes writing side of socket.  Tells sockets lib to send a FIN packet to the server.
	virtual void startGracefulShutdown() = 0;

	// Wait for the other end to 'gracefully disconnect', e.g. wait for the other end to send a FIN packet.
	// This allows the use of the 'Client Closes First' method from http://hea-www.harvard.edu/~fine/Tech/addrinuse.html
	// This is good because it allows the server to rebind to the same port without a long 30s wait on e.g. OS X.
	// Therefore this is especially useful in the server code.
	virtual void waitForGracefulDisconnect() = 0;

	// Read 1 or more bytes from the socket, up to a maximum of max_num_bytes.  Returns number of bytes read.
	// Returns zero if connection was closed gracefully.
	virtual size_t readSomeBytes(void* buffer, size_t max_num_bytes) = 0;

	size_t readSomeBytesChecked(MutableArrayRef<uint8> buf, size_t buf_index, size_t max_num_bytes);

	virtual void setNoDelayEnabled(bool enabled) = 0; // NoDelay option is off by default.

	// Enable TCP Keep-alive, and set the period between keep-alive messages to 'period' seconds.
	virtual void enableTCPKeepAlive(float period) = 0;

	// Enables SO_REUSEADDR - allows a socket to bind to an address/port that is in the wait state.
	virtual void setAddressReuseEnabled(bool enabled) = 0;

	virtual bool readable(double timeout_s) = 0;
	virtual bool readable(EventFD& event_fd) = 0; // Block until either the socket is readable or the event_fd is signalled (becomes readable).
	// Returns true if the socket was readable, false if the event_fd was signalled.

	virtual IPAddress getOtherEndIPAddress() const = 0;
	virtual int getOtherEndPort() const = 0;

	virtual void flush() {}
};


typedef Reference<SocketInterface> SocketInterfaceRef;
