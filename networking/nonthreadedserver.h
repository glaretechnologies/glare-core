/*=====================================================================
nonthreadedserver.h
-------------------
File created by ClassTemplate on Sun Sep 05 21:00:39 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __NONTHREADEDSERVER_H_666_
#define __NONTHREADEDSERVER_H_666_



#include "distobserver.h"
#include "serverstreamlistener.h"
#include "pollsocket.h"
#include <set>
#include "intobuid.h"
class DistObClientProxy;
class Packet;
class CSMessage;
class StreamSysServer;
class DistObStoreInterface;
//class IntObUID;
class DistObServerListener;
class DistObUIDFactory;

/*=====================================================================
NonThreadedServer
-----------------
Distributed-object server implementation.
Fully general.
The server side of a client-server distributed object implementation.
=====================================================================*/
class NonThreadedServer : /*public DistObServer, */public ServerStreamListener
{
public:
	/*=====================================================================
	NonThreadedServer
	-----------------
	
	=====================================================================*/
	NonThreadedServer(StreamSysServer* stream_sys_server, DistObServerListener* listener,
						int stream_uid,
						DistObStoreInterface* store, float update_push_period,
						DistObUIDFactory* uid_factory);//,
						//DistObStoreInterface* updatestore);
		//throws DistObServerExcep

	virtual ~NonThreadedServer();

	//to be called by server logic if change accepted or autonomously
	virtual void makeStateChange(const UID_REF& uid, MyStream& state_stream);

	//deletes object, removes from store, broadcasts destroy to clients.
	//does nothing if no such object in store.
	virtual void destroyDistObject(const UID_REF& uid);

	//called by server side code if the object has been changed.
	//marks agent as needing to be broadcast to clients
	void objectChangedLocally(const UID_REF& uid);


	//virtual void createDistObject(DistObject& ob);


	//virtual 
	void think(double time);//poll for events

	//virtual void addListener(DistObServerListener* listener);
	//virtual void removeListener(DistObServerListener* listener);


	//for client proxies:
	//void handlePacketFromClient(Packet& packet, DistObClientProxy& client);


	//ServerStreamListener interface:
	//called when a packet for the server arrives thru the stream system.
	virtual void handlePacket(int stream_uid, Packet& packet, 
									StreamSysClientProxy& sender_client);

	virtual void clientJoined(StreamSysClientProxy& client);


	DistObStoreInterface& getStore(){ return *store; }

private:
	float update_push_period;
	double last_push_time;

	StreamSysServer* stream_sys_server;
	DistObUIDFactory* uid_factory;

	//std::set<DistObServerListener*> listeners;
	DistObServerListener* listener;

	int stream_uid;

	DistObStoreInterface* store;

	//PollSocket socket;

	//std::set<DistObClientProxy*> clients;

	//std::deque<CSMessage> messages_for_clients;

	std::set<UID_REF> updated_objects;

};



#endif //__NONTHREADEDSERVER_H_666_




