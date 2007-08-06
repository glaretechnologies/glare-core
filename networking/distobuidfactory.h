/*=====================================================================
distobuidfactory.h
------------------
File created by ClassTemplate on Mon Oct 11 14:59:20 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __DISTOBUIDFACTORY_H_666_
#define __DISTOBUIDFACTORY_H_666_


#include "distobuid.h"


/*=====================================================================
DistObUIDFactory
----------------

=====================================================================*/
class DistObUIDFactory
{
public:
	/*=====================================================================
	DistObUIDFactory
	----------------
	
	=====================================================================*/
	//DistObUIDFactory();

	virtual ~DistObUIDFactory(){}
	

	virtual const UID_REF createUID() = 0;

};



#endif //__DISTOBUIDFACTORY_H_666_




