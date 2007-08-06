/*=====================================================================
nonthreadedserver.cpp
---------------------
File created by ClassTemplate on Sun Sep 05 21:00:39 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "nonthreadedserver.h"


#include <assert.h>
#include "distobclientproxy.h"
#include "csmessage.h"
#include "distobject.h"
#include "distobserverlistener.h"
#include "distobstoreinterface.h"
#include <iostream>
#include "streamuids.h"
#include "streamsysserver.h"
#include "streamsysclientproxy.h"
#include "distobuidfactory.h"

NonThreadedServer::NonThreadedServer(	StreamSysServer* stream_sys_server_, 
										DistObServerListener* listener_,
										int stream_uid_,
										DistObStoreInterface* store_, 
										float update_push_period_,
										DistObUIDFactory* uid_factory_)//,
										//DistObStoreInterface* updatestore)
:	stream_sys_server(stream_sys_server_),
	listener(listener_),
	stream_uid(stream_uid_),
	store(store_),
	update_push_period(update_push_period_),
	uid_factory(uid_factory_)
{
	assert(update_push_period > 0.0f);

	last_push_time = 0.0;

	stream_sys_server->addStreamListener(stream_uid, this);
}


NonThreadedServer::~NonThreadedServer()
{
	stream_sys_server->removeStreamListener(stream_uid);
}


void NonThreadedServer::makeStateChange(const UID_REF& uid, MyStream& state_stream)
{
	//------------------------------------------------------------------------
	//update local store
	//------------------------------------------------------------------------
	//std::cout << "uid: " + uid->toString() << std::endl;
	DistObject* local_ob = store->getDistObjectForUID(uid);
	//std::cout << "uid: " + uid->toString() << std::endl;

	if(local_ob)
	{
//		std::cout << "updating object with uid=" << uid->toString() << std::endl;

		local_ob->deserialise(state_stream);//update local ob
		assert(local_ob->getUID() == uid);
	}
	else
	{			
		std::cout << "creating new object with uid=" << uid->toString() << std::endl;

		//create new object in local store
		store->constructAndInsertFromStream(state_stream, true);

		assert(store->getDistObjectForUID(uid));//assert in store with correct UID now.
	}

	objectChangedLocally(uid);//mark object as changed so is sent to clients.
}


void NonThreadedServer::objectChangedLocally(const UID_REF& uid)
{
	updated_objects.insert(uid);//mark object as changed so is sent to clients.
}

/*void NonThreadedServer::createDistObject(DistObject& ob)
{
	//------------------------------------------------------------------------
	//form a message
	//------------------------------------------------------------------------
	CSMessage message;
	message.msg_id = CSMessage::CREATE_OBJECT;
	//message.object_id = ob.getUID();
	ob.serialiseUID(message.object_id_packet);
	ob.serialise(message.object_state);

	//queue it
	//messages_for_clients.push_back(message);

	try
	{
		Packet p;
		message.serialise(p);
		stream_sys_server->streamToAllClients(stream_uid, p);
	}
	catch(PollSocketExcep&)
	{}
}*/


void NonThreadedServer::destroyDistObject(const UID_REF& uid)
{
	objectChangedLocally(uid);//mark object as changed so is sent to clients.


	//change local store

	std::cout << "Server: destroying object with uid: " << uid->toString() << std::endl;

	DistObject* ob = store->getDistObjectForUID(uid);

	if(ob)
	{
		store->removeDistObject(uid);


	//	assert(ob);

		//------------------------------------------------------------------------
		//finally delete the object. NOTE that uid is a const reference to a member
		//of this object!!!!!!!!!!! (so don't delete earlier)
		//--explanation OBSOLETE now: uid ref counted.
		//------------------------------------------------------------------------
		delete ob;
	}
}


