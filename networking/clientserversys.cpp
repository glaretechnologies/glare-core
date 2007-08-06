/*=====================================================================
clientserversys.cpp
-------------------
File created by ClassTemplate on Sat Sep 04 12:55:04 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "clientserversys.h"


//#include "pollsocket.h"
#include "distobject.h"
#include "csmessage.h"
#include "distobuid.h"
//#include "packetstream.h"
#include "distoblistener.h"
#include "assert.h"
#include "intobuid.h"
#include "clientserverstreamsys.h"
#include "streamuids.h"
#include "distobstoreinterface.h"
#include <assert.h>
#include "distobuidfactory.h"



ClientServerSys::ClientServerSys(	DistObListener* listener, 
									ClientServerStreamSys* cs_stream_sys_,
									int stream_uid_,
									DistObStoreInterface* update_store,
									DistObStoreInterface* store,
									DistObUIDFactory* uid_factory,
									double update_push_period_)
:	DistObSys(listener, update_store, store, uid_factory),
	cs_stream_sys(cs_stream_sys_),
	stream_uid(stream_uid_),
	update_push_period(update_push_period_)
{
	assert(update_push_period > 0.0);

	cs_stream_sys->addStreamListener(stream_uid, this);

	last_push_time = 0.0;
}


ClientServerSys::~ClientServerSys()
{
	cs_stream_sys->removeStreamListener(stream_uid);
}




void ClientServerSys::tryCreateDistOb(DistObject& ob)
{
	assert(getUpdateStore());//if making changes then need update store.

	this->created_objects.insert(ob.getUID());

	//NOTE: actually no reason to delay this message :P
}

void ClientServerSys::tryUpdateDistOb(DistObject& ob)
{
	assert(getUpdateStore());//if making changes then need update store.

	updated_objects.insert(ob.getUID());
}


void ClientServerSys::tryDestroyDistOb(DistObject& ob)
{
	assert(getUpdateStore());//if making changes then need update store.

	updated_objects.insert(ob.getUID());
}



	//handle an incoming packet from the stream server on the DistOb stream
void ClientServerSys::handlePacket(int incoming_stream_uid, Packet& packet)
{
	assert(incoming_stream_uid == stream_uid);

	//------------------------------------------------------------------------
	//deserialise CS message from packet
	//------------------------------------------------------------------------
	CSMessage message;
	message.deserialise(packet);

	processMessage(message);
}


void ClientServerSys::processMessage(CSMessage& msg)
{
//	try
//	{
	//for(std::set<DistObListener*>::iterator i = listeners.begin(); i != listeners.end(); ++i)
	//{
		//DistObListener* listener = this->getListener();//(*i);

		if(msg.msg_id == CSMessage::/*MESSAGE_ID::*/CHANGE_OBJECT_STATE)
		{
			//listener->distObjectStateChange(msg.object_id_packet, msg.object_state);
			this->updateDistOb(msg.object_id_packet, msg.object_state);//inform superclass
		}
		/*else if(msg.msg_id == CSMessage::CREATE_OBJECT)
		{
			listener->distObjectCreated(msg.object_id_packet, msg.object_state);
		}*/
		else if(msg.msg_id == CSMessage::/*MESSAGE_ID::*/DESTROY_OBJECT)
		{
			//------------------------------------------------------------------------
			//deserialise UID
			//------------------------------------------------------------------------
			UID_REF uid = this->getUIDFactory().createUID();
			uid->deserialise(msg.object_id_packet);

			//------------------------------------------------------------------------
			//remove the uid from the 'updated_objects' set
			//Otherwise will just get erroneously recreated.
			//------------------------------------------------------------------------
			updated_objects.erase(uid);

			
			//------------------------------------------------------------------------
			//object has been destroyed by server, get base DistObSys class to handle it
			//------------------------------------------------------------------------
			//listener->distObjectDestroyed(msg.object_id_packet);
			//this->destroyDistOb(msg.object_id_packet);//inform superclass

			this->destroyDistOb(uid);
		}
		else
		{
			//error
			assert(0);
		}
	//}

/*	}
	catch(MyStreamExcep&)
	{
		//do something here?
		assert(0);
	}
*/
}


void ClientServerSys::think(double time)
{
	const double time_since_last_push = time - last_push_time;

	if(time_since_last_push >= update_push_period)//if time to push updates...
	{	
		//------------------------------------------------------------------------
		//push pending updates to server
		//------------------------------------------------------------------------
		for(std::set<UID_REF>::iterator i = updated_objects.begin(); i != updated_objects.end(); ++i)
		{
			assert(getUpdateStore());//if making changes then need update store.

			if(!getUpdateStore())
				return;//avoid crash

			//lookup in update store
			DistObject* object = this->getUpdateStore()->getDistObjectForUID(*i);

			if(object)
			{
				//send update message
				CSMessage message;

				message.msg_id = CSMessage::CHANGE_OBJECT_STATE;//CSMessage::MESSAGE_ID::CHANGE_OBJECT_STATE;
				object->getUID()->serialise(message.object_id_packet);
				object->serialise(message.object_state);

				Packet packet;
				message.serialise(packet);

				cs_stream_sys->sendPacket(stream_uid, packet);
			}
			else
			{
				//send destroy message
				CSMessage message;

				message.msg_id = CSMessage::DESTROY_OBJECT;//CSMessage::MESSAGE_ID::DESTROY_OBJECT;
				(*i)->serialise(message.object_id_packet);

				Packet packet;
				message.serialise(packet);

				cs_stream_sys->sendPacket(stream_uid, packet);
			}
		}

		updated_objects.clear();

		//------------------------------------------------------------------------
		//push pending creates to server
		//------------------------------------------------------------------------
		for(i = created_objects.begin(); i != created_objects.end(); ++i)
		{
			if(!getUpdateStore())
				return;//avoid crash

			//lookup in update store
			DistObject* object = this->getUpdateStore()->getDistObjectForUID(*i);

			//assert(object);

			if(object)
			{
				CSMessage message;

				message.msg_id = CSMessage::CREATE_OBJECT;
				object->getUID()->serialise(message.object_id_packet);
				object->serialise(message.object_state);

				Packet packet;
				message.serialise(packet);

				cs_stream_sys->sendPacket(stream_uid, packet);
			}
		}

		created_objects.clear();


		last_push_time = time;
	}
}



/*void ClientServerSys::doUpdateReceived(const UID_REF uid)
{
	if(updated_objects.find(uid) == updated_objects.end())
	{
		getUpdateStore()->removeDistObject(uid);
	}
}*/
