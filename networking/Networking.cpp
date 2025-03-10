/*=====================================================================
Networking.cpp
--------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#include "Networking.h"


#include "../utils/StringUtils.h"
#include "../utils/PlatformUtils.h"
#include "../utils/ConPrint.h"
#include <cstring>
#include <cassert>
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


static bool initialised = false;


void Networking::init()
{
	assert(!initialised);

	//-----------------------------------------------------------------
	// Do windows sockets startup
	//-----------------------------------------------------------------
#if defined(_WIN32)
	WSADATA wsaData;
	if(WSAStartup(
		MAKEWORD(2, 2), // version requested - Use 2.2
		&wsaData) != 0)
	{
		throw NetworkingExcep("Unable to start WinSock");
	}
#endif

	initialised = true;
}


void Networking::shutdown()
{
	assert(initialised);

	//-----------------------------------------------------------------
	// Close down windows sockets
	//-----------------------------------------------------------------
#if defined(_WIN32)
	const int result = WSACleanup();
	assertOrDeclareUsed(result == 0);
#endif

	initialised = false;
}


bool Networking::isInitialised()
{
	return initialised;
}


int Networking::getPortFromSockAddr(const sockaddr& sock_addr)
{
	if(sock_addr.sa_family == AF_INET)
		return ntohs(((struct sockaddr_in*)&sock_addr)->sin_port);
	else
		return ntohs(((struct sockaddr_in6*)&sock_addr)->sin6_port);
}


int Networking::getPortFromSockAddr(const sockaddr_storage& sock_addr)
{
	if(((struct sockaddr_in*)&sock_addr)->sin_family == AF_INET)
		return ntohs(((struct sockaddr_in*)&sock_addr)->sin_port);
	else
		return ntohs(((struct sockaddr_in6*)&sock_addr)->sin6_port);
}


const std::string Networking::getError()
{
	// Note that in practice, WSAGetLastError seems to be just an alias for GetLastError: http://stackoverflow.com/questions/15586224/is-wsagetlasterror-just-an-alias-for-getlasterror
	return PlatformUtils::getLastErrorString();
}


const std::vector<IPAddress> Networking::doDNSLookup(const std::string& hostname)
{
	addrinfo hints;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; // don't care IPv4 or IPv6
	hints.ai_socktype = SOCK_STREAM; // TCP stream sockets
	hints.ai_protocol = IPPROTO_TCP;
#if !defined(_WIN32) 
	hints.ai_flags |= AI_ADDRCONFIG; // On Linux, some users were having issues with getaddrinfo returning IPv6 addresses which were not valid (IPv6 support was disabled).
	// AI_ADDRCONFIG will hopefully prevent invalid IPv6 addresses being returned by getaddrinfo.
#endif

	addrinfo* result = NULL;

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

	if(host_ip_list.empty())
		throw NetworkingExcep("Failed to resolve hostname '" + hostname + "'");

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
