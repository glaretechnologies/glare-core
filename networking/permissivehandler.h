/*=====================================================================
permissivehandler.h
-------------------
File created by ClassTemplate on Wed Sep 15 05:22:01 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __PERMISSIVEHANDLER_H_666_
#define __PERMISSIVEHANDLER_H_666_



#include "distobserverlistener.h"
#include "distobuid.h"


/*=====================================================================
PermissiveHandler
-----------------
implementation of the event handler that allows any change from any client.
=====================================================================*/
class PermissiveHandler : public DistObServerListener
{
public:
	/*=====================================================================
	PermissiveHandler
	-----------------
	
	=====================================================================*/
	PermissiveHandler();

	virtual ~PermissiveHandler();


	virtual void tryDistObjectDestroyed(NonThreadedServer& server, 
		StreamSysClientProxy& client, const UID_REF& uid);

	virtual void tryDistObjectStateChange(NonThreadedServer& server, 
		StreamSysClientProxy& client, const UID_REF& uid, MyStream& state_stream);


};



#endif //__PERMISSIVEHANDLER_H_666_




