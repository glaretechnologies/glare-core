/*=====================================================================
TLSSocket.h
-----------
Copyright Glare Technologies Limited 2020 -
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
#else
#include <sys/select.h>
#endif

#include "IPAddress.h"
#include "MySocket.h"
#include "../utils/Platform.h"
#include "../utils/InStream.h"
#include "../utils/OutStream.h"
#include "../utils/ThreadSafeRefCounted.h"
#include "../utils/Reference.h"
#include <string>
class FractionListener;
class EventFD;
struct tls;
struct tls_config;


#if defined(_MSC_VER)
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif


// Wrapper class for tls_config
class TLSConfig
{
public:
	TLSConfig(); // Allocates new tls_config.  Throws MySocketExcep on failure to alloc config.
	~TLSConfig(); // Frees tls_config.

	tls_config* config;
private:
	GLARE_DISABLE_COPY(TLSConfig);
};


// tls_error() can return NULL so use this wrapper function which handles the NULL return instead.
std::string getTLSErrorString(struct tls* tls_context);

// Wrapper around tls_config_error()
std::string getTLSConfigErrorString(struct tls_config* tls_config_);


/*=====================================================================
TLSSocket
---------
Socket with TLS.
Kind of a wrapper around LibTLS: http://www.openbsd.org/cgi-bin/man.cgi/OpenBSD-current/man3/tls_accept_fds.3?query=tls_init&sec=3

Tests in TLSSocketTests.
=====================================================================*/
class TLSSocket final : public SocketInterface
{
public:
#if defined(_WIN32)
	typedef SOCKET SOCKETHANDLE_TYPE;
#else
	typedef int SOCKETHANDLE_TYPE;
#endif

	TLSSocket(MySocketRef plain_socket, tls_config* client_tls_config, const std::string& servername); // Create client TLS socket
	TLSSocket(MySocketRef plain_socket, struct tls* tls_context); // Create server TLS socket.  Takes ownership of tls_context (frees in destructor).

	~TLSSocket();


	static void initTLS(); // Throws MySocketExcep on failure.


	// Calls shutdown on the socket, then closes the socket handle.
	// This will cause the socket to return from any blocking calls.
	virtual void ungracefulShutdown();

	// Wait for the other end to 'gracefully disconnect'
	// This allows the use of the 'Client Closes First' method from http://hea-www.harvard.edu/~fine/Tech/addrinuse.html
	// This is good because it allows the server to rebind to the same port without a long 30s wait on e.g. OS X.
	// Before this is called, the protocol should have told the other end to disconnect in some way. (e.g. a disconnect message)
	virtual void waitForGracefulDisconnect();

	virtual void startGracefulShutdown();

	//-----------------------------------------------------------------
	//if you use this directly you must do host->network and vice versa byte reordering yourself
	//-----------------------------------------------------------------
	void write(const void* data, size_t numbytes);
	void write(const void* data, size_t numbytes, FractionListener* frac);


	// Read 1 or more bytes from the socket, up to a maximum of max_num_bytes.  Returns number of bytes read.
	// Returns zero if connection was closed gracefully
	size_t readSomeBytes(void* buffer, size_t max_num_bytes);

	virtual void setNoDelayEnabled(bool enabled); // NoDelay option is off by default.

	virtual void enableTCPKeepAlive(float period);

	virtual void setAddressReuseEnabled(bool enabled);

	virtual IPAddress getOtherEndIPAddress() const { return plain_socket->getOtherEndIPAddress(); }
	virtual int getOtherEndPort() const { return plain_socket->getOtherEndPort(); }


	void readTo(void* buffer, size_t numbytes);
	void readTo(void* buffer, size_t numbytes, FractionListener* frac);


	bool readable(double timeout_s);
	bool readable(EventFD& event_fd); // Block until either the socket is readable or the event_fd is signalled (becomes readable).
	// Returns true if the socket was readable, false if the event_fd was signalled.



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

	struct tls* getTLSContext() { return tls_context; }

private:
	TLSSocket(const TLSSocket& other);
	TLSSocket& operator = (const TLSSocket& other);

	static void initFDSetWithSocket(fd_set& sockset, SOCKETHANDLE_TYPE& sockhandle);


	MySocketRef plain_socket;

	struct tls* tls_context;
};


typedef Reference<TLSSocket> TLSSocketRef;
