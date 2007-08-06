/*=====================================================================
intobuidfactory.h
-----------------
File created by ClassTemplate on Mon Oct 11 17:24:01 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __INTOBUIDFACTORY_H_666_
#define __INTOBUIDFACTORY_H_666_



#include "distobuidfactory.h"


/*=====================================================================
IntObUIDFactory
---------------

=====================================================================*/
class IntObUIDFactory : public DistObUIDFactory
{
public:
	/*=====================================================================
	IntObUIDFactory
	---------------
	
	=====================================================================*/
	IntObUIDFactory();

	virtual ~IntObUIDFactory();



	virtual const UID_REF createUID();

};



#endif //__INTOBUIDFACTORY_H_666_




