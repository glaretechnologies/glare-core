/*=====================================================================
simpleclientstore.cpp
---------------------
File created by ClassTemplate on Tue Sep 07 05:01:57 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "simpleclientstore.h"

#include "testdistob.h"
#include "../networking/mystream.h"
#include <iostream>

SimpleClientStore::SimpleClientStore()
{
	
}


SimpleClientStore::~SimpleClientStore()
{
	
}


void SimpleClientStore::distObjectCreated(MyStream& uid_stream, MyStream& state_stream)
{
	TestDistOb* incoming_ob = new TestDistOb();
	incoming_ob->deserialiseUID(uid_stream);
	incoming_ob->deserialise(state_stream);

	objects[incoming_ob->getUID()] = incoming_ob;


	std::cout << "added new object to client store, UID: " << incoming_ob->getUID() << 
		", payload: " << incoming_ob->getPayload() << std::endl;

}
void SimpleClientStore::distObjectDestroyed(MyStream& uid_stream)
{
	TestDistOb* incoming_ob = new TestDistOb();
	incoming_ob->deserialiseUID(uid_stream);
	
	objects.erase(incoming_ob->getUID());

}

void SimpleClientStore::distObjectStateChange(MyStream& uid_stream, 
											  MyStream& state_stream)
{

	int uid;
	uid_stream >> uid;

	DistObject* current = objects[uid];
	current->deserialise(state_stream);

}

void SimpleClientStore::getLocalObjects(std::vector<DistObject*>& local_objects_out)
{
	local_objects_out.resize(0);

	for(std::map<int, TestDistOb*>::iterator i = objects.begin(); i != objects.end(); ++i)
	{
		local_objects_out.push_back((*i).second);
	}

}




