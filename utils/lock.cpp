/*=====================================================================
lock.cpp
--------
File created by ClassTemplate on Wed Jul 24 13:28:21 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "lock.h"


#include "mutex.h"

Lock::Lock(Mutex& mutex_)
:	mutex(mutex_)
{
	mutex.acquire();
}


Lock::~Lock()
{
	mutex.release();
}






