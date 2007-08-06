/*=====================================================================
systemmutexlock.h
-----------------
File created by ClassTemplate on Fri Aug 27 19:23:01 2004
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __SYSTEMMUTEXLOCK_H_666_
#define __SYSTEMMUTEXLOCK_H_666_



#include "systemmutex.h"

/*=====================================================================
SystemMutexLock
---------------

=====================================================================*/
class SystemMutexLock
{
public:
	/*=====================================================================
	SystemMutexLock
	---------------
	
	=====================================================================*/
	SystemMutexLock(SystemMutex& system_mutex);

	~SystemMutexLock();

	static bool couldLockMutex(SystemMutex& sys_mutex);


private:
	SystemMutex& system_mutex;
};



#endif //__SYSTEMMUTEXLOCK_H_666_




