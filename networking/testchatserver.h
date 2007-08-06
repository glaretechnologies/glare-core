/*=====================================================================
testchatserver.h
----------------
File created by ClassTemplate on Fri Sep 10 05:23:52 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TESTCHATSERVER_H_666_
#define __TESTCHATSERVER_H_666_



#include "serverStreamListener.h"
class StreamSysServer;


/*=====================================================================
TestChatServer
--------------

=====================================================================*/
class TestChatServer : public ServerStreamListener
{
public:
	/*=====================================================================
	TestChatServer
	--------------
	
	=====================================================================*/
	TestChatServer(StreamSysServer* stream_sys_server);

	~TestChatServer();


	void handlePacket(int stream_uid, Packet& packet, 
									StreamSysClientProxy& sender_client);


private:
	StreamSysServer* stream_sys_server;
};



#endif //__TESTCHATSERVER_H_666_




