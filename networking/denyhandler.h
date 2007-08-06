/*=====================================================================
denyhandler.h
-------------
File created by ClassTemplate on Wed Oct 27 07:26:38 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DENYHANDLER_H_666_
#define __DENYHANDLER_H_666_


#include "distobserverlistener.h"



/*=====================================================================
DenyHandler
-----------

=====================================================================*/
class DenyHandler : public DistObServerListener
{
public:
	/*=====================================================================
	DenyHandler
	-----------
	
	=====================================================================*/
	DenyHandler();

	virtual ~DenyHandler();


	virtual void tryDistObjectDestroyed(NonThreadedServer& server, 
		StreamSysClientProxy& client, const UID_REF& uid);

	virtual void tryDistObjectStateChange(NonThreadedServer& server, 
		StreamSysClientProxy& client, const UID_REF& uid, MyStream& state_stream);




};



#endif //__DENYHANDLER_H_666_




