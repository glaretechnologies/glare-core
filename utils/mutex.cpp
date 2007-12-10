/*=====================================================================
mutex.cpp
---------
File created by ClassTemplate on Wed Jul 24 13:24:30 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "mutex.h"


#include <assert.h>

/*#ifdef CYBERSPACE
#include "../cyberspace/globals.h"
#endif*/

Mutex::Mutex()
{
//	created = false;

#if defined(WIN32) || defined(WIN64)
	InitializeCriticalSection(&mutex);
#else
	const pthread_mutexattr_t mutex_attributes;
	int result = pthread_mutexattr_init(&mutex_attributes);
	assert(result == 0);
	pthread_mutexattr_init.type = PTHREAD_MUTEX_RECURSIVE;

	result = pthread_mutex_init(&mutex, NULL);
	assert(result == 0);
#endif
//	created = true;
}

Mutex::~Mutex()
{
//	created = false;

#if defined(WIN32) || defined(WIN64)
	DeleteCriticalSection(&mutex);
#else
	pthread_mutex_destroy(&mutex);//, NULL);
#endif
}

void Mutex::acquire()
{
//	assert(created);

#if defined(WIN32) || defined(WIN64)
	EnterCriticalSection(&mutex);
#else
	pthread_mutex_lock(&mutex);
#endif
}

void Mutex::release()
{	
//	assert(created);

#if defined(WIN32) || defined(WIN64)
	LeaveCriticalSection(&mutex);
#else
	pthread_mutex_unlock(&mutex);
#endif
}


