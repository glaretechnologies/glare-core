/*=====================================================================
testchatclient.cpp
------------------
File created by ClassTemplate on Fri Sep 10 05:23:42 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "testchatclient.h"


#include <iostream>
#include <string>
#include <assert.h>
#include "packet.h"
#include "streamsys.h"
#include "streamuids.h"

TestChatClient::TestChatClient(StreamSys* stream_sys_)
:	stream_sys(stream_sys_)
{
	//register stream packet handler
	stream_sys->addStreamListener(StreamUIDs::STREAM_UI_CHAT, this);
}


TestChatClient::~TestChatClient()
{
	//unregister stream packet handler
	stream_sys->removeStreamListener(StreamUIDs::STREAM_UI_CHAT);
}

void TestChatClient::sendChatMessage(const std::string& message)
{
	Packet p;
	p << message;

	stream_sys->sendPacket(StreamUIDs::STREAM_UI_CHAT, p);
}


void TestChatClient::handlePacket(int stream_uid, Packet& packet)
{
	assert(stream_uid == StreamUIDs::STREAM_UI_CHAT);

	std::string chatstring;
	packet >> chatstring;

	std::cout << "[chat]" << chatstring << std::endl;
}





