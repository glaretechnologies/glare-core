/*=====================================================================
Networking.cpp
--------------
Copyright Glare Technologies Limited 2013 -
File created by ClassTemplate on Thu Apr 18 15:19:22 2002
=====================================================================*/
#include "Networking.h"


#include "../utils/StringUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/ConPrint.h"
#include <cstring>
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <netdb.h> // getaddrinfo
#include <string.h> // strerror_r
#include <errno.h>
#include <unistd.h> // gethostname
#endif


Networking::Networking()
{
	//-----------------------------------------------------------------
	//do windows sockets startup
	//-----------------------------------------------------------------
#if defined(_WIN32)
	WSADATA wsaData;
	if(WSAStartup(
		0x0101, // version requested - program requires Winsock V1.1
		&wsaData) != 0)
	{
		WSACleanup();
		
		throw NetworkingExcep("unable to start WinSock");
	}
#endif
}


Networking::~Networking()
{
	//-----------------------------------------------------------------
	//close down windows sockets
	//-----------------------------------------------------------------
#if defined(_WIN32)
	const int result = WSACleanup();
	assertOrDeclareUsed(result == 0);
#endif
}


int Networking::getPortFromSockAddr(const sockaddr& sock_addr)
{
	if(sock_addr.sa_family == AF_INET)
		return ntohs(((struct sockaddr_in*)&sock_addr)->sin_port);
	else
		return ntohs(((struct sockaddr_in6*)&sock_addr)->sin6_port);
}


const std::string Networking::getError()
{
	// Note that in practice, WSAGetLastError seems to be just an alias for GetLastError: http://stackoverflow.com/questions/15586224/is-wsagetlasterror-just-an-alias-for-getlasterror
	return PlatformUtils::getLastErrorString();

	/*
	if(errno == EADDRINUSE)
		return "Address in use";
	else if(errno == EADDRNOTAVAIL)
		return "Address not available";
	else if(errno == EAGAIN)
		return "Resource unavailable, try again";
	else if(errno == EALREADY)
		return "Connection already in progress.";
	else if(errno == ECANCELED)
		return "Operation canceled.";
	else if(errno == ECONNABORTED)
		return "Connection aborted.";
	else if(errno == ECONNREFUSED)
		return "Connection refused.";
	else if(errno == ECONNRESET)
		return "Connection reset.";
	else if(errno == EHOSTUNREACH)
		return "Host is unreachable.";
	else if(errno == EINPROGRESS)
		return "Operation in progress.";
	else if(errno == EINVAL)
		return "Invalid argument.";
	else if(errno == EISCONN)
		return "Socket is connected.";
	else if(errno == ENETRESET)
		return "Connection aborted by network.";
	else if(errno == ENETUNREACH)
		return "Network unreachable.";
	else if(errno == ENOTCONN)
		return "The socket is not connected.";
	else if(errno == EOPNOTSUPP)
		return "Operation not supported on socket.";
	else if(errno == ETIME)
		return "Stream timeout.";
	else if(errno == ETIMEDOUT)
		return "Connection timed out.";
	else if(errno == EWOULDBLOCK)
		return "Operation would block";
	else
		return "[unknown: errno=" + ::toString(errno) + "]";*/
}


const std::vector<IPAddress> Networking::doDNSLookup(const std::string& hostname)
{
	addrinfo hints;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	hints.ai_protocol = IPPROTO_TCP;

	addrinfo *result = NULL;

	const int ret = ::getaddrinfo(
		hostname.c_str(), // node name
		NULL, // service name. From http://linux.die.net/man/3/getaddrinfo: "If service is NULL, then the port number of the returned socket addresses will be left uninitialized."
		&hints, // hints
		&result
	);

	if(ret != 0)
#if defined(_WIN32)
		throw NetworkingExcep("Failed to resolve hostname '" + hostname + "': " + PlatformUtils::getErrorStringForCode(ret));
#else
		throw NetworkingExcep("Failed to resolve hostname '" + hostname + "': " + ::gai_strerror(ret)); 
		// The gai_strerror() function returns an error message string corresponding to the error code returned by getaddrinfo(3)
#endif

	std::vector<IPAddress> host_ip_list;
	for(addrinfo* info = result; info != NULL; info = info->ai_next)
		host_ip_list.push_back(IPAddress(*info->ai_addr));

	freeaddrinfo(result); // free the linked list

	return host_ip_list;
}


//const std::string Networking::doReverseDNSLookup(const IPAddress& ipaddress)
//{
//	assert(isInited());
//
//	const unsigned int addr = ipaddress.getIPv4Addr();
//
//	struct hostent *hostaddr;
//	
//	hostaddr = ::gethostbyaddr((const char*)&addr, sizeof(addr), AF_INET);
//
//
//	if(!hostaddr)
//		throw NetworkingExcep("gethostbyaddr failed");
//
//	return hostaddr->h_name;
//}


const std::string Networking::getHostName()
{
	char buf[256];
	const int result = ::gethostname(buf, sizeof(buf));
	if(result != 0)
		throw NetworkingExcep("gethostname failed: " + getError());
	return std::string(buf);
}
