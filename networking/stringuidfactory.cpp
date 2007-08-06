/*=====================================================================
stringuidfactory.cpp
--------------------
File created by ClassTemplate on Wed Oct 27 07:02:20 2004Code By Nicholas Chapman.
=====================================================================*/
#include "stringuidfactory.h"


#include "stringuid.h"


StringUIDFactory::StringUIDFactory()
{
	
}


StringUIDFactory::~StringUIDFactory()
{
	
}

const UID_REF StringUIDFactory::createUID()
{
	return new StringUID();
}






