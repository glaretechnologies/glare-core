/*=====================================================================
IPAddress.cpp
-------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "IPAddress.h"


#include "Networking.h"
#include "../utils/StringUtils.h"
#include "../utils/Parser.h"
#include "../utils/PlatformUtils.h"
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


void IPAddress::fillOutSockAddr(sockaddr_storage& sock_addr, int port) const
{
	memset(&sock_addr, 0, sizeof(sockaddr_storage)); // Clear struct.

	if(getVersion() == IPAddress::Version_4)
	{
		sockaddr_in* ipv4_address = (sockaddr_in*)&sock_addr;
		ipv4_address->sin_family = AF_INET;
		ipv4_address->sin_port = htons((unsigned short)port);
		std::memcpy(&ipv4_address->sin_addr, this->address, 4);
	}
	else
	{
		sockaddr_in6 *ipv6_address = (sockaddr_in6*)&sock_addr;
		ipv6_address->sin6_family = AF_INET6;
		ipv6_address->sin6_port = htons((unsigned short)port);
		std::memcpy(&ipv6_address->sin6_addr, this->address, 16);
	}
}


void IPAddress::fillOutIPV6SockAddr(sockaddr_storage& sock_addr, int port) const
{
	memset(&sock_addr, 0, sizeof(sockaddr_storage)); // Clear struct.

	sockaddr_in6* ipv6_address = (sockaddr_in6*)&sock_addr;
	ipv6_address->sin6_family = AF_INET6;
	ipv6_address->sin6_port = htons((unsigned short)port);

	if(getVersion() == IPAddress::Version_4)
	{
		// See section "IP Addresses with a Dual-Stack Socket", from Dual-Stack Sockets for IPv6 Winsock Applications
		// http://msdn.microsoft.com/en-us/library/windows/desktop/bb513665(v=vs.85).aspx
		const uint8 FF = 0xFFu;
		std::memcpy((uint8*)&ipv6_address->sin6_addr + 10, &FF, 1);
		std::memcpy((uint8*)&ipv6_address->sin6_addr + 11, &FF, 1);
		std::memcpy((uint8*)&ipv6_address->sin6_addr + 12, this->address, 4);
	}
	else
	{
		std::memcpy(&ipv6_address->sin6_addr, this->address, 16);
	}
}


IPAddress::IPAddress(const std::string& addr_string)
{
	// If the string contains at least one ':', assume it is an IPv6 address.
	this->version = (addr_string.find(':') == std::string::npos) ? Version_4 : Version_6;

	const int family = (this->version == Version_4) ? AF_INET : AF_INET6;

#if defined(_WIN32)
	// We'll use the ASCII form of inet_pton (as opposed to the wide-string InetPton), 
	// since IP addresses shouldn't have unicode chars in them.
	const int result = inet_pton(
		family,
		addr_string.c_str(),
		this->address
	);
	if(result != 1)
	{
		if(result == 0)
			throw NetworkingExcep("Invalid IP address string '" + addr_string + "'");
		else
			throw NetworkingExcep("inet_pton failed: " + Networking::getError());
	}
#else
	const int result = inet_pton(
		family,
		addr_string.c_str(),
		this->address
	);

	if(result != 1)
		throw NetworkingExcep("Failed to parse IP address string '" + addr_string + "'");
#endif
}


const std::string IPAddress::toString() const
{
	const int family = (version == Version_4) ? AF_INET : AF_INET6;
	
#if defined(_WIN32)
	char buf[INET6_ADDRSTRLEN];

	// We'll use the ASCII form of inet_ntop (as opposed to the wide-string InetNtop), 
	// since IP addresses shouldn't have unicode chars in them.
	const PCSTR res = inet_ntop(
		family,
		(void*)address,
		buf,
		INET6_ADDRSTRLEN
	);
	if(res == NULL)
		throw NetworkingExcep("inet_ntop failed: " + Networking::getError());
	return std::string(buf);
#else
	char ipstr[INET6_ADDRSTRLEN];

	const char* result = inet_ntop(
		family,
		(void*)address,
		ipstr,
		INET6_ADDRSTRLEN
	);

	if(result != NULL)
		return std::string(result);
	else
		throw NetworkingExcep("inet_ntop failed: " + PlatformUtils::getLastErrorString());
#endif
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

		string_view token;
		if(!parser.parseToChar(']', token))
			return; // Parse error.
		hostname_or_ip_out = token.to_string();

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
		string_view token;
		parser.parseToCharOrEOF(':', token);
		hostname_or_ip_out = token.to_string();

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
	catch(NetworkingExcep&)
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
	catch(NetworkingExcep& e)
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
	catch(NetworkingExcep& e)
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
