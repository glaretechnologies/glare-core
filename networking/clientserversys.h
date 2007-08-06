/*=====================================================================
clientserversys.h
-----------------
File created by ClassTemplate on Sat Sep 04 12:55:04 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __CLIENTSERVERSYS_H_666_
#define __CLIENTSERVERSYS_H_666_

#pragma warning(disable : 4786)//disable long debug name warning


//#include "mysocket.h"
//class MySocket;
//class PollSocket;
//class PacketStream;
//#include "intobuid.h"
//#include "distobuid.h"
#include "distobsys.h"
#include "streamlistener.h"
#include <set>
class ClientServerStreamSys;
class CSMessage;
class DistObStoreInterface;
class DistObUIDFactory;

/*=====================================================================
ClientServerSys
---------------
concrete class.
Implements a client for the client server distributed object protocol.

TODO: change name
=====================================================================*/
class ClientServerSys : public DistObSys, public StreamListener
{
public:
	/*=====================================================================
	ClientServerSys
	---------------
	listener may be NULL
	update_store may be NULL
	=====================================================================*/
	ClientServerSys(DistObListener* listener, ClientServerStreamSys* cs_stream_sys,
						int stream_uid,
						DistObStoreInterface* update_store,
						DistObStoreInterface* store,
						DistObUIDFactory* uid_factory,
						double update_push_period);

	virtual ~ClientServerSys();



	//DistObSys interface:
	//to be called from a client: call these after updates are made to the update store.
	virtual void tryCreateDistOb(DistObject& ob);
	virtual void tryUpdateDistOb(DistObject& ob);
	virtual void tryDestroyDistOb(DistObject& ob);


	virtual void think(double time);

	//virtual void addListener(DistObListener* listener);
	//virtual void removeListener(DistObListener* listener);



	//StreamListener interface:
	//handle an incoming packet from the stream server on the DistOb stream
	virtual void handlePacket(int stream_uid, Packet& packet);


protected:
//	virtual void doUpdateReceived(const UID_REF uid);

private:
	void processMessage(CSMessage& msg);

	double update_push_period;
	double last_push_time;

	ClientServerStreamSys* cs_stream_sys;

	int stream_uid;

	std::set<UID_REF> updated_objects;
	std::set<UID_REF> created_objects;
};



#endif //__CLIENTSERVERSYS_H_666_




