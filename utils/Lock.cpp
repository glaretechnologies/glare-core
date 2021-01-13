/*=====================================================================
Lock.cpp
--------
Copyright Glare Technologies Limited 2021 -
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
