/*=====================================================================
p2psys.h
--------
File created by ClassTemplate on Fri Sep 10 03:12:31 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __P2PSYS_H_666_
#define __P2PSYS_H_666_


#pragma warning(disable : 4786)//disable long debug name warning


#include "distobsys.h"

#include "../cyberspace/peerid.h"
class StateSender;
class DaemonNotifier;
class PTIPSender;
class UDPHandler;

/*=====================================================================
P2PSys
------
peer to peer distributed object system.
=====================================================================*/
class P2PSys : public DistObSys
{
public:
	/*=====================================================================
	P2PSys
	------
	
	=====================================================================*/
	P2PSys(DistObListener* listener);

	virtual ~P2PSys();

	////////DistObSys interface/////////////
	virtual void connect(const std::string& hostname, int port);
	virtual void disconnect();


	virtual void tryStateChange(DistObject& ob);

	virtual void tryCreateDistObject(DistObject& ob);
	virtual void tryDestroyDistObject(DistObject& ob);

	virtual void think();//poll for events
	///////////////////////////////////////



private:
	StateSender* statesender;
	DaemonNotifier* daemon_notifier;
	PTIPSender* ptip_sender;

	UDPHandler* udp_handler;

	PeerID init_id;
};



#endif //__P2PSYS_H_666_




