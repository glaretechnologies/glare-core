/*=====================================================================
networking.cpp
--------------
File created by ClassTemplate on Thu Apr 18 15:19:22 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "networking.h"


#if defined(WIN32) || defined(WIN64)
#include <winsock.h>
#else
//#include <net/inet.h>
#include <netinet/in.h>
#include <netdb.h>//gethostbyname
#include <string.h>//strerror_r
#include <errno.h>
#endif

//#include "../cyberspace/globals.h"
#include "../utils/stringutils.h"


#if defined(WIN32) || defined(WIN64)
#else
const int SOCKET_ERROR = -1;
#endif


Networking::Networking()
{
	//sockets_shut_down = false;

//	used_ipaddr_determined = false;

	//-----------------------------------------------------------------
	//do windows sockets startup
	//-----------------------------------------------------------------
#if defined(WIN32) || defined(WIN64)
	WSADATA wsaData;
	#define USE_WINSOCK_VERSION 0x0101	// program requires Winsock V1.1
	if(WSAStartup(USE_WINSOCK_VERSION, &wsaData))
	{
		WSACleanup();
		
		throw NetworkingExcep("unable to start WinSock");
	}
#endif

	//-----------------------------------------------------------------
	//work out the IP adress of this computer
	//-----------------------------------------------------------------
	char buffer[1024];
	if(::gethostname(buffer, 1024) == SOCKET_ERROR)
		throw NetworkingExcep("error calling gethostname()");



/*	struct hostent *hostaddr;

	hostaddr = ::gethostbyname(buffer);

	if(!hostaddr)
		throw std::exception("error calling gethostbyname()");


	//this_ipaddr = IPAddress(htonl(*(unsigned int*)hostaddr->h_addr));//NOTE CHECKME

	//-----------------------------------------------------------------
	//get this host's IP addresses
	//-----------------------------------------------------------------
	for(int i=0; hostaddr->h_addr_list[i] != NULL; ++i)
	{
		IPAddress ip(*(unsigned int*)( hostaddr->h_addr_list[i] ));

#ifndef NOT_CYBERSPACE
		debugPrint("host IP " + intToString(i) + ": " + ip.toString());
#endif

		hostips.push_back(ip);
	}*/

	/*TEMPtry
	{
		hostips = doDNSLookup(std::string(buffer));
	}
	catch(std::exception& e)
	{
		throw e;
	}*/

	/*if(hostips.size() == 1)
	{
		used_ipaddr_determined = true;
		used_ipaddr = hostips[0];
	}*/

	//debugPrint("guessing this host IP as: " + this_ipaddr.toString());

	//char* s = inet_ntoa((*((struct in_addr *)hostaddr->h_addr)));

}


Networking::~Networking()
{
	//-----------------------------------------------------------------
	//close down windows sockets
	//-----------------------------------------------------------------
#if defined(WIN32) || defined(WIN64)
	WSACleanup();
#endif
}
	
/*const IPAddress& Networking::getUsedIPAddr() const 
{
	if(!used_ipaddr_determined)
		::debugPrint("WARNING: getUsedIPAddr() called when used ip has not yet been determined.");

	return used_ipaddr; 
}

void Networking::setUsedIPAddr(const IPAddress& newip)
{ 
	if(newip == IPAddress("0.0.0.0"))
		return;

	if(used_ipaddr_determined && newip != used_ipaddr)
	{
		::debugPrint("WARNING: used ip address set to different value than before: old: " + 
			used_ipaddr.toString() + ", new: " + newip.toString());
	}

	if(!used_ipaddr_determined)
		::debugPrint("This IP determined to be " + newip.toString());


	used_ipaddr = newip; 
	used_ipaddr_determined = true; 
}*/


/*bool Networking::isIPAddrThisHosts(const IPAddress& ip)
{
	assert(isInited());

	for(unsigned int i=0; i<hostips.size(); ++i)
	{
		if(ip == hostips[i])
			return true;
	}
	
	return false;
}*/


