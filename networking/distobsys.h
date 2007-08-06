/*=====================================================================
distobsys.h
-----------
File created by ClassTemplate on Fri Sep 03 04:46:37 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DISTOBSYS_H_666_
#define __DISTOBSYS_H_666_



//class IntObUID;
#include "distobuid.h"
#include <string>
class DistObject;
class DistObjectState;
class DistObListener;
class DistObStoreInterface;
class MyStream;
class DistObUIDFactory;


class DistObSysExcep
{
public:
	//DistObSysExcep(){}
	DistObSysExcep(const std::string& message_) : message(message_) {}
	~DistObSysExcep(){}

	const std::string& what() const { return message; }
private:
	std::string message;
};

/*=====================================================================
DistObSys
---------
client/peer interface for a distributed object system

P2P/Client-server agnostic.
=====================================================================*/
class DistObSys
{
public:
	/*=====================================================================
	DistObSys
	---------
	listener may be NULL.

	=====================================================================*/
	DistObSys(DistObListener* listener, DistObStoreInterface* update_store,
										DistObStoreInterface* store,
										DistObUIDFactory* uid_factory);

	virtual ~DistObSys();


	//to be called from a client: call these after updates are made to the update store.
	virtual void tryCreateDistOb(DistObject& ob) = 0;
	virtual void tryUpdateDistOb(DistObject& ob) = 0;
	virtual void tryDestroyDistOb(DistObject& ob) = 0;



	virtual void think(double time) = 0;	
	//poll for events.  throws DistObSysExcep if disconnected.



	//virtual void addListener(DistObListener* listener) = 0;
	//virtual void removeListener(DistObListener* listener) = 0;

protected:
	//DistObListener* getListener(){ return listener; }

	DistObStoreInterface* getUpdateStore(){ return update_store; }

	//to be called by subclass when update received over network
	void updateDistOb(MyStream& uid_stream, MyStream& state_stream);
	void destroyDistOb(const UID_REF& uid);


//	virtual void doUpdateReceived(const UID_REF uid){}

	DistObUIDFactory& getUIDFactory(){ return *uid_factory; }
private:
	DistObListener* listener;
	DistObStoreInterface* store;
	DistObStoreInterface* update_store;
	DistObUIDFactory* uid_factory;
};



#endif //__DISTOBSYS_H_666_




