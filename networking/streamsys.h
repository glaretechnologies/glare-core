/*=====================================================================
streamsys.h
-----------
File created by ClassTemplate on Fri Sep 10 03:41:08 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __STREAMSYS_H_666_
#define __STREAMSYS_H_666_



class StreamListener;
#include <string>
class Packet;


class StreamSysExcep
{
public:
	StreamSysExcep(const std::string& message_) : message(message_) {}
	~StreamSysExcep(){}

	const std::string& what() const { return message; }
private:
	std::string message;
};


/*=====================================================================
StreamSys
---------
Interface to a stream system.
Used by a client/peer.  (not by a server tho)
=====================================================================*/
class StreamSys
{
public:
	/*=====================================================================
	StreamSys
	---------
	
	=====================================================================*/
	//StreamSys();

	virtual ~StreamSys(){}

	//connect to server or bootstrap host in the case of a p2p sys.
	virtual void connect(const std::string& hostname, int port) = 0;//throws DistObSysExcep
	virtual void disconnect() = 0;

	virtual bool isConnected() = 0;


	virtual void think() = 0;//poll for events.  
							//throws StreamSysExcep if disconnected.

	virtual void addStreamListener(int stream_uid, StreamListener* listener) = 0;
	virtual void removeStreamListener(int stream_uid) = 0;

	virtual void sendPacket(int stream_uid, Packet& packet) = 0;
	//fails silently if write failed.  But if failed next think() will throw excep.
};



#endif //__STREAMSYS_H_666_




