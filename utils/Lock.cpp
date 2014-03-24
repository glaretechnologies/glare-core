/*=====================================================================
Lock.cpp
--------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Wed Jul 24 13:28:21 2002
=====================================================================*/
#include "Lock.h"


#include "Mutex.h"


Lock::Lock(Mutex& mutex_)
:	mutex(mutex_)
{
	mutex.acquire();
}


Lock::~Lock()
{
	mutex.release();
}
