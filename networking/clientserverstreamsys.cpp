/*=====================================================================
clientserverstreamsys.cpp
-------------------------
File created by ClassTemplate on Fri Sep 10 03:49:37 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "clientserverstreamsys.h"


#include "pollsocket.h"
#include "packetstream.h"
#include <assert.h>
#include "streamlistener.h"


ClientServerStreamSys::ClientServerStreamSys()
{
	socket = NULL;
	packetstream = NULL;	

	state = DISCONNECTED;

	send_error_occurred = false;
}


ClientServerStreamSys::~ClientServerStreamSys()
{
	disconnect();	
}




void ClientServerStreamSys::connect(const std::string& hostname, int port)//throws DistObSysExcep
{
	send_error_occurred = false;//reset send error flag.

	try
	{
		socket = new PollSocket(hostname, port);

		packetstream = new PacketStream(*socket);

		state = CONNECTING;
	}
	catch(PollSocketExcep& e)
	{
		//NOTE: delete socket and packetstream here?
		throw StreamSysExcep("poll socket exception: " + e.what());
	}
}


bool ClientServerStreamSys::isConnected()
{
	/*if(socket)
		return socket->getState() == PollSocket::CONNECTED;
	else
		return NULL;*/

	return state == ACCEPTED;
}


void ClientServerStreamSys::disconnect()
{
	delete packetstream;
	packetstream = NULL;

	delete socket;
	socket = NULL;

	state = DISCONNECTED;
}

void ClientServerStreamSys::think()//poll for events.  throws StreamSysExcep if disconnected.
{
	if(send_error_occurred)
	{
		//if there was an error when sending a packet over the stream
		throw StreamSysExcep("send error.");
	}

	if(!socket || !packetstream)//if not connected
	{
		throw StreamSysExcep("not connected.");
		//return;//TODO: throw excep
	}

	//------------------------------------------------------------------------
	//process incoming packets from the packetstream
	//------------------------------------------------------------------------
	if(packetstream)
	{
		try
		{
			packetstream->think();
		}
		catch(PacketStreamExcep& e)
		{
			throw StreamSysExcep("connection lost. (" + e.what() + ")");
		}

		if(socket->getState() == PollSocket::CONNECTED)
		{
			//if TCP connect has been established

			if(state == CONNECTING)
			{
				//------------------------------------------------------------------------
				//send client version
				//------------------------------------------------------------------------
				const int client_version = 1;
				Packet p;
				p << client_version;

				packetstream->writePacket(p);

				state = SENT_CLIENT_VERSION;
			}
			else if(state == SENT_CLIENT_VERSION)
			{
				Packet* incoming_packet = packetstream->pollReadPacket();

				if(incoming_packet)
				{
					//------------------------------------------------------------------------
					//decode reply packet
					//------------------------------------------------------------------------
					int join_result;
					*incoming_packet >> join_result;

					std::string join_message;
					*incoming_packet >> join_message;

					//TODO: print out join message...

					if(join_result != 1)
						throw StreamSysExcep("Connection refused by server: '" + join_message + "'");

					state = ACCEPTED;
				}
			}
			else if(state == ACCEPTED)
			{

				Packet* incoming_packet = packetstream->pollReadPacket();
				while(incoming_packet)
				{
					processPacket(*incoming_packet);

					incoming_packet = packetstream->pollReadPacket();//go on to next packet
				}
			}
			else
			{
				assert(0);//invalid state
			}
		}
		
		/*}
		catch(PollSocketExcep& e)
		{
			throw StreamSysExcep("poll socket exception: " + e.what());
		}*/
	}

}

void ClientServerStreamSys::addStreamListener(int stream_uid, StreamListener* listener)
{
	std::map<int, StreamListener*>::iterator result = listeners.find(stream_uid);

	if(result == listeners.end())
	{
		listeners[stream_uid] = listener;
	}
	else
	{
		assert(0);//stream uid already used.
	}
		
}

void ClientServerStreamSys::removeStreamListener(int stream_uid)
{
	listeners.erase(stream_uid);
}


void ClientServerStreamSys::processPacket(Packet& packet)
{
	//read stream uid
	int stream_uid;
	packet >> stream_uid;

	//------------------------------------------------------------------------
	//lookup stream handler by stream UID
	//------------------------------------------------------------------------
	std::map<int, StreamListener*>::iterator result = listeners.find(stream_uid);

	if(result == listeners.end())
	{
		//no handler for this stream uid...
		assert(0);
	}
	else
	{
		try
		{
			(*result).second->handlePacket(stream_uid, packet);
		}
		catch(MyStreamExcep&)
		{
			//malformed message packet.
			//assert(0);

			//throw StreamSysExcep("MyStreamExcep: " + e.what());

			send_error_occurred = true;
		}
	}
}


void ClientServerStreamSys::sendPacket(int stream_uid, Packet& packet)
{
	assert(isConnected()); //state == ACCEPTED);


	Packet wrapper_packet;

	//------------------------------------------------------------------------
	//prefix message with the stream UID
	//------------------------------------------------------------------------
	wrapper_packet << stream_uid;
	packet.writeToStream(wrapper_packet);

	if(packetstream)
	{
		try
		{
			packetstream->writePacket(wrapper_packet);
		}
		catch(PacketStreamExcep& e)
		{
			//was an exception writing to the packet stream..

			//assert(0);
			//NOTE: throw exception here? or just give warning?

			throw StreamSysExcep("PacketStreamExcep: " + e.what());
		}
	}
}

