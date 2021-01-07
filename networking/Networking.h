/*=====================================================================
Networking.h
------------
Copyright Glare Technologies Limited 2013 -
File created by ClassTemplate on Thu Apr 18 15:19:22 2002
=====================================================================*/
#pragma once


#include "IPAddress.h"
#include "../utils/Singleton.h"
#include <vector>
#include <string>


class NetworkingExcep
{
public:
	NetworkingExcep(const std::string& message_) : message(message_) {}
	const std::string& what() const { return message; }
private:
	std::string message;
};


/*=====================================================================
Networking
----------
Networking subsystem.
Only one of these will be instantiated per process.
This class is in charge of starting up the winsock stuff and closing it down.
=====================================================================*/
class Networking : public Singleton<Networking>
{
public:
	Networking(); // throws NetworkingExcep

	~Networking();


	static int getPortFromSockAddr(const sockaddr& sock_addr);

	const std::vector<IPAddress> doDNSLookup(const std::string& hostname); // throws NetworkingExcep;
	
	//const std::string doReverseDNSLookup(const IPAddress& ipaddr); // throws NetworkingExcep

	static const std::string getHostName(); // throws NetworkingExcep


	static inline bool isInited(){ return !isNull(); }

	// Returns descriptive string of last (WSA) networking error
	static const std::string getError();
};
