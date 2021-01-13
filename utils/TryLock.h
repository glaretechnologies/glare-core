/*=====================================================================
TryLock.h
------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Wed Jul 24 13:28:21 2002
=====================================================================*/
#pragma once


#include "Platform.h"
class Mutex;


/*=====================================================================
TryLock
----
non blocking RAII style mutex locking/acquisition
=====================================================================*/
class TryLock
{
public:
	/*=====================================================================
	TryLock
	----
	
	=====================================================================*/
	TryLock(Mutex& mutex);

	~TryLock();

	const bool isLocked() const { return acquired; }

	// Just for debugging
	const Mutex& getMutex() const { return mutex; }

private:
	GLARE_DISABLE_COPY(TryLock);

	Mutex& mutex;
	bool acquired;
};
