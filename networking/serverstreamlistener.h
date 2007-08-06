/*=====================================================================
serverstreamlistener.h
----------------------
File created by ClassTemplate on Fri Sep 10 04:47:17 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __SERVERSTREAMLISTENER_H_666_
#define __SERVERSTREAMLISTENER_H_666_



class StreamSysClientProxy;
class Packet;

/*=====================================================================
ServerStreamListener
--------------------
interface for handling stream-specific events.
=====================================================================*/
class ServerStreamListener
{
public:
	/*=====================================================================
	ServerStreamListener
	--------------------
	
	=====================================================================*/
	//ServerStreamListener();

	virtual ~ServerStreamListener(){}


	virtual void handlePacket(int stream_uid, Packet& packet, 
									StreamSysClientProxy& sender_client) = 0;


	virtual void clientJoined(StreamSysClientProxy& client){}

	virtual void clientLeft(StreamSysClientProxy& client){}
};



#endif //__SERVERSTREAMLISTENER_H_666_




