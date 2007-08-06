/*=====================================================================
stringuidfactory.h
------------------
File created by ClassTemplate on Wed Oct 27 07:02:20 2004Code By Nicholas Chapman.
=====================================================================*/
#ifndef __STRINGUIDFACTORY_H_666_
#define __STRINGUIDFACTORY_H_666_



#include "distobuidfactory.h"


/*=====================================================================
StringUIDFactory
----------------

=====================================================================*/
class StringUIDFactory : public DistObUIDFactory
{
public:
	/*=====================================================================
	StringUIDFactory
	----------------
	
	=====================================================================*/
	StringUIDFactory();

	virtual ~StringUIDFactory();


	virtual const UID_REF createUID();

};



#endif //__STRINGUIDFACTORY_H_666_




