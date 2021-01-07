/*=====================================================================
Port.cpp
--------
File created by ClassTemplate on Mon Jul 07 18:58:34 2003
Code By Nicholas Chapman.
=====================================================================*/
#include "Port.h"


#include "../utils/StringUtils.h"


Port::Port()
{
	port = 0;	
}


Port::Port(unsigned short port_)
:	port(port_)
{
	
}


Port::~Port()
{
	
}


const std::string Port::toString() const
{
	return ::toString((int)port);
}
