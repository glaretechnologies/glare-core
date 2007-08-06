/*=====================================================================
distobstoreinterface.h
----------------------
File created by ClassTemplate on Wed Sep 15 03:34:19 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DISTOBSTOREINTERFACE_H_666_
#define __DISTOBSTOREINTERFACE_H_666_



class DistObject;
//class IntObUID;
class MyStream;
#include <vector>
#include "distobuid.h"

/*=====================================================================
DistObStoreInterface
--------------------
Interface to a class that provides storage for DistObjects.
=====================================================================*/
class DistObStoreInterface
{
public:
	/*=====================================================================
	DistObStoreInterface
	--------------------
	
	=====================================================================*/
	//DistObStoreInterface();

	virtual ~DistObStoreInterface(){}

	//virtual void insertDistObject(DistObject* ob) = 0;

	virtual void removeDistObject(const UID_REF uid) = 0;

	virtual DistObject* getDistObjectForUID(const UID_REF uid) = 0;


	//factory insert function
	//virtual DistObject* constructObFromStream(MyStream& state_stream) = 0;
	virtual void constructAndInsertFromStream(MyStream& state_stream,
															bool proper_object) = 0;

	virtual void getObjects(std::vector<DistObject*>& objects_out) = 0;

};



#endif //__DISTOBSTOREINTERFACE_H_666_




