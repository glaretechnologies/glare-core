/*=====================================================================
Mutex.h
-------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Wed Jul 24 13:24:30 2002
=====================================================================*/
#pragma once


#if defined(_WIN32)
#include "IncludeWindows.h"
#include <process.h>
#else
#include <pthread.h>
#endif
#include "Platform.h"


/*=====================================================================
Mutex
-----

=====================================================================*/
class Mutex
{
public:
	Mutex();
	~Mutex();

	void acquire();
	void release();

//private:
#if defined(_WIN32)
	CRITICAL_SECTION mutex;
#else
	pthread_mutex_t mutex;
#endif

private:
	// Mutex is non-copyable, as copying the CRITICAL_SECTION structure causes crashes.
	INDIGO_DISABLE_COPY(Mutex);
};
