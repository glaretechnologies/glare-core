/*=====================================================================
Port.cpp
--------
Copyright Glare Technologies Limited 2020 -
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
