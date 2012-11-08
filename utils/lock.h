/*=====================================================================
lock.h
------
File created by ClassTemplate on Wed Jul 24 13:28:21 2002
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __LOCK_H_666_
#define __LOCK_H_666_


#include "platform.h"
class Mutex;


/*=====================================================================
Lock
----
RAII style mutex locking/acquisition
=====================================================================*/
class Lock
{
public:
	/*=====================================================================
	Lock
	----
	
	=====================================================================*/
	Lock(Mutex& mutex); // blocking

	~Lock();

	// Just for debugging
	const Mutex& getMutex() const { return mutex; }

private:
	INDIGO_DISABLE_COPY(Lock);

	Mutex& mutex;
};



#endif //__LOCK_H_666_




