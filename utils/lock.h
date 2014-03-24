/*=====================================================================
lock.h
------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Wed Jul 24 13:28:21 2002
=====================================================================*/
#pragma once


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
