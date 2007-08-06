/*=====================================================================
distobserver.h
--------------
File created by ClassTemplate on Sun Sep 05 20:26:26 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DISTOBSERVER_H_666_
#define __DISTOBSERVER_H_666_



#pragma warning(disable : 4786)//disable long debug name warning


#include <string>
class DistObServerListener;
class DistObject;
//class DistObClientProxy;
//#include "../utils/threadsafequeue.h"



class DistObServerExcep
{
public:
	//DistObServerExcep(){}
	DistObServerExcep(const std::string& message_) : message(message_) {}
	~DistObServerExcep(){}

	const std::string& what() const { return message; }
private:
	std::string message;
};

/*=====================================================================
DistObServer
------------
interface
NOTE USED RIGHT NOW
=====================================================================*/
class DistObServer
{
public:
	/*=====================================================================
	DistObServer
	------------
	
	=====================================================================*/
	DistObServer(); //throws DistObServerExcep if could not bind to port.

	virtual ~DistObServer();


	virtual void makeStateChange(DistObject& ob) = 0;

	virtual void createDistObject(DistObject& ob) = 0;
	virtual void destroyDistObject(DistObject& ob) = 0;


	virtual void think() = 0;//poll for events

	//virtual void addListener(DistObServerListener* listener) = 0;
	//virtual void removeListener(DistObServerListener* listener) = 0;

private:
	/*std::set<DistObServerListener*> listeners;

	std::set<DistObClientProxy*> clients;

	ThreadSafeQueue<DistObEvent*> ob_event_queue;
	ThreadSafeQueue<DistObCientEvent*> client_event_queue;*/
};



#endif //__DISTOBSERVER_H_666_




