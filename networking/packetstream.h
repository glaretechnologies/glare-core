/*=====================================================================
packetstream.h
--------------
File created by ClassTemplate on Sun Sep 05 05:59:58 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PACKETSTREAM_H_666_
#define __PACKETSTREAM_H_666_



//class MySocket;
class PollSocket;
#include "packet.h"

class PacketStreamExcep
{
public:
	PacketStreamExcep(const std::string& message_) : message(message_) {}
	~PacketStreamExcep(){}

	const std::string& what() const { return message; }
private:
	std::string message;
};

/*=====================================================================
PacketStream
------------
For non-blocking sockets.
Stream for reading/writing Packets.
Non-blocking (Polling).
=====================================================================*/
class PacketStream
{
public:
	/*=====================================================================
	PacketStream
	------------
	
	=====================================================================*/
	PacketStream(PollSocket& socket);//MySocket& stream);

	~PacketStream();

	void writePacket(Packet& p);//throws PacketStreamExcep


	//void processIncomingData();

	void think();//throws PacketStreamExcep

	Packet* pollReadPacket();//NOTE: make this return a const pointer?
	//returns NULL if no more packets to read.

private:
	//MySocket& stream;
	PollSocket& socket;
	
	Packet currentpacket;
	//std::string queued_data;
	bool recving_packetsize;
};



#endif //__PACKETSTREAM_H_666_




