/*=====================================================================
streamsysclientproxy.cpp
------------------------
File created by ClassTemplate on Fri Sep 10 04:38:08 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "streamsysclientproxy.h"




#include "packetstream.h"
#include "pollsocket.h"
#include <assert.h>
#include "streamsysserver.h"


StreamSysClientProxy::StreamSysClientProxy(PollSocket* socket_)
:	socket(socket_)
{
	state = INITIAL;

	packetstream = new PacketStream(*socket);

}


StreamSysClientProxy::~StreamSysClientProxy()
{
	delete packetstream;
	delete socket;
}

const IPAddress StreamSysClientProxy::getClientIPAddr() const
{
	return socket->getOtherEndIPAddress();
}

int StreamSysClientProxy::getClientPort() const
{
	return socket->getOtherEndPort();
}



void StreamSysClientProxy::think(StreamSysServer& server) //throw StreamSysClientProxyExcep
{

	try
	{
		//------------------------------------------------------------------------
		//let socket think
		//------------------------------------------------------------------------
		packetstream->think();
	}
	catch(PacketStreamExcep& e)
	{
		//client is dead
		//just do nothing for now

		throw StreamSysClientProxyExcep("PacketStreamExcep: " + e.what());

		//assert(socket->getState() == PollSocket::CLOSED);
		//return;
	}

	Packet* packet = packetstream->pollReadPacket();

	while(packet)
	{

		if(state == INITIAL)
		{
			//------------------------------------------------------------------------
			//read version
			//------------------------------------------------------------------------
			try
			{
				int client_version;
				*packet >> client_version;

				//TODO: test for appropriate version.
			}
			catch(MyStreamExcep&)
			{
				throw StreamSysClientProxyExcep("could not read client version.");
			}

			std::string denied_msg;
			const bool connection_allowed = server.connectionPermitted(*this, denied_msg);

			Packet reply;

			if(connection_allowed)
			{
				//------------------------------------------------------------------------
				//send back ok and welcome message
				//------------------------------------------------------------------------
				reply << (int)1;

				//reply << std::string("welcome to the server");
				reply << server.getAcceptedMessage();
			}
			else
			{
				reply << (int)0;

				reply << denied_msg;

			}


			sendPacket(reply);

			if(connection_allowed)
			{
				state = ACCEPTED;
				server.clientAccepted(*this);
			}
			else
				throw StreamSysClientProxyExcep("client connect denied by server.");

		}
		else if(state == ACCEPTED)
		{

			//------------------------------------------------------------------------
			//forward packet to stream sys server for demultiplexing.
			//------------------------------------------------------------------------
			try
			{
				server.handlePacketFromClient(*packet, *this);
			}
			catch(MyStreamExcep& e)
			{
				throw StreamSysClientProxyExcep("MyStreamExcep: " + e.what());
			}
		}
		else
		{
			assert(0);//invalid state
		}


		packet = packetstream->pollReadPacket();
	}
}


bool StreamSysClientProxy::accepted() const
{
	return state == ACCEPTED;
}

/*bool StreamSysClientProxy::isConnected() const
{
	assert(socket->getState() == PollSocket::CONNECTED || socket->getState() == PollSocket::CLOSED);

	return socket->getState() != PollSocket::CLOSED;
}*/


void StreamSysClientProxy::sendPacket(Packet& packet)
{
	try
	{
		packetstream->writePacket(packet);
	}
	catch(PacketStreamExcep& e)
	{
		throw StreamSysClientProxyExcep("PacketStreamExcep: " + e.what());
	}
}



