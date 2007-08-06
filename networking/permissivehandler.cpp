/*=====================================================================
permissivehandler.cpp
---------------------
File created by ClassTemplate on Wed Sep 15 05:22:01 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "permissivehandler.h"


#include "nonthreadedserver.h"

PermissiveHandler::PermissiveHandler()
{
	
}


PermissiveHandler::~PermissiveHandler()
{
	
}



void PermissiveHandler::tryDistObjectDestroyed(NonThreadedServer& server, 
		StreamSysClientProxy& client, const UID_REF& uid)
{
	server.destroyDistObject(uid);
}

void PermissiveHandler::tryDistObjectStateChange(NonThreadedServer& server, 
		StreamSysClientProxy& client, const UID_REF& uid, MyStream& state_stream)
{
	server.makeStateChange(uid, state_stream);
}


