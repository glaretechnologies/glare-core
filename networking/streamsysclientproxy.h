/*=====================================================================
streamsysclientproxy.h
----------------------
File created by ClassTemplate on Fri Sep 10 04:38:08 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __STREAMSYSCLIENTPROXY_H_666_
#define __STREAMSYSCLIENTPROXY_H_666_


#include <string>
class PollSocket;
class PacketStream;
class IPAddress;
class StreamSysServer;
class Packet;


class StreamSysClientProxyExcep
{
public:
	StreamSysClientProxyExcep(const std::string& message_) : message(message_) {}
	~StreamSysClientProxyExcep(){}

	const std::string& what() const { return message; }
private:
	std::string message;
};

/*=====================================================================
StreamSysClientProxy
--------------------
Internal class used by StreamSysServer
Proxy class for a client connected to a StreamSysServer
=====================================================================*/
class StreamSysClientProxy
{
public:
	/*=====================================================================
	StreamSysClientProxy
	--------------------
	
	=====================================================================*/
	StreamSysClientProxy(PollSocket* socket);
	~StreamSysClientProxy();


	const IPAddress getClientIPAddr() const;
	int getClientPort() const;


	void think(StreamSysServer& server); //throw StreamSysClientProxyExcep

	//bool isConnected() const;
	bool accepted() const;

	
	//this func should only be called by StreamSysServer!!!!!!
	void sendPacket(Packet& packet); //throw StreamSysClientProxyExcep

	//just for getting ip address :P
	//const PollSocket& getSocket() const { return *socket; }


private:
	PollSocket* socket;
	PacketStream* packetstream;

	enum STATE
	{
		INITIAL,
		ACCEPTED
	};

	STATE state;
};



#endif //__STREAMSYSCLIENTPROXY_H_666_




