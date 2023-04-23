/*=====================================================================
Networking.h
------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "IPAddress.h"
#include "../utils/Exception.h"
#include <vector>
#include <string>


class NetworkingExcep : public glare::Exception
{
public:
	NetworkingExcep(const std::string& message_) : glare::Exception(message_) {}
};


/*=====================================================================
Networking
----------
Networking subsystem.
This class is in charge of starting Winsock and closing it down, on Windows.
=====================================================================*/
class Networking
{
public:
	static void init(); // throws NetworkingExcep on failure.
	static void shutdown();
	
	static bool isInitialised();


	static int getPortFromSockAddr(const sockaddr& sock_addr);
	static int getPortFromSockAddr(const sockaddr_storage& sock_addr);

	static const std::vector<IPAddress> doDNSLookup(const std::string& hostname); // throws NetworkingExcep.  Returned vector will have at least one element.
	
	//const std::string doReverseDNSLookup(const IPAddress& ipaddr); // throws NetworkingExcep

	static const std::string getHostName(); // throws NetworkingExcep

	// Returns descriptive string of last (WSA) networking error
	static const std::string getError();
};
