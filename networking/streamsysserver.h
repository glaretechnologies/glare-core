/*=====================================================================
streamsysserver.h
-----------------
File created by ClassTemplate on Fri Sep 10 04:33:02 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __STREAMSYSSERVER_H_666_
#define __STREAMSYSSERVER_H_666_



#include "pollsocket.h"
#include <map>
#include <set>

class ServerStreamListener;
class Packet;
class StreamSysClientProxy;
class ServerListener;


class StreamSysServerExcep
{
public:
	StreamSysServerExcep(const std::string& message_) : message(message_) {}
	~StreamSysServerExcep(){}

	const std::string& what() const { return message; }
private:
	std::string message;
};


/*=====================================================================
StreamSysServer
---------------
Acts as the server side of a Multiplexer/Demultiplexer.
a concrete class for now.
=====================================================================*/
class StreamSysServer
{
public:
	/*=====================================================================
	StreamSysServer
	---------------
	
	=====================================================================*/
	StreamSysServer(int listenport, ServerListener* server_listener);

	~StreamSysServer();


	void think();//poll for events

	
	void addStreamListener(int stream_uid, ServerStreamListener* listener);
	void removeStreamListener(int stream_uid);

	void streamToAllClients(int stream_uid, Packet& packet);

	void streamToClient(StreamSysClientProxy& client, int stream_uid, Packet& packet);




	//to be called only by client proxies:
	void handlePacketFromClient(Packet& packet, StreamSysClientProxy& client);

	void clientAccepted(StreamSysClientProxy& client);

	bool connectionPermitted(StreamSysClientProxy& client,
								std::string& denied_msg_out);




	const std::string& getAcceptedMessage() const;
	void setAcceptedMessage(const std::string& msg);


private:
	ServerListener* server_listener;

	//ServerStreamListener* listener;

	PollSocket socket;


	std::map<int, ServerStreamListener*> listeners;

	std::set<StreamSysClientProxy*> clients;

	std::string accepted_msg;
};



#endif //__STREAMSYSSERVER_H_666_




