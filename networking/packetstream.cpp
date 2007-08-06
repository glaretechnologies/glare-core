/*=====================================================================
packetstream.cpp
----------------
File created by ClassTemplate on Sun Sep 05 05:59:58 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "packetstream.h"


#include "pollsocket.h"

PacketStream::PacketStream(PollSocket& socket_)//MySocket& stream_)
:	//stream(stream_)
	socket(socket_)
{
	recving_packetsize = true;
}


PacketStream::~PacketStream()
{
	
}

void PacketStream::writePacket(Packet& p)
{
	//p.writeToStreamSized(stream);
	int packetsize = p.getPacketSize();//TEMP HACK

	try
	{
		socket.write(&packetsize, sizeof(packetsize));

		socket.write(p.getData(), packetsize);
	}
	catch(PollSocketExcep& e)
	{
		throw PacketStreamExcep("PollSocketExcep: " + e.what());
	}

}

void PacketStream::think()
{
	/*std::string rcvd_data;
	stream.pollRead(rcvd_data);
	queued_data += rcvd_data;*/

	try
	{
		socket.think();
	}
	catch(PollSocketExcep& e)
	{
		throw PacketStreamExcep("PollSocketExcep: " + e.what());
	}
}

const int MAX_PACKETSIZE = 10000000;//1MB


Packet* PacketStream::pollReadPacket()//Packet* packet_out)
{
	if(recving_packetsize)
	{
		if(socket.getInBufferSize() >= sizeof(int))
		{
			currentpacket = Packet();//reinit packet

			//NOTE: fugly code, uses intimate knowledge of packet class :P
			int packetsize;
			//socket.readTo(&packetsize, sizeof(int));
			socket >> packetsize;

			if(packetsize <= 0)
				throw PacketStreamExcep("incoming packetsize < 0");
			else if(packetsize > MAX_PACKETSIZE)
				throw PacketStreamExcep("incoming packetsize > MAX_PACKETSIZE");

			currentpacket.data.resize(packetsize);

			//queued_data = queued_data.substr(4, (int)queued_data.size() - 4);

			recving_packetsize = false;
		}
	}

	if(!recving_packetsize)
	{
		//if receiving packet body now
		if(socket.getInBufferSize() >= currentpacket.data.size())
		{
			//fill current packet
			//currentpacket.write(&(*queued_data.begin()), currentpacket.data.size());
			socket.readTo(currentpacket.getData(), currentpacket.data.size());
		
			recving_packetsize = true;//now rcv size of next packet

			return &currentpacket;
		}
	}



	/*if(recving_packetsize)
	{
		if(queued_data.size() >= 4)
		{
			//NOTE: fugly code
			int packetsize = *(int*)(&(*queued_data.begin()));
			currentpacket.data.resize(packetsize);

			queued_data = queued_data.substr(4, (int)queued_data.size() - 4);

			recving_packetsize = false;
		}
	}

	if(!recving_packetsize)
	{
		//if receiving packet body now
		if(queued_data.size() >= currentpacket.data.size())
		{
			//fill current packet
			currentpacket.write(&(*queued_data.begin()), currentpacket.data.size());

			queued_data = queued_data.substr(currentpacket.data.size(), (int)queued_data.size() - (int)currentpacket.data.size());
		
			return &currentpacket;
		}
	}*/

	return NULL;
}





