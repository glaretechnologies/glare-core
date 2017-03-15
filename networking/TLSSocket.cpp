/*=====================================================================
TLSSocket.cpp
-------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "TLSSocket.h"


#if USING_LIBRESSL


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
#include <tls.h>
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
typedef int SOCKLEN_TYPE;
#else
typedef socklen_t SOCKLEN_TYPE;

static const int SOCKET_ERROR = -1;
#endif


TLSSocket::TLSSocket(MySocketRef plain_socket_, tls_config* client_tls_config, const std::string& servername)
{
	init();

	plain_socket = plain_socket_;

	tls_context = tls_client();
	if(!tls_context)
		throw MySocketExcep("Failed to create tls_context.");

	if(tls_configure(tls_context, client_tls_config) != 0)
		throw MySocketExcep("tls_configure failed: " + std::string(tls_error(tls_context)));

	if(tls_connect_socket(tls_context, (int)plain_socket->getSocketHandle(), servername.c_str()) != 0)
		throw MySocketExcep("tls_connect_socket failed: " + std::string(tls_error(tls_context)));

	//Timer timer;
	if(tls_handshake(tls_context) != 0) //TEMP NEW
		throw MySocketExcep("tls_handshake failed: " + std::string(tls_error(tls_context)));
	//conPrint("Client TLS handshake took " + timer.elapsedStringNSigFigs(4));

	//conPrint("tls_conn_cipher: " + std::string(tls_conn_cipher(tls_context)));
	//conPrint("tls_conn_version: " + std::string(tls_conn_version(tls_context)));
}


TLSSocket::TLSSocket(MySocketRef plain_socket_, struct tls* tls_context_)
{
	init();

	plain_socket = plain_socket_;

	tls_context = tls_context_;

	//Timer timer;
	//if(tls_handshake(tls_context) != 0) //TEMP NEW
	//	throw MySocketExcep("tls_handshake failed: " + std::string(tls_error(tls_context)));
	//conPrint("Server TLS handshake took " + timer.elapsedStringNSigFigs(4));
}


void TLSSocket::init()
{
	// Due to a bug with Windows XP, we can't use a large buffer size for reading to and writing from the socket.
	// See http://support.microsoft.com/kb/201213 for more details on the bug.
	this->max_buffersize = PlatformUtils::isWindowsXPOrEarlier() ? 1024 : (1024 * 1024 * 8);
}


TLSSocket::~TLSSocket()
{
	plain_socket->shutdown();

	tls_close(tls_context);
	tls_free(tls_context);
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
	const size_t totalnumbytestowrite = datalen;

	while(datalen > 0) // while still bytes to write
	{
		const int numbytestowrite = (int)std::min(this->max_buffersize, datalen);
		assert(numbytestowrite > 0);

		const int numbyteswritten = (int)tls_write(tls_context, (const void*)data, numbytestowrite);
		if(numbyteswritten == TLS_WANT_POLLIN || numbyteswritten == TLS_WANT_POLLOUT)
			continue;

		if(numbyteswritten == SOCKET_ERROR)
			throw MySocketExcep("write failed: " + std::string(tls_error(tls_context)));

		datalen -= numbyteswritten;
		data = (void*)((char*)data + numbyteswritten); // Advance data pointer

		if(frac)
			frac->setFraction((float)(totalnumbytestowrite - datalen) / (float)totalnumbytestowrite);
	}
}


size_t TLSSocket::readSomeBytes(void* buffer, size_t max_num_bytes)
{
	while(1)
	{
		const ssize_t numbytesread = tls_read(tls_context, buffer, max_num_bytes);
		if(numbytesread == TLS_WANT_POLLIN || numbytesread == TLS_WANT_POLLOUT)
			continue;

		if(numbytesread == SOCKET_ERROR) // Connection was reset/broken
			throw MySocketExcep("Read failed, error: " + std::string(tls_error(tls_context)));

		return (size_t)numbytesread;
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


void TLSSocket::readTo(void* buffer, size_t readlen)
{
	readTo(buffer, readlen, NULL);
}


void TLSSocket::readTo(void* buffer, size_t readlen, FractionListener* frac)
{
	const size_t totalnumbytestoread = readlen;

	while(readlen > 0) // While still bytes to read
	{
		const int numbytestoread = (int)std::min(this->max_buffersize, readlen);
		assert(numbytestoread > 0);

		//------------------------------------------------------------------------
		//do the read
		//------------------------------------------------------------------------
		const int numbytesread = (int)tls_read(tls_context, buffer, numbytestoread);
		if(numbytesread == TLS_WANT_POLLIN || numbytesread == TLS_WANT_POLLOUT)
			continue;

		if(numbytesread == SOCKET_ERROR) // Connection was reset/broken
			throw MySocketExcep("Read failed, error: " + std::string(tls_error(tls_context)));
		else if(numbytesread == 0) // Connection was closed gracefully
			throw MySocketExcep("Connection Closed.");

		readlen -= numbytesread;
		buffer = (void*)((char*)buffer + numbytesread);

		if(frac)
			frac->setFraction((float)(totalnumbytestoread - readlen) / (float)totalnumbytestoread);
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
		const int numbytesread = (int)tls_read(tls_context, buf, sizeof(buf));
		if(numbytesread == TLS_WANT_POLLIN || numbytesread == TLS_WANT_POLLOUT)
			continue;

		if(numbytesread == SOCKET_ERROR) // Connection was reset/broken
			throw MySocketExcep("Read failed, error: " + std::string(tls_error(tls_context)));
		else if(numbytesread == 0) // Connection was closed gracefully
		{
			// conPrint("\tConnection was closed gracefully.");
			return;
		}
	}
}


void TLSSocket::writeInt32(int32 x)
{
	const uint32 i = htonl(bitCast<uint32>(x));
	write(&i, sizeof(uint32));
}


void TLSSocket::writeUInt32(uint32 x)
{
	const uint32 i = htonl(x); // Convert to network byte ordering.
	write(&i, sizeof(uint32));
}


int TLSSocket::readInt32()
{
	uint32 i;
	readTo(&i, sizeof(uint32));
	i = ntohl(i);
	return bitCast<int32>(i);
}


uint32 TLSSocket::readUInt32()
{
	uint32 x;
	readTo(&x, sizeof(uint32));
	return ntohl(x);
}


bool TLSSocket::readable(double timeout_s)
{
	return plain_socket->readable(timeout_s); // TEMP HACK

	/*assert(timeout_s < 1.0);

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

	return num > 0;*/
}


// Block until either the socket is readable or the event_fd is signalled (becomes readable). 
// Returns true if the socket was readable, false if the event_fd was signalled.
bool TLSSocket::readable(EventFD& event_fd)
{	
	return plain_socket->readable(event_fd); // TEMP HACK

#if 0
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
#endif
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
