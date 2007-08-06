/*=====================================================================
denyhandler.cpp
---------------
File created by ClassTemplate on Wed Oct 27 07:26:38 2004Code By Nicholas Chapman.
=====================================================================*/
#include "denyhandler.h"




DenyHandler::DenyHandler()
{
	
}


DenyHandler::~DenyHandler()
{
	
}



void DenyHandler::tryDistObjectDestroyed(NonThreadedServer& server, 
		StreamSysClientProxy& client, const UID_REF& uid)
{
	//just do nothing
}

void DenyHandler::tryDistObjectStateChange(NonThreadedServer& server, 
		StreamSysClientProxy& client, const UID_REF& uid, MyStream& state_stream)
{
	//just do nothing
}




