/*=====================================================================
systemmutexlock.cpp
-------------------
File created by ClassTemplate on Fri Aug 27 19:23:01 2004
Code By Nicholas Chapman.
=====================================================================*/
#include "systemmutexlock.h"

#include <assert.h>


SystemMutexLock::SystemMutexLock(SystemMutex& system_mutex_)
:	system_mutex(system_mutex_)
{
	const int result = WaitForSingleObject(system_mutex.getMutexHandle(), INFINITE);

	assert(result != WAIT_FAILED);
}


SystemMutexLock::~SystemMutexLock()
{
	const bool result = ::ReleaseMutex(system_mutex.getMutexHandle());

	assert(result);
}


bool SystemMutexLock::couldLockMutex(SystemMutex& sys_mutex)
{
	
	const int result = WaitForSingleObject(sys_mutex.getMutexHandle(), 0);

	return result != WAIT_TIMEOUT;
}




