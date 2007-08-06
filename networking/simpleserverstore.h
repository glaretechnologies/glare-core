/*=====================================================================
simpleserverstore.h
-------------------
File created by ClassTemplate on Tue Sep 07 02:59:35 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __SIMPLESERVERSTORE_H_666_
#define __SIMPLESERVERSTORE_H_666_


#pragma warning(disable : 4786)//disable long debug name warning


#include "distobserverlistener.h"
class NonThreadedServer;
#include <map>
class TestDistOb;

/*=====================================================================
SimpleServerStore
-----------------

=====================================================================*/
class SimpleServerStore : public DistObServerListener
{
public:
	/*=====================================================================
	SimpleServerStore
	-----------------
	
	=====================================================================*/
	SimpleServerStore();
	virtual ~SimpleServerStore();



	virtual void tryDistObjectCreated(StreamSysClientProxy& client, MyStream& uid_stream, MyStream& state_stream);
	virtual void tryDistObjectDestroyed(StreamSysClientProxy& client, MyStream& uid_stream);
	virtual void tryDistObjectStateChange(StreamSysClientProxy& client, MyStream& uid_stream, MyStream& state_stream);


//	virtual DistObject& getDistObject(const DistObUID& ob_uid) = 0;

	//used to dump state to new clients.
	virtual void getLocalObjects(std::vector<DistObject*>& local_objects_out);

	void setServer(NonThreadedServer* server_){ server = server_; }

private:
	NonThreadedServer* server;

	std::map<int, TestDistOb*> objects;
};



#endif //__SIMPLESERVERSTORE_H_666_




