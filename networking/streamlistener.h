/*=====================================================================
streamlistener.h
----------------
File created by ClassTemplate on Fri Sep 10 03:46:12 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __STREAMLISTENER_H_666_
#define __STREAMLISTENER_H_666_



class Packet;

/*=====================================================================
StreamListener
--------------
Interface so that a client can receive packet events from a stream
=====================================================================*/
class StreamListener
{
public:
	/*=====================================================================
	StreamListener
	--------------
	
	=====================================================================*/
	//StreamListener();

	virtual ~StreamListener(){}

	//a packet has been delivered over the stream..
	virtual void handlePacket(int stream_uid, Packet& packet) = 0;


};



#endif //__STREAMLISTENER_H_666_




