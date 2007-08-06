/*=====================================================================
serverlistener.h
----------------
File created by ClassTemplate on Sun Sep 19 05:09:26 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __SERVERLISTENER_H_666_
#define __SERVERLISTENER_H_666_



class StreamSysClientProxy;
#include <string>


/*=====================================================================
ServerListener
--------------
Handles events sent by a StreamSysServer
These events are high level client-related events,
not related to any specific stream.
=====================================================================*/
class ServerListener
{
public:
	/*=====================================================================
	ServerListener
	--------------
	
	=====================================================================*/
	//ServerListener()

	virtual ~ServerListener(){}


	virtual bool connectionPermitted(StreamSysClientProxy& client,
										std::string& denied_msg_out) = 0;

	//client was accepted
	virtual void clientJoined(StreamSysClientProxy& client){}

	virtual void clientLeft(StreamSysClientProxy& client){}
};



#endif //__SERVERLISTENER_H_666_




