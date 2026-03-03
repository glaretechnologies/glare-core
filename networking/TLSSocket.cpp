/*=====================================================================
TLSSocket.cpp
-------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "TLSSocket.h"


#if USING_LIBRESSL


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
#include "../utils/OpenSSL.h"
#include <vector>
#include <string.h>
#include <algorithm>
#include <tls.h>
#include <openssl/ssl.h>
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

// This is a hack so we can recover the internal tls structure, and call SSL_pending on the ssl_conn member.

// Modified from C:\programming\LibreSSL\libressl-2.8.3\tls\tls_internal.h
struct glare_tls_error {
	char *msg;
	int num;
	int tls;
};

struct glare_tls {
	struct tls_config *config;
	struct tls_keypair *keypair;

	struct glare_tls_error error;

	uint32_t flags;
	uint32_t state;

	char *servername;
	int socket;

	SSL *ssl_conn;
	SSL_CTX *ssl_ctx;

	// More members follow...
};

// The hacked in structure above may become invalid for different libtls versions.
static_assert(TLS_API == 20180210 || TLS_API == 20200120, "TLS_API == 20180210 || TLS_API == 20200120"); // Structure is the same in both these versions (2.8.3 and 3.3.3)


#if defined(_WIN32)
#else
static const int SOCKET_ERROR = -1;
#endif


static size_t MAX_READ_OR_WRITE_SIZE = 1024 * 1024 * 8;


TLSConfig::TLSConfig()
{
	this->config = tls_config_new();
	if(!this->config)
		throw MySocketExcep("Failed to allocate tls_config.");
}


TLSConfig::~TLSConfig()
{
	if(this->config)
		tls_config_free(this->config);
}


std::string getTLSErrorString(struct tls* tls_context)
{
	const char* err = tls_error(tls_context); // tls_error can return NULL
	return err ? std::string(err) : std::string("[No error reported from TLS lib]");
}


std::string getTLSConfigErrorString(struct tls_config* tls_config_)
{
	const char* err = tls_config_error(tls_config_);
	return err ? std::string(err) : std::string("[No error reported from TLS lib]");
}


TLSSocket::TLSSocket(MySocketRef plain_socket_, tls_config* client_tls_config, const std::string& servername)
{
	plain_socket = plain_socket_;

	tls_context = tls_client();
	if(!tls_context)
		throw MySocketExcep("Failed to create tls_context.");

	if(tls_configure(tls_context, client_tls_config) != 0)
		throw MySocketExcep("tls_configure failed: " + getTLSErrorString(tls_context));

	if(tls_connect_socket(tls_context, (int)plain_socket->getSocketHandle(), servername.c_str()) != 0)
		throw MySocketExcep("tls_connect_socket failed: " + getTLSErrorString(tls_context));

	//Timer timer;
	// Calling tls_handshake explicitly is optional, but it's nice to get any handshake error messages now, instead of in a later read or write call.
	if(tls_handshake(tls_context) != 0)
		throw MySocketExcep("tls_handshake failed: " + getTLSErrorString(tls_context));
	//conPrint("Client TLS handshake took " + timer.elapsedStringNSigFigs(4));

	//conPrint("tls_conn_cipher: " + std::string(tls_conn_cipher(tls_context)));
	//conPrint("tls_conn_version: " + std::string(tls_conn_version(tls_context)));
}


TLSSocket::TLSSocket(MySocketRef plain_socket_, struct tls* tls_context_)
{
	plain_socket = plain_socket_;

	tls_context = tls_context_;

	//Timer timer;
	//if(tls_handshake(tls_context) != 0) //TEMP NEW
	//	throw MySocketExcep("tls_handshake failed: " + getTLSErrorString(tls_context));
	//conPrint("Server TLS handshake took " + timer.elapsedStringNSigFigs(4));
}


TLSSocket::~TLSSocket()
{
	// If the socket has been closed in an ungracefulShutdown() call, tls_close hits an assert, so avoid that.
	if(plain_socket->socketHandleValid())
	{
		// "tls_close() closes a connection after use. Only the TLS layer will be shut down and the caller is responsible for closing the file descriptors" (https://sortix.org/man/man3/tls_close.3.html)
		tls_close(tls_context);
	}

	tls_free(tls_context); // Free TLS context.
}


void TLSSocket::initTLS()
{
	if(tls_init() != 0)
		throw MySocketExcep("tls_init failed.");
}


void TLSSocket::write(const void* data, size_t datalen)
{
	write(data, datalen, NULL);
}


void TLSSocket::write(const void* data, size_t datalen, FractionListener* frac)
{
	const size_t total_num_bytes_to_write = datalen;

	while(datalen > 0) // while still bytes to write:
	{
		const size_t num_bytes_to_write = std::min(MAX_READ_OR_WRITE_SIZE, datalen);
		assert(num_bytes_to_write > 0);

		const ssize_t num_bytes_written = tls_write(tls_context, data, num_bytes_to_write);
		if(num_bytes_written == TLS_WANT_POLLIN || num_bytes_written == TLS_WANT_POLLOUT)
			continue;

		if(num_bytes_written == SOCKET_ERROR)
			throw MySocketExcep("write failed: " + getTLSErrorString(tls_context));

		datalen -= num_bytes_written;
		data = (void*)((uint8*)data + num_bytes_written); // Advance data pointer

		if(frac)
			frac->setFraction((float)(total_num_bytes_to_write - datalen) / (float)total_num_bytes_to_write);
	}
}


size_t TLSSocket::readSomeBytes(void* buffer, size_t max_num_bytes)
{
	while(1)
	{
		const ssize_t num_bytes_read = tls_read(tls_context, buffer, max_num_bytes);
		if(num_bytes_read == TLS_WANT_POLLIN || num_bytes_read == TLS_WANT_POLLOUT)
			continue;

		if(num_bytes_read == SOCKET_ERROR) // Connection was reset/broken
			throw MySocketExcep("Read failed, error: " + getTLSErrorString(tls_context));

		return (size_t)num_bytes_read;
	}
}


void TLSSocket::setNoDelayEnabled(bool enabled) // NoDelay option is off by default.
{
	plain_socket->setNoDelayEnabled(enabled);
}


void TLSSocket::enableTCPKeepAlive(float period)
{
	plain_socket->enableTCPKeepAlive(period);
}


void TLSSocket::setAddressReuseEnabled(bool enabled)
{
	plain_socket->setAddressReuseEnabled(enabled);
}


void TLSSocket::setTimeout(double s)
{
	plain_socket->setTimeout(s);
}


void TLSSocket::readTo(void* buffer, size_t readlen)
{
	readTo(buffer, readlen, NULL);
}


void TLSSocket::readTo(void* buffer, size_t readlen, FractionListener* frac)
{
	const size_t total_num_bytes_to_read = readlen;

	while(readlen > 0) // While still bytes to read
	{
		const size_t num_bytes_to_read = std::min(MAX_READ_OR_WRITE_SIZE, readlen);
		assert(num_bytes_to_read > 0);

		//------------------------------------------------------------------------
		//do the read
		//------------------------------------------------------------------------
		const ssize_t num_bytes_read = tls_read(tls_context, buffer, num_bytes_to_read);
		if(num_bytes_read == TLS_WANT_POLLIN || num_bytes_read == TLS_WANT_POLLOUT)
			continue;
		else if(num_bytes_read == SOCKET_ERROR) // Connection was reset/broken
			throw MySocketExcep("Read failed, error: " + getTLSErrorString(tls_context));
		else if(num_bytes_read == 0) // Connection was closed gracefully
			throw MySocketExcep("Connection Closed.", MySocketExcep::ExcepType_ConnectionClosedGracefully);

		assert(num_bytes_read <= (ssize_t)num_bytes_to_read);
		readlen -= num_bytes_read;
		buffer = (void*)((uint8*)buffer + num_bytes_read);

		if(frac)
			frac->setFraction((float)(total_num_bytes_to_read - readlen) / (float)total_num_bytes_to_read);
	}
}


void TLSSocket::ungracefulShutdown()
{
	plain_socket->ungracefulShutdown();
}


void TLSSocket::waitForGracefulDisconnect()
{
	// conPrint("---------waitForGracefulDisconnect-------");
	while(1)
	{
		char buf[1024];
		const ssize_t num_bytes_read = tls_read(tls_context, buf, sizeof(buf));
		if(num_bytes_read == TLS_WANT_POLLIN || num_bytes_read == TLS_WANT_POLLOUT)
			continue;
		else if(num_bytes_read == SOCKET_ERROR) // Connection was reset/broken
			throw MySocketExcep("Read failed, error: " + getTLSErrorString(tls_context));
		else if(num_bytes_read == 0) // Connection was closed gracefully
		{
			// conPrint("\tConnection was closed gracefully.");
			return;
		}
	}
}


void TLSSocket::startGracefulShutdown()
{
	plain_socket->startGracefulShutdown();
}


void TLSSocket::writeInt32(int32 x)
{
	if(plain_socket->getUseNetworkByteOrder())
		x = bitCast<int32>(htonl(bitCast<uint32>(x)));

	write(&x, sizeof(int32));
}


void TLSSocket::writeUInt32(uint32 x)
{
	if(plain_socket->getUseNetworkByteOrder())
		x = htonl(x); // Convert to network byte ordering.

	write(&x, sizeof(uint32));
}


int TLSSocket::readInt32()
{
	int32 i;
	readTo(&i, sizeof(int32));

	if(plain_socket->getUseNetworkByteOrder())
		i = bitCast<int32>(ntohl(bitCast<uint32>(i)));

	return i;
}


uint32 TLSSocket::readUInt32()
{
	uint32 x;
	readTo(&x, sizeof(uint32));

	if(plain_socket->getUseNetworkByteOrder())
		x = ntohl(x);

	return x;
}


bool TLSSocket::readable(double timeout_s)
{
	// "SSL_pending() returns the number of bytes which have been processed, buffered and are available inside ssl for immediate read." - https://www.openssl.org/docs/man1.1.0/man3/SSL_pending.html
	if(SSL_pending(((glare_tls*)tls_context)->ssl_conn) > 0)
		return true;
	return plain_socket->readable(timeout_s);
}


// Block until either the socket is readable or the event_fd is signalled (becomes readable). 
// Returns true if the socket was readable, false if the event_fd was signalled.
bool TLSSocket::readable(EventFD& event_fd)
{	
	if(SSL_pending(((glare_tls*)tls_context)->ssl_conn) > 0)
		return true;
	return plain_socket->readable(event_fd);
}


void TLSSocket::readData(void* buf, size_t num_bytes)
{
	readTo(buf, num_bytes, 
		NULL // fraction listener
	);
}


bool TLSSocket::endOfStream()
{
	return false;
}


void TLSSocket::writeData(const void* data, size_t num_bytes)
{
	write(data, num_bytes, 
		NULL // fraction listener
	);
}


void TLSSocket::initFDSetWithSocket(fd_set& sockset, SOCKETHANDLE_TYPE& sockhandle)
{
	FD_ZERO(&sockset);
	FD_SET(sockhandle, &sockset);
}


#endif // USING_LIBRESSL
