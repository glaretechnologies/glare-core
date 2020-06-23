/*=====================================================================
Lock.cpp
--------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Wed Jul 24 13:28:21 2002
=====================================================================*/
#include "TryLock.h"


#include "Mutex.h"


TryLock::TryLock(Mutex& mutex_)
:	mutex(mutex_)
{
	acquired = mutex.tryAcquire();
}


TryLock::~TryLock()
{
	if(acquired) mutex.release();
}
