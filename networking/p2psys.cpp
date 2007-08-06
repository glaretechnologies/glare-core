/*=====================================================================
p2psys.cpp
----------
File created by ClassTemplate on Fri Sep 10 03:12:31 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "p2psys.h"




P2PSys::P2PSys(DistObListener* listener)
:	DistObSys(listener)
{
	
}


P2PSys::~P2PSys()
{
	
}


void P2PSys::connect(const std::string& hostname, int port)
{

}

void P2PSys::disconnect()
{

}

void P2PSys::tryStateChange(DistObject& ob)
{


}
void P2PSys::tryCreateDistObject(DistObject& ob)
{


}

void P2PSys::tryDestroyDistObject(DistObject& ob)
{

}

void P2PSys::think()//poll for events
{

}



