/*=====================================================================
mysocket.h
----------
Copyright Glare Technologies Limited 2018 -
File created by ClassTemplate on Wed Apr 17 14:43:14 2002
=====================================================================*/
#pragma once


#if defined(_WIN32)
// Stop windows.h from defining the min() and max() macros
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <sys/select.h>
#endif

#include "ipaddress.h"
#include "SocketInterface.h"
#include "../utils/Platform.h"
#include "../utils/InStream.h"
#include "../utils/OutStream.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include <string>
class FractionListener;
class EventFD;


class MySocketExcep
{
public:
	enum ExcepType
	{
		ExcepType_ConnectionFailed, // Failed to connect to a host.
		ExcepType_BlockingCallCancelled,
		ExcepType_NotASocket,
		ExcepType_Other
	};

	MySocketExcep(const std::string& message_, ExcepType excep_type_ = ExcepType_Other) : message(message_), excep_type(excep_type_) {}

	const std::string& what() const { return message; }
	ExcepType excepType() const { return excep_type; }
private:
	std::string message;
	ExcepType excep_type;
};


/*=====================================================================
MySocket
--------
TCP socket class.
Blocking.
Does both client and server sockets.
=====================================================================*/
class MySocket : public SocketInterface
{
public:
#if defined(_WIN32)
	typedef SOCKET SOCKETHANDLE_TYPE;
#else
	typedef int SOCKETHANDLE_TYPE;
#endif

	MySocket(const std::string& hostname, int port); // Client connect via DNS lookup
	MySocket(const IPAddress& ipaddress, int port); // Client connect
	MySocket(SOCKETHANDLE_TYPE sockethandle_); // For sockets returned from acceptConnection().
	MySocket();

	~MySocket();

	// Connect given a hostname
	void connect(
		const std::string& hostname,
		int port
	);

	// Connect given an IP address
	void connect(
		const IPAddress& ipaddress,
		const std::string& hostname, // Just for printing out in exceptions.  Can be empty string.
		int port
	);


	// If reuse_address is true, enables SO_REUSEADDR on the socket so that it can be (re)bound during the wait state.
	void bindAndListen(int port, bool reuse_address = false); // throws MySocketExcep


	Reference<MySocket> acceptConnection(); // throws MySocketExcep

	// Calls shutdown on the socket, then closes the socket handle.
	// This will cause the socket to return from any blocking calls.
	virtual void ungracefulShutdown();

	// Wait for the other end to 'gracefully disconnect'
	// This allows the use of the 'Client Closes First' method from http://hea-www.harvard.edu/~fine/Tech/addrinuse.html
	// This is good because it allows the server to rebind to the same port without a long 30s wait on e.g. OS X.
	// Before this is called, the protocol should have told the other end to disconnect in some way. (e.g. a disconnect message)
	virtual void waitForGracefulDisconnect();

	const IPAddress& getOtherEndIPAddress() const{ return otherend_ipaddr; }
	int getOtherEndPort() const { return otherend_port; }


	//void writeInt32(int32 x);
	//void writeUInt32(uint32 x);
	void writeUInt64(uint64 x);
	void writeString(const std::string& s); // Write null-terminated string.

	//-----------------------------------------------------------------
	//if you use this directly you must do host->network and vice versa byte reordering yourself
	//-----------------------------------------------------------------
	void write(const void* data, size_t numbytes);
	void write(const void* data, size_t numbytes, FractionListener* frac);


	//int32 readInt32();
	//uint32 readUInt32();
	uint64 readUInt64();
	const std::string readString(size_t max_string_length); // Read null-terminated string.



	// Read 1 or more bytes from the socket, up to a maximum of max_num_bytes.  Returns number of bytes read.
	// Returns zero if connection was closed gracefully
	size_t readSomeBytes(void* buffer, size_t max_num_bytes);


	void readTo(void* buffer, size_t numbytes);
	void readTo(void* buffer, size_t numbytes, FractionListener* frac);

	virtual void setNoDelayEnabled(bool enabled); // NoDelay option is off by default.

	// Enable TCP Keep-alive, and set the period between keep-alive messages to 'period' seconds.
	virtual void enableTCPKeepAlive(float period);

	// Enables or disables SO_REUSEADDR - allows a socket to bind to an address/port that is in the wait state.
	virtual void setAddressReuseEnabled(bool enabled);

	bool readable(double timeout_s);
	bool readable(EventFD& event_fd); // Block until either the socket is readable or the event_fd is signalled (becomes readable).
	// Returns true if the socket was readable, false if the event_fd was signalled.

	// Determines if bytes are reordered into network byte order in readInt32(), writeInt32() etc..
	// Network byte order is enabled by default.
	void setUseNetworkByteOrder(bool use_network_byte_order_) { use_network_byte_order = use_network_byte_order_; }


	//------------------------ InStream ---------------------------------
	virtual int32 readInt32();
	virtual uint32 readUInt32();
	virtual void readData(void* buf, size_t num_bytes);
	virtual bool endOfStream();
	//------------------------------------------------------------------

	//------------------------ OutStream --------------------------------
	virtual void writeInt32(int32 x);
	virtual void writeUInt32(uint32 x);
	virtual void writeData(const void* data, size_t num_bytes);
	//------------------------------------------------------------------

	SOCKETHANDLE_TYPE getSocketHandle() { return sockethandle; }
	bool socketHandleValid() const { return isSockHandleValid(sockethandle); }

	void shutdown();

private:
	MySocket(const MySocket& other);
	MySocket& operator = (const MySocket& other);


	void init();
	void createClientSideSocket();
	static SOCKETHANDLE_TYPE nullSocketHandle();
	static bool isSockHandleValid(SOCKETHANDLE_TYPE handle);
	static void initFDSetWithSocket(fd_set& sockset, SOCKETHANDLE_TYPE& sockhandle);


	SOCKETHANDLE_TYPE sockethandle;

	IPAddress otherend_ipaddr;
	int otherend_port;

	bool connected;
	bool use_network_byte_order;
};


typedef Reference<MySocket> MySocketRef;