const std::string Networking::getError()
{
#if defined(WIN32) || defined(WIN64)
	const int error = WSAGetLastError();

	if(error == WSANOTINITIALISED)
		return "WSANOTINITIALISED";
	else if(error == WSAENETDOWN)
		return "WSAENETDOWN";
	else if(error == WSAEINTR)
		return "WSAEINTR";
	else if(error == WSAEINPROGRESS)
		return "WSAEINPROGRESS";
	else if(error == WSAEINVAL)
		return "WSAEINVAL";
	else if(error == WSAEISCONN)
		return "WSAEISCONN";
	else if(error == WSAENETRESET)
		return "WSAENETRESET";
	else if(error == WSAENOTSOCK)
		return "WSAENOTSOCK";
	else if(error == WSAEOPNOTSUPP)
		return "WSAEOPNOTSUPP";
	else if(error == WSAESHUTDOWN)
		return "WSAESHUTDOWN";
	else if(error == WSAEWOULDBLOCK)
		return "WSAEWOULDBLOCK";
	else if(error == WSAEMSGSIZE)
		return "WSAEMSGSIZE";
	else if(error == WSAETIMEDOUT)
		return "WSAETIMEDOUT";
	else if(error == WSAECONNRESET)
		return "WSAECONNRESET";
	else if(error == WSAEADDRINUSE)
		return "WSAEADDRINUSE";
	else if(error == WSAEADDRNOTAVAIL)
		return "WSAEADDRNOTAVAIL";
	else if(error == WSAEAFNOSUPPORT)
		return "WSAEAFNOSUPPORT";
	else if(error == WSAECONNREFUSED)
		return "WSAECONNREFUSED";
	else if(error == WSAEFAULT)
		return "WSAEFAULT";
	else if(error == WSAENETUNREACH)
		return "WSAENETUNREACH";
	else if(error == WSAENOBUFS)
		return "WSAENOBUFS";
	else if(error == WSAEACCES)
		return "WSAEACCES";
	else
		return "[unknown]";
#else
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
		return "[unknown: errno=" + ::toString(errno) + "]";
	/*char buf[2000];
	//int strerror_r(int errnum, char *buf, size_t n);
	strerror_r(errno, buf, sizeof(buf));

	return std::string(buf);*/

#endif
}

const std::vector<IPAddress> Networking::doDNSLookup(const std::string& hostname) // throw (NetworkingExcep)
{
	//assert(isInited());

	struct hostent *hostaddr;

	hostaddr = ::gethostbyname(hostname.c_str());

	if(!hostaddr)
		throw NetworkingExcep("gethostbyname() failed");

	//-----------------------------------------------------------------
	//get the host's IP addresses
	//-----------------------------------------------------------------
	std::vector<IPAddress> host_ip_list;

	for(int i=0; hostaddr->h_addr_list[i] != NULL; ++i)
	{
		IPAddress ip(*(unsigned int*)( hostaddr->h_addr_list[i] ));

		host_ip_list.push_back(ip);
	}

	return host_ip_list;
}


const std::string Networking::doReverseDNSLookup(const IPAddress& ipaddress) // throw (NetworkingExcep)
{
	assert(isInited());

	const unsigned int addr = ipaddress.getAddr();

	struct hostent *hostaddr;
	
	hostaddr = ::gethostbyaddr((const char*)&addr, sizeof(addr), AF_INET);


	if(!hostaddr)
	{
		throw NetworkingExcep("gethostbyaddr failed");
		return "";
	}

	return hostaddr->h_name;
}

/*void Networking::makeSocketsShutDown()
{
	sockets_shut_down = true;
}
	
bool Networking::shouldSocketsShutDown() const
{
	return sockets_shut_down;
}*/


/*void Networking::createInstance(Networking* newinstance)
{
	assert(instance == NULL);

	instance = newinstance;

}

void Networking::destroyInstance()
{
	delete instance;
	instance = NULL;
}



Networking* Networking::instance = NULL;*/

