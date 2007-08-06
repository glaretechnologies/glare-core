/*=====================================================================
testchatclient.h
----------------
File created by ClassTemplate on Fri Sep 10 05:23:42 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __TESTCHATCLIENT_H_666_
#define __TESTCHATCLIENT_H_666_



#include "StreamListener.h"
#include <string>
class StreamSys;

/*=====================================================================
TestChatClient
--------------
a chat stream client.
=====================================================================*/
class TestChatClient : public StreamListener
{
public:
	/*=====================================================================
	TestChatClient
	--------------
	
	=====================================================================*/
	TestChatClient(StreamSys* stream_sys);

	virtual ~TestChatClient();

	//send a chat message into the wide wide world.
	void sendChatMessage(const std::string& message);


	//StreamListener interface:
	virtual void handlePacket(int stream_uid, Packet& packet);

private:
	StreamSys* stream_sys;
};



#endif //__TESTCHATCLIENT_H_666_




