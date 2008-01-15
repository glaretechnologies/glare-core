/*=====================================================================
networking.h
------------
File created by ClassTemplate on Thu Apr 18 15:19:22 2002
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __NETWORKING_H_666_
#define __NETWORKING_H_666_

//#pragma warning(disable : 4786)//disable long debug name warning

#include "ipaddress.h"
#include <vector>
#include <string>
#include "../utils/singleton.h"
//#include <assert.h>


class NetworkingExcep
{
public:
	NetworkingExcep(){}
	NetworkingExcep(const std::string& message_) : message(message_) {}
	~NetworkingExcep(){}

	const std::string& what() const { return message; }
private:
	std::string message;
};




/*=====================================================================
Networking
----------
networking subsystem.
Only one of these will be instantiated per process.
This class is in charge of starting up the winsock stuff and closing it down.
Also in charge of getting the IPs of local interfaces.
=====================================================================*/
class Networking : public Singleton<Networking>
{
public:
	/*=====================================================================
	Networking
	----------
	
	=====================================================================*/
	Networking(); //throws NetworkingExcep

	~Networking();

	//get the ip address of this computer
	//inline const IPAddress& getThisIPAddr() const { return this_ipaddr; }
	//inline void setThisIPAddr(const IPAddress& newip){ this_ipaddr = newip; }

	//warning, may not be determined yet.
	//const IPAddress& getUsedIPAddr() const;

	//void setUsedIPAddr(const IPAddress& newip);
	
	//inline bool isUsedIPDetermined() const { return used_ipaddr_determined; }



	//TEMP bool isIPAddrThisHosts(const IPAddress& ip);
	//bool areEndPointsThisHosts(const IPAddress& ip

	//throws NetworkingExcep
	const std::vector<IPAddress> doDNSLookup(const std::string& hostname);// throw (NetworkingExcep);
	
	//throws NetworkingExcep
	const std::string doReverseDNSLookup(const IPAddress& ipaddr);// throw (NetworkingExcep);


	//-----------------------------------------------------------------
	//singleton stuff
	//-----------------------------------------------------------------
	/*static inline Networking& getInstance();

	static void createInstance(Networking* newinstance);
	static void destroyInstance();*/

	static inline bool isInited(){ return !isNull(); }

	//returns descriptive string of last (WSA) networking error
	static const std::string getError();

	void makeSocketsShutDown();
	bool shouldSocketsShutDown() const;

private:
	bool sockets_shut_down;
	//static Networking* instance;

	//IPAddress used_ipaddr;
	//bool used_ipaddr_determined;

	std::vector<IPAddress> hostips;
};



/*Networking& Networking::getInstance()
{
	assert(instance);
	return *instance;
}*/


#endif //__NETWORKING_H_666_




