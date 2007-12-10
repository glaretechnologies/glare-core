/*=====================================================================
mutex.cpp
---------
File created by ClassTemplate on Wed Jul 24 13:24:30 2002
Code By Nicholas Chapman.
=====================================================================*/
#include "mutex.h"


#include <assert.h>


Mutex::Mutex()
{
//	created = false;

#if defined(WIN32) || defined(WIN64)
	InitializeCriticalSection(&mutex);
#else
	pthread_mutexattr_t mutex_attributes;
	int result = pthread_mutexattr_init(&mutex_attributes);
	assert(result == 0);
	result = pthread_mutexattr_settype(&mutex_attributes, PTHREAD_MUTEX_RECURSIVE);
	assert(result == 0);

	result = pthread_mutex_init(&mutex, &mutex_attributes);
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


