/*=====================================================================
ipaddress.cpp
-------------
Copyright Glare Technologies Limited 2013 -
File created by ClassTemplate on Mon Mar 04 05:05:01 2002
=====================================================================*/
#include "ipaddress.h"


#include "../utils/StringUtils.h"
#include "../utils/Parser.h"
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
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


// Since XP doesn't support inet_ntop, we'll provide our own implementation here
// inet_ntop - convert IPv4 and IPv6 addresses from binary to text form
static const char* glare_inet_ntop(int family, const void* src, char* dst, socklen_t size)
{
#if defined(_WIN32)
	struct sockaddr_storage addr;
	memset(&addr, 0, sizeof(struct sockaddr_storage));
	
	addr.ss_family = (ADDRESS_FAMILY)family;
	if(family == AF_INET)
	{
		std::memcpy(&((struct sockaddr_in *)&addr)->sin_addr , src, sizeof(struct in_addr));
    }
	else if(family == AF_INET6)
	{
		std::memcpy(&((struct sockaddr_in6*)&addr)->sin6_addr, src, sizeof(struct in6_addr));
    }

	const int addr_len = sizeof(addr);


	wchar_t buf[INET6_ADDRSTRLEN];
	DWORD buflen = INET6_ADDRSTRLEN;

	if(WSAAddressToString(
		(struct sockaddr*)&addr,
		addr_len,
		NULL, // lpProtocolInfo
		buf, // lpszAddressString
		&buflen // lpdwAddressStringLength
	) != 0)
		return NULL;

	// Convert to UTF-8
	const std::string s = StringUtils::PlatformToUTF8UnicodeEncoding(std::wstring(buf));

	// Copy to output buffer (dst).  Copy null terminator as well.
	if(s.length() + 1 <= size)
		std::memcpy(dst, s.c_str(), s.length() + 1);
	else
		return NULL;

    return dst;

#else // #else if !defined(_WIN32):
	
	return inet_ntop(
		family,
		src,
		dst,
		size
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
	
	const char* result = glare_inet_ntop(family, (void*)address, ipstr, sizeof(ipstr));

	if(result != NULL)
		return std::string(result);
	else
		return "[Error]";
}


const std::string IPAddress::formatIPAddressAndPort(const IPAddress& ipaddress, int port)
{
	if(ipaddress.getVersion() == IPAddress::Version_6)
	{
		// This is an IPv6 address, so enclose in square brackets:
		return "[" + ipaddress.toString() + "]:" + ::toString(port);
	}
	else
	{
		return ipaddress.toString() + ":" + ::toString(port);
	}
}


const std::string IPAddress::formatIPAddressAndPort(const std::string& ipaddress, int port)
{
	if(ipaddress.find_first_of(':') != std::string::npos)
	{
		// This is an IPv6 address, so enclose in square brackets:
		return "[" + ipaddress + "]:" + ::toString(port);
	}
	else
	{
		return ipaddress + ":" + ::toString(port);
	}
}


// Sets port_out to zero if not present
void IPAddress::parseIPAddrOrHostnameAndOptionalPort(const std::string& s, std::string& hostname_or_ip_out, int& port_out)
{
	hostname_or_ip_out = "";
	port_out = 0;

	Parser parser(s.c_str(), (unsigned int)s.size());

	if(parser.currentIsChar('['))
	{
		// This is an IPv6 address with a port, like '[1fff:0:a88:85a3::ac1f]:8001'

		parser.advance();

		if(!parser.parseToChar(']', hostname_or_ip_out))
			return; // Parse error.

		if(!parser.parseChar(']'))
			return; // Parse error.

		if(parser.currentIsChar(':')) // If we have a port suffix:
		{
			parser.advance();
			
			if(!parser.parseInt(port_out))
				return; // Parse error.
		}
	}
	else if(::getNumMatches(s, ':') > 1)
	{
		// This is an IPv6 address without the port, e.g. '1fff:0:a88:85a3::ac1f'.
		hostname_or_ip_out = s;
	}
	else
	{
		Parser parser(s.c_str(), (unsigned int)s.size());

		parser.parseToCharOrEOF(':', hostname_or_ip_out);

		if(parser.currentIsChar(':')) // If we have a port suffix:
		{
			parser.advance();
			
			if(!parser.parseInt(port_out))
				return; // Parse error.
		}
	}
}


// Parse IP address or hostname and optional port.  If port is present, return it (in port_out), otherwise return the default port
void IPAddress::parseIPAddrOrHostnameWithDefaultPort(const std::string& s, int default_port, std::string& hostname_or_ip_out, int& port_out)
{
	int parsed_port;
	parseIPAddrOrHostnameAndOptionalPort(s, hostname_or_ip_out, parsed_port);
	if(parsed_port == 0) // If there was no port present:
		port_out = default_port;
	else
		port_out = parsed_port;
}



// Append the port to he IP Address or hostname string, if a port is not already present.
const std::string IPAddress::appendPortToAddrOrHostname(const std::string& s, int port)
{
	std::string hostname;
	int existing_port;
	parseIPAddrOrHostnameAndOptionalPort(s, hostname, existing_port);
	
	if(existing_port == 0)
	{
		// If there was no port:
		return formatIPAddressAndPort(s, port);
	}
	else
	{
		return s;
	}
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

		{
		IPAddress a("2001:1db8:85a3:1234:5678:8a2e:1370:7334");
		testAssert(a.getVersion() == IPAddress::Version_6);
		testAssert(a.toString() == "2001:1db8:85a3:1234:5678:8a2e:1370:7334");
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


	// ===================== Test parseIPAddrOrHostnameAndPort =========================

	// Test for IPv6 addresses:
	{
		std::string ip;
		int port;
		parseIPAddrOrHostnameAndOptionalPort("[1fff:0:a88:85a3::ac1f]:8001", ip, port);
		testAssert(ip == "1fff:0:a88:85a3::ac1f" && port == 8001);
	}
	{
		std::string ip;
		int port;
		parseIPAddrOrHostnameAndOptionalPort("[1fff:0:a88:85a3::ac1f]", ip, port);
		testAssert(ip == "1fff:0:a88:85a3::ac1f" && port == 0);
	}
	{
		std::string ip;
		int port;
		parseIPAddrOrHostnameAndOptionalPort("1fff:0:a88:85a3::ac1f", ip, port);
		testAssert(ip == "1fff:0:a88:85a3::ac1f" && port == 0);
	}

	// Test for IPv4 addresses:
	{
		std::string ip;
		int port;
		parseIPAddrOrHostnameAndOptionalPort("192.168.1.1:8001", ip, port);
		testAssert(ip == "192.168.1.1" && port == 8001);
	}
	{
		std::string ip;
		int port;
		parseIPAddrOrHostnameAndOptionalPort("192.168.1.1", ip, port);
		testAssert(ip == "192.168.1.1" && port == 0);
	}

	// Test for hostname and port:
	{
		std::string ip;
		int port;
		parseIPAddrOrHostnameAndOptionalPort("localhost:8001", ip, port);
		testAssert(ip == "localhost" && port == 8001);
	}
	{
		std::string ip;
		int port;
		parseIPAddrOrHostnameAndOptionalPort("localhost", ip, port);
		testAssert(ip == "localhost" && port == 0);
	}

	//==================== Test parseIPAddrOrHostnameWithDefaultPort() ====================
	{
		std::string ip;
		int port;
		parseIPAddrOrHostnameWithDefaultPort("1fff:0:a88:85a3::ac1f", 1000, ip, port);
		testAssert(ip == "1fff:0:a88:85a3::ac1f" && port == 1000);
	}

	{
		std::string ip;
		int port;
		parseIPAddrOrHostnameWithDefaultPort("localhost", 1000, ip, port);
		testAssert(ip == "localhost" && port == 1000);
	}


	{
		std::string ip;
		int port;
		parseIPAddrOrHostnameWithDefaultPort("[1fff:0:a88:85a3::ac1f]:8000", 1000, ip, port);
		testAssert(ip == "1fff:0:a88:85a3::ac1f" && port == 8000);
	}

	{
		std::string ip;
		int port;
		parseIPAddrOrHostnameWithDefaultPort("localhost:8000", 1000, ip, port);
		testAssert(ip == "localhost" && port == 8000);
	}

	//==================== Test appendPortToAddrOrHostname() =====================
	// Test for port already present.
	{
		testAssert(appendPortToAddrOrHostname("192.168.1.1:80", 1000) == "192.168.1.1:80");
		testAssert(appendPortToAddrOrHostname("[1fff:0:a88:85a3::ac1f]:80", 1000) == "[1fff:0:a88:85a3::ac1f]:80");
		testAssert(appendPortToAddrOrHostname("localhost:80", 1000) == "localhost:80");
		testAssert(appendPortToAddrOrHostname("www.google.com:80", 1000) == "www.google.com:80");
	}

	// Test when port is not already present
	{
		testAssert(appendPortToAddrOrHostname("192.168.1.1", 1000) == "192.168.1.1:1000");
		testAssert(appendPortToAddrOrHostname("1fff:0:a88:85a3::ac1f", 1000) == "[1fff:0:a88:85a3::ac1f]:1000");
		testAssert(appendPortToAddrOrHostname("localhost", 1000) == "localhost:1000");
		testAssert(appendPortToAddrOrHostname("www.google.com", 1000) == "www.google.com:1000");
	}
}


#endif // BUILD_TESTS
