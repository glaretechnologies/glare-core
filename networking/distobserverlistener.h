/*=====================================================================
distobserverlistener.h
----------------------
File created by ClassTemplate on Sun Sep 05 20:31:14 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DISTOBSERVERLISTENER_H_666_
#define __DISTOBSERVERLISTENER_H_666_



//class IntObUID;
//class DistObject;
#include "distobuid.h"
class MyStream;
class StreamSysClientProxy;
class NonThreadedServer;
#include <vector>

/*=====================================================================
DistObServerListener
--------------------
Interface to be informed of updates to distributed objects requested by clients.
=====================================================================*/
class DistObServerListener
{
public:
	/*=====================================================================
	DistObServerListener
	--------------------
	
	=====================================================================*/
	//DistObServerListener();

	virtual ~DistObServerListener(){}

	//client send message to destroy object
	virtual void tryDistObjectDestroyed(NonThreadedServer& server, 
		StreamSysClientProxy& client, const UID_REF& uid) = 0;

	//client set message to create/alter object
	virtual void tryDistObjectStateChange(NonThreadedServer& server, 
		StreamSysClientProxy& client, const UID_REF& uid, MyStream& state_stream) = 0;
	
	
	//TEMP need to provide more info here later


	//virtual void clientConnected(DistObClientProxy& client){};
	//virtual void clientDisconnected(DistObClientProxy& client){};


/*	virtual void tryDistObjectCreated(DistObClientProxy& client, const DistObUID& ob_uid, MyStream& stream) = 0;
	virtual void tryDistObjectDestroyed(DistObClientProxy& client, const DistObUID& ob_uid) = 0;
	virtual void tryDistObjectStateChange(DistObClientProxy& client, const DistObUID& ob_uid, MyStream& stream) = 0;
*/
/*	virtual void tryDistObjectCreated(DistObClientProxy& client, MyStream& uid_stream, MyStream& state_stream) = 0;
	virtual void tryDistObjectDestroyed(DistObClientProxy& client, MyStream& uid_stream) = 0;
	virtual void tryDistObjectStateChange(DistObClientProxy& client, MyStream& uid_stream, MyStream& state_stream) = 0;
*/
	//virtual void tryDistObjectCreated(StreamSysClientProxy& client, MyStream& uid_stream, MyStream& state_stream) = 0;
//	virtual void tryDistObjectDestroyed(NonThreadedServer& server, StreamSysClientProxy& client, MyStream& uid_stream) = 0;
//	virtual void tryDistObjectStateChange(NonThreadedServer& server, StreamSysClientProxy& client, MyStream& uid_stream, MyStream& state_stream) = 0;
	


//	virtual DistObject& getDistObject(const DistObUID& ob_uid) = 0;

	//used to dump state to new clients.
	//virtual void getLocalObjects(std::vector<DistObject*>& local_objects_out) = 0;

};



#endif //__DISTOBSERVERLISTENER_H_666_




