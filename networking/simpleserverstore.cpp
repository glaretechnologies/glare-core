/*=====================================================================
simpleserverstore.cpp
---------------------
File created by ClassTemplate on Tue Sep 07 02:59:35 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "simpleserverstore.h"


#include "nonthreadedserver.h"
#include "testdistob.h"
#include "../networking/mystream.h"
#include <assert.h>


SimpleServerStore::SimpleServerStore()//DistObServer& server_)
//:	server(server_)
{
	server = NULL;	
}


SimpleServerStore::~SimpleServerStore()
{
	
}

void SimpleServerStore::tryDistObjectCreated(StreamSysClientProxy& client, 
											 MyStream& uid_stream, 
											 MyStream& state_stream)
{
	TestDistOb* incoming_ob = new TestDistOb();
	incoming_ob->deserialiseUID(uid_stream);
	incoming_ob->deserialise(state_stream);

	
	objects[incoming_ob->getUID()] = incoming_ob;
	
	assert(server);
	if(server)
	{
		server->createDistObject(*incoming_ob);
	}

}

void SimpleServerStore::tryDistObjectDestroyed(StreamSysClientProxy& client, MyStream& uid_stream)
{
	TestDistOb* incoming_ob = new TestDistOb();
	incoming_ob->deserialiseUID(uid_stream);
	
	objects.erase(incoming_ob->getUID());

	assert(server);
	if(server)
	{
		server->destroyDistObject(*incoming_ob);
	}
}

void SimpleServerStore::tryDistObjectStateChange(StreamSysClientProxy& client, 
												 MyStream& uid_stream, 
												 MyStream& state_stream)
{
	int uid;
	uid_stream >> uid;

	DistObject* current = objects[uid];
	current->deserialise(state_stream);

	assert(server);
	if(server)
	{
		server->makeStateChange(*current);
	}
}


void SimpleServerStore::getLocalObjects(std::vector<DistObject*>& local_objects_out)
{
	local_objects_out.resize(0);

	for(std::map<int, TestDistOb*>::iterator i = objects.begin(); i != objects.end(); ++i)
	{
		local_objects_out.push_back((*i).second);
	}
}






