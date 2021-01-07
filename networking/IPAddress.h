/*=====================================================================
IPAddress.h
-----------
Copyright Glare Technologies Limited 2018 -
File created by ClassTemplate on Mon Mar 04 05:05:01 2002
=====================================================================*/
#pragma once


#include "../utils/Platform.h"
#include <string>
struct sockaddr;
struct sockaddr_storage;


/*=====================================================================
IPAddress
---------
Represents an IPv4 or IPv6 address
=====================================================================*/
class IPAddress
{
public:
	enum Version
	{
		Version_4,
		Version_6
	};

	inline IPAddress();

	// Construct from a sockets API address
	explicit IPAddress(const sockaddr& sock_addr);

	// Construct from a string
	// Can be an IPv4 string like "127.0.0.1", or an IPv6 string like "::1".
	explicit IPAddress(const std::string& address); // throws NetworkingExcep on failure

	// Is this an IPv4 or IPv6 address?
	Version getVersion() const { return version; }

	// Fill out a sockets API address structure.  Takes a port argument for the structure as well.
	void fillOutSockAddr(sockaddr_storage& sock_addr, int port) const;

	// Fill out a sockets API address structure.  Takes a port argument for the structure as well.
	void fillOutIPV6SockAddr(sockaddr_storage& sock_addr, int port) const;

	// Return as a string like "127.0.0.1" or "::1".
	const std::string toString() const;


	// For IPv6, returns a string like [1fff:0:a88:85a3::ac1f]:8001  (see http://stackoverflow.com/questions/186829/ipv6-and-ports)
	// For IPv4, returns a string like 192.168.1.1:8001
	static const std::string formatIPAddressAndPort(const IPAddress& ipaddress, int port);
	static const std::string formatIPAddressAndPort(const std::string& ipaddress, int port);

	// Sets port_out to zero if not present
	static void parseIPAddrOrHostnameAndOptionalPort(const std::string& s, std::string& hostname_or_ip_out, int& port_out);

	// Parse IP address or hostname and optional port.  If port is present, return it (in port_out), otherwise return the default port
	static void parseIPAddrOrHostnameWithDefaultPort(const std::string& s, int default_port, std::string& hostname_or_ip_out, int& port_out);

	// Append the port to he IP Address or hostname string, if a port is not already present.
	static const std::string appendPortToAddrOrHostname(const std::string& s, int port);

	static void test();
private:
	uint8 address[16];
	Version version;
};


IPAddress::IPAddress()
:	version(Version_4)
{
}
