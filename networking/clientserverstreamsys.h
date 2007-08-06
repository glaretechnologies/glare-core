/*=====================================================================
clientserverstreamsys.h
-----------------------
File created by ClassTemplate on Fri Sep 10 03:49:37 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __CLIENTSERVERSTREAMSYS_H_666_
#define __CLIENTSERVERSTREAMSYS_H_666_


#pragma warning(disable : 4786)//disable long debug name warning


#include "streamsys.h"
class PollSocket;
class PacketStream;
#include <map>
class Packet;





/*=====================================================================
ClientServerStreamSys
---------------------
Client for a client-server implementation of a stream system.
=====================================================================*/
class ClientServerStreamSys : public StreamSys
{
public:
	/*=====================================================================
	ClientServerStreamSys
	---------------------
	
	=====================================================================*/
	ClientServerStreamSys();

	virtual ~ClientServerStreamSys();


	virtual void connect(const std::string& hostname, int port);//throws StreamSysExcep
	virtual void disconnect();

	virtual void think();//poll for events.  throws StreamSysExcep if disconnected.

	//don't call sendPacket until this is true.
	virtual bool isConnected();



	virtual void addStreamListener(int stream_uid, StreamListener* listener);
	virtual void removeStreamListener(int stream_uid);
	
	//send over a particular stream.
	//isConnected() must be true.
	virtual void sendPacket(int stream_uid, Packet& packet);
	//fails silently if write failed.  But if failed next think() will throw excep.


private:
	void processPacket(Packet& packet);

	PollSocket* socket;

	PacketStream* packetstream;

	std::map<int, StreamListener*> listeners;

	enum STATE
	{
		DISCONNECTED,
		CONNECTING,//socket connecting
		SENT_CLIENT_VERSION,//socket connected, sent client version
		ACCEPTED//received welcome message, fully connected.
	};

	STATE state;

	bool send_error_occurred;
};



#endif //__CLIENTSERVERSTREAMSYS_H_666_




