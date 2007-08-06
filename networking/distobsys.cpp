/*=====================================================================
distobsys.cpp
-------------
File created by ClassTemplate on Fri Sep 03 04:46:37 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "distobsys.h"


//#include "intobuid.h"
#include "distobstoreinterface.h"
#include "distobject.h"
#include "distoblistener.h"
#include "distobuidfactory.h"
#include <assert.h>



DistObSys::DistObSys(	DistObListener* listener_, 
						DistObStoreInterface* update_store_,
						DistObStoreInterface* store_,
						DistObUIDFactory* uid_factory_)
:	listener(listener_),
	update_store(update_store_),
	store(store_),
	uid_factory(uid_factory_)
{
	//assert(update_store);
	assert(store);
	assert(uid_factory);
}


DistObSys::~DistObSys()
{
	
}

//called by subclass when updated received over network
void DistObSys::updateDistOb(MyStream& uid_stream, MyStream& state_stream)
{

	//------------------------------------------------------------------------
	//update object in store
	//------------------------------------------------------------------------
	assert(store);

	//deserialise UID
	UID_REF uid = uid_factory->createUID();
	uid->deserialise(uid_stream);

	//see if object with this UID exists in store.
	DistObject* local_ob = store->getDistObjectForUID(uid);

	if(local_ob)//if exists already...
	{
		local_ob->deserialise(state_stream);//update local ob
	}
	else
	{
		//else create a new object with incoming state.

		//DistObject* newob = store->constructObFromStream(state_stream);
		//store->insertDistObject(newob);
		store->constructAndInsertFromStream(state_stream, true);
	}

	local_ob = store->getDistObjectForUID(uid);
	assert(local_ob);

	//------------------------------------------------------------------------
	//inform listener of received object update.
	//------------------------------------------------------------------------
	if(listener)
		listener->distObjectUpdated(local_ob);
}

//called by subclass when updated received over network
void DistObSys::destroyDistOb(const UID_REF& uid)
//void DistObSys::destroyDistOb(MyStream& uid_stream)
{
	//------------------------------------------------------------------------
	//remove object from store
	//------------------------------------------------------------------------
	assert(store);

	//deserialise UID
	//UID_REF uid = uid_factory->createUID();
	//(*uid).deserialise(uid_stream);

	//see if object with this UID exists in store.
	DistObject* object = store->getDistObjectForUID(uid);

	//assert(object);
	if(object)//if exists in store
	{
		//remove from store
		store->removeDistObject(uid);
	
		//------------------------------------------------------------------------
		//inform listener that object was destroyed
		//------------------------------------------------------------------------
		if(listener)
			listener->distObjectDestroyed(uid);

		//------------------------------------------------------------------------
		//delete the object
		//------------------------------------------------------------------------
		delete object;
	}


	//------------------------------------------------------------------------
	//remove object from update store as well.
	//------------------------------------------------------------------------
	if(update_store)
	{
		DistObject* update_object = update_store->getDistObjectForUID(uid);
		if(update_object)
		{
			update_store->removeDistObject(uid);
			delete update_object;
		}
	}

}







