/*=====================================================================
distoblistener.h
----------------
File created by ClassTemplate on Fri Sep 03 04:54:56 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DISTOBLISTENER_H_666_
#define __DISTOBLISTENER_H_666_



class DistObject;
class MyStream;
//class DistObUID;
//class IntObUID;
#include <vector>
#include "distobuid.h"

/*=====================================================================
DistObListener
--------------
Interface for clients to receive events about distributed objects being
created/changed/destroyed.
=====================================================================*/
class DistObListener
{
public:
	/*=====================================================================
	DistObListener
	--------------
	
	=====================================================================*/
	DistObListener(){}

	virtual ~DistObListener(){}

	//void changeFailed();

	/*virtual void distObjectCreated(const DistObUID& ob_uid, MyStream& stream) = 0;
	virtual void distObjectDestroyed(const DistObUID& ob_uid) = 0;

	virtual void distObjectStateChange(const DistObUID& ob_uid, MyStream& stream) = 0;*/

	//throw MyStreamExcep on parse failure, defined in mystream.h
	//virtual void distObjectCreated(MyStream& uid_stream, MyStream& state_stream) = 0;
	virtual void distObjectDestroyed(const UID_REF uid) = 0;
	virtual void distObjectUpdated(DistObject* ob) = 0;

	//virtual void getLocalObjects(std::vector<DistObject*>& local_objects_out) = 0;
};



#endif //__DISTOBLISTENER_H_666_




