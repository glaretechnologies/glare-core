/*=====================================================================
distobclientproxy.cpp
---------------------
File created by ClassTemplate on Sun Sep 05 20:35:14 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "distobclientproxy.h"


#include "packetstream.h"
#include "pollsocket.h"
#include <assert.h>
#include "nonthreadedserver.h"


DistObClientProxy::DistObClientProxy(PollSocket* socket_)
:	socket(socket_)
{
	packetstream = new PacketStream(*socket);
}


DistObClientProxy::~DistObClientProxy()
{
	delete packetstream;
	delete socket;
}

void DistObClientProxy::think(NonThreadedServer& server) //throw PollSocketExcep
{

	//try
	//{
		//------------------------------------------------------------------------
		//let socket think
		//------------------------------------------------------------------------
		packetstream->think();
	/*}
	catch(PollSocketExcep&)
	{
		//client is dead
		//assert(socket->getState() == PollSocket::CLOSED);
		//return;
	}*/

	Packet* packet = packetstream->pollReadPacket();

	while(packet)
	{
		server.handlePacketFromClient(*packet, *this);

		packet = packetstream->pollReadPacket();
	}




}



bool DistObClientProxy::isConnected() const
{
	assert(socket->getState() == PollSocket::CONNECTED || socket->getState() == PollSocket::CLOSED);

	return socket->getState() != PollSocket::CLOSED;
}


void DistObClientProxy::sendPacket(Packet& packet)
{
	packetstream->writePacket(packet);
}
