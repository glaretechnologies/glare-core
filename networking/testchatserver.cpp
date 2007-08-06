/*=====================================================================
testchatserver.cpp
------------------
File created by ClassTemplate on Fri Sep 10 05:23:52 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "testchatserver.h"


#include <iostream>
#include <string>
#include "packet.h"
#include "streamsysclientproxy.h"
#include "ipaddress.h"
#include "pollsocket.h"
#include "streamsysserver.h"
#include "streamuids.h"


TestChatServer::TestChatServer(StreamSysServer* stream_sys_server_)
:	stream_sys_server(stream_sys_server_)
{
	stream_sys_server->addStreamListener(StreamUIDs::STREAM_UI_CHAT, this);
}


TestChatServer::~TestChatServer()
{
	stream_sys_server->removeStreamListener(StreamUIDs::STREAM_UI_CHAT);
}

void TestChatServer::handlePacket(int stream_uid, Packet& packet, 
									StreamSysClientProxy& sender_client)
{
	std::string chatstring;
	packet >> chatstring;

	std::cout << "[chat from client " << sender_client.getSocket().getOtherEndIPAddress().toString() << 
				":" << sender_client.getSocket().getOtherEndPort() << "]" << chatstring << std::endl;

	//broadcast to all clients
	Packet newpacket;//NOTE: Needed?
	newpacket << chatstring;
	stream_sys_server->streamToAllClients(StreamUIDs::STREAM_UI_CHAT, newpacket);
}





