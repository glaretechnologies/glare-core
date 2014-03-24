/*=====================================================================
ipaddress.h
-----------
Copyright Glare Technologies Limited 2013 -
File created by ClassTemplate on Mon Mar 04 05:05:01 2002
=====================================================================*/
#pragma once


#include "../utils/Platform.h"
#include <string>
struct sockaddr;


class MalformedIPStringExcep
{
public:
	MalformedIPStringExcep(const std::string& message_) : message(message_) {}
	const std::string& what() const { return message; }
private:
	std::string message;
};


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
	explicit IPAddress(const std::string& address); // throws MalformedIPStringExcep on failure

	// Is this an IPv4 or IPv6 address?
	Version getVersion() const { return version; }

	// Fill out a sockets API address structure.  Takes a port argument for the structure as well.
	void fillOutSockAddr(sockaddr& sock_addr, int port) const;

	// Return as a string like "127.0.0.1" or "::1".
	const std::string toString() const;

	static void test();
private:
	uint8 address[16];
	Version version;
};


IPAddress::IPAddress()
:	version(Version_4)
{
}
