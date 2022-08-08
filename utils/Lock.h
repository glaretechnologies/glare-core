/*=====================================================================
Lock.h
------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "ThreadSafetyAnalysis.h"
class Mutex;


/*=====================================================================
Lock
----
RAII style mutex locking/acquisition
=====================================================================*/
class SCOPED_CAPABILITY Lock
{
public:
	Lock(Mutex& mutex) ACQUIRE(mutex); // blocking

	~Lock() RELEASE();

	// Just for debugging
	const Mutex& getMutex() const { return mutex; }

private:
	GLARE_DISABLE_COPY(Lock);

	Mutex& mutex;
};
