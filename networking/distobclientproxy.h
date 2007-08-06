/*=====================================================================
distobclientproxy.h
-------------------
File created by ClassTemplate on Sun Sep 05 20:35:14 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DISTOBCLIENTPROXY_H_666_
#define __DISTOBCLIENTPROXY_H_666_



class PollSocket;
class PacketStream;
class IPAddress;
class NonThreadedServer;
class Packet;

/*=====================================================================
DistObClientProxy
-----------------

=====================================================================*/
class DistObClientProxy
{
public:
	/*=====================================================================
	DistObClientProxy
	-----------------
	
	=====================================================================*/
	DistObClientProxy(PollSocket* socket);

	~DistObClientProxy();


	const IPAddress getClientIPAddr() const;


	void think(NonThreadedServer& server); //throw PollSocketExcep

	bool isConnected() const;

	void sendPacket(Packet& packet);

	const PollSocket& getSocket() const { return *socket; }

private:
	PollSocket* socket;
	PacketStream* packetstream;
};



#endif //__DISTOBCLIENTPROXY_H_666_




