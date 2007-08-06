/*=====================================================================
intobuidfactory.cpp
-------------------
File created by ClassTemplate on Mon Oct 11 17:24:01 2004Code By Nicholas Chapman.
=====================================================================*/
#include "intobuidfactory.h"


#include "intobuid.h"

IntObUIDFactory::IntObUIDFactory()
{
	
}


IntObUIDFactory::~IntObUIDFactory()
{
	
}

const UID_REF IntObUIDFactory::createUID()
{
	return new IntObUID();
}





