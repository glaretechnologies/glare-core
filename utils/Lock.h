/*=====================================================================
Lock.h
------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
class Mutex;


/*=====================================================================
Lock
----
RAII style mutex locking/acquisition
=====================================================================*/
class Lock
{
public:
	Lock(Mutex& mutex); // blocking

	~Lock();

	// Just for debugging
	const Mutex& getMutex() const { return mutex; }

private:
	INDIGO_DISABLE_COPY(Lock);

	Mutex& mutex;
};
