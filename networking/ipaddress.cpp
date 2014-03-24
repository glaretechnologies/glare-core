/*=====================================================================
ipaddress.cpp
-------------
Copyright Glare Technologies Limited 2013 -
File created by ClassTemplate on Mon Mar 04 05:05:01 2002
=====================================================================*/
#include "ipaddress.h"


#include "../utils/StringUtils.h"
#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
#include <cstring>


IPAddress::IPAddress(const sockaddr& sock_addr)
{
	if(sock_addr.sa_family == AF_INET)
	{
		// IPv4
		const sockaddr_in *ipv4 = (const sockaddr_in *)&sock_addr;

		this->version = IPAddress::Version_4;
		std::memcpy(address, &ipv4->sin_addr, sizeof(uint32));
	}
	else
	{
		// IPv6
		const sockaddr_in6 *ipv6 = (const sockaddr_in6 *)&sock_addr;

		this->version = IPAddress::Version_6;
		std::memcpy(address, &ipv6->sin6_addr, 16);
	}
}


void IPAddress::fillOutSockAddr(sockaddr& sock_addr, int port) const
{
	memset(&sock_addr, 0, sizeof(sockaddr)); // Clear struct.

	if(getVersion() == IPAddress::Version_4)
	{
		sock_addr.sa_family = AF_INET;

		sockaddr_in *ipv4_address = (sockaddr_in*)&sock_addr;
		
		ipv4_address->sin_port = htons((unsigned short)port);
		std::memcpy(&ipv4_address->sin_addr, this->address, 4);
	}
	else
	{
		sock_addr.sa_family = AF_INET6;

		sockaddr_in6 *ipv6_address = (sockaddr_in6*)&sock_addr;

		ipv6_address->sin6_port = htons((unsigned short)port);
		std::memcpy(&ipv6_address->sin6_addr, this->address, 16);
	}
}


// Since XP doesn't support inet_pton, we'll provide our own implementation here, based on WSAStringToAddress, which is defined in Windows 2000 Professional onwards.
// TODO: Take out when we don't support Windows XP any more.
static int glare_inet_pton(int family, const char *src, void *dst)
{
#if defined(_WIN32)

    struct sockaddr_storage addr;
    addr.ss_family = (ADDRESS_FAMILY)family;

	int addr_len = sizeof(addr);

    const int result = WSAStringToAddressA(
		(char*)src,
		family,
		NULL, // lpProtocolInfo
        (struct sockaddr*)&addr,
		&addr_len
	);
    if(result != 0)
		return 0;

    if(family == AF_INET)
	{
		std::memcpy(dst, &((struct sockaddr_in *) &addr)->sin_addr, sizeof(struct in_addr));
    }
	else if(family == AF_INET6)
	{
		std::memcpy(dst, &((struct sockaddr_in6 *)&addr)->sin6_addr, sizeof(struct in6_addr));
    }

    return 1; // inet_pton() returns 1 on success

#else // #else if !defined(_WIN32):
	
	return inet_pton(
		family,
		src,
		dst
	);

#endif
}


IPAddress::IPAddress(const std::string& addr_string)
{
	if(addr_string.find(':') == std::string::npos)
	{
		// String does not contain any ':'.  Therefore assume it is an IPv4 address.

		this->version = Version_4;

		const int result = glare_inet_pton(
			AF_INET, // Family: IPv4
			addr_string.c_str(),
			this->address
		);

		if(result != 1)
			throw MalformedIPStringExcep("Failed to parse IP address string '" + addr_string + "'");
	}
	else
	{
		// String contains at least one ':'.  Therefore assume it is an IPv6 address.

		this->version = Version_6;

		const int result = glare_inet_pton(
			AF_INET6, // Family: IPv6
			addr_string.c_str(),
			this->address
		);

		if(result != 1)
			throw MalformedIPStringExcep("Failed to parse IP address string '" + addr_string + "'");
	}
}


const std::string IPAddress::toString() const
{
	char ipstr[INET6_ADDRSTRLEN];
	
	const int family = version == Version_4 ? AF_INET : AF_INET6;
	
	inet_ntop(family, (void*)address, ipstr, sizeof(ipstr));

	return std::string(ipstr);
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"


static void testMalformedIPAddress(const std::string& s)
{
	try
	{
		IPAddress a(s);
		failTest("");
	}
	catch(MalformedIPStringExcep&)
	{
	}
}


void IPAddress::test()
{
	// Test an IPv4 address
	try
	{
		IPAddress a("127.0.0.1");
		testAssert(a.getVersion() == IPAddress::Version_4);
		testAssert(a.toString() == "127.0.0.1");
	}
	catch(MalformedIPStringExcep& e)
	{
		failTest(e.what());
	}

	// Apparently Windows thinks '127' is a valid IP address string.  Linux doesn't however.
	/*try
	{
		IPAddress a("127");
		testAssert(a.getVersion() == IPAddress::Version_4);
		testAssert(a.toString() == "0.0.0.127");
	}
	catch(MalformedIPStringExcep& e)
	{
		failTest(e.what());
	}*/

	// Test an IPv6 address
	try
	{
		{
		IPAddress a("::1");
		testAssert(a.getVersion() == IPAddress::Version_6);
		testAssert(a.toString() == "::1");
		}

		{
		IPAddress a("2001:0db8:85a3:0000:0000:8a2e:0370:7334");
		testAssert(a.getVersion() == IPAddress::Version_6);
		testAssert(a.toString() == "2001:db8:85a3::8a2e:370:7334" || a.toString() == "2001:db8:85a3:0:0:8a2e:370:7334" || a.toString() == "2001:0db8:85a3:0000:0000:8a2e:0370:7334" );
		}
	}
	catch(MalformedIPStringExcep& e)
	{
		failTest(e.what());
	}


	// Test some malformed addresses
	testMalformedIPAddress("");
	testMalformedIPAddress("a");
	//testMalformedIPAddress("127");
	//testMalformedIPAddress("127.0");
	//testMalformedIPAddress("127.0.0");
	testMalformedIPAddress("127.0.0.300");
	testMalformedIPAddress("127.0.0.1.1");
}


#endif // BUILD_TESTS