void NonThreadedServer::think(double time)
{
	//NOTE: do socket excep handling

	const double time_since_last_push = time - last_push_time;

	if(time_since_last_push >= (double)update_push_period)
	{
		//------------------------------------------------------------------------
		//push pending updates to clients
		//------------------------------------------------------------------------

		for(std::set<UID_REF>::iterator i = updated_objects.begin(); i != updated_objects.end(); ++i)
		{
			//lookup in update store
			DistObject* object = this->store->getDistObjectForUID(*i);

			if(object)
			{
				//send update message
				CSMessage message;

				message.msg_id = CSMessage::CHANGE_OBJECT_STATE;//CSMessage::MESSAGE_ID::CHANGE_OBJECT_STATE;
				object->getUID()->serialise(message.object_id_packet);//write UID to packet
				object->serialise(message.object_state);

				//------------------------------------------------------------------------
				//send message to all clients
				//------------------------------------------------------------------------
				Packet packet;
				message.serialise(packet);

				stream_sys_server->streamToAllClients(stream_uid, packet);
			}
			else //else if object does not exist...
			{
				//send destroy message
				CSMessage message;

				message.msg_id = CSMessage::DESTROY_OBJECT;//CSMessage::MESSAGE_ID::DESTROY_OBJECT;
				(*i)->serialise(message.object_id_packet);//write UID to packet

				//------------------------------------------------------------------------
				//send message to all clients
				//------------------------------------------------------------------------
				Packet packet;
				message.serialise(packet);

				stream_sys_server->streamToAllClients(stream_uid, packet);
			}
		}

		updated_objects.clear();

		last_push_time = time;
	}
}



	//ServerStreamListener interface:
void NonThreadedServer::handlePacket(int incoming_stream_uid, Packet& packet, 
									StreamSysClientProxy& sender_client)
{
	assert(incoming_stream_uid == stream_uid);

	//------------------------------------------------------------------------
	//deserialise into CSMessage
	//------------------------------------------------------------------------
	try
	{
		CSMessage msg;
		msg.deserialise(packet);

	
		//read object UID from packet
		UID_REF uid = uid_factory->createUID();
		uid->deserialise(msg.object_id_packet);

		//for(std::set<DistObServerListener*>::iterator i = listeners.begin(); i != listeners.end(); ++i)
		//{
		//	DistObServerListener* listener = (*i);

			if(msg.msg_id == CSMessage::CREATE_OBJECT)
			{
				//listener->tryDistObjectCreated(sender_client, msg.object_id_packet, msg.object_state);
			
				listener->tryDistObjectStateChange(*this, sender_client, 
									uid, msg.object_state);

			}
			else if(msg.msg_id == CSMessage::CHANGE_OBJECT_STATE)
			{
				//std::cout << "received state update for object with uid=" << uid.getId() << std::endl;
				//------------------------------------------------------------------------
				//deserialise into an object
				//------------------------------------------------------------------------
				//DistObject* incoming_ob = store->constructObFromStream(msg.object_state);
				
				if(store->getDistObjectForUID(uid))//if object exists
				{
					listener->tryDistObjectStateChange(*this, sender_client, 
										uid, msg.object_state);
				}
				else
				{
					//client tried to alter an object that does not exist,
					//perhaps because it was just deleted at server
					//but not at client yet.  So just do nothing.

					//std::cout << "discarding update for non-existant object." << std::endl;
				}

			}
			else if(msg.msg_id == CSMessage::DESTROY_OBJECT)
			{
//				std::cout << "destroying object with uid=" << uid.getId()->toString() << std::endl;
				
				//------------------------------------------------------------------------
				//deserialise UID
				//------------------------------------------------------------------------
				//IntObUID uid;
				//uid.deserialise(msg.object_id_packet);
				
				listener->tryDistObjectDestroyed(*this, sender_client, uid);
			}
			else
			{
				//error - unknown message id.
				//TODO: throw excep?
				assert(0);
			}
		//}


	}
	catch(MyStreamExcep& e)
	{
		//invalid message from client.
		//assert(0);
		std::cerr << "invalid message from client: " << e.what() << std::endl;
	}

}

void NonThreadedServer::clientJoined(StreamSysClientProxy& client)
{
	//------------------------------------------------------------------------
	//transfer all object state to new client
	//------------------------------------------------------------------------
	std::vector<DistObject*> objects;
	store->getObjects(objects);

	std::cout << "Server: sending " << objects.size() << " objects to new client..." << std::endl;

	for(int i=0; i<objects.size(); ++i)
	{
		CSMessage message;
		message.msg_id = CSMessage::CHANGE_OBJECT_STATE;
		objects[i]->getUID()->serialise(message.object_id_packet);
		objects[i]->serialise(message.object_state);

		try
		{
			Packet p;
			message.serialise(p);

			stream_sys_server->streamToClient(client, stream_uid, p);
		}
		catch(PollSocketExcep&)
		{}
	}
}
