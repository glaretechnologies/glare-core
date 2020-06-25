/*=====================================================================
Mutex.cpp
---------
Copyright Glare Technologies Limited 2014 -
File created by ClassTemplate on Wed Jul 24 13:24:30 2002
=====================================================================*/
#include "Mutex.h"


#include <assert.h>


Mutex::Mutex()
{
#if defined(_WIN32)
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
}


Mutex::~Mutex()
{
#if defined(_WIN32)
	DeleteCriticalSection(&mutex);
#else
	pthread_mutex_destroy(&mutex);//, NULL);
#endif
}


void Mutex::acquire()
{
#if defined(_WIN32)
	EnterCriticalSection(&mutex);
#else
	pthread_mutex_lock(&mutex);
#endif
}


bool Mutex::tryAcquire()
{
#if defined(_WIN32)
	return TryEnterCriticalSection(&mutex) != 0;
#else
	return pthread_mutex_trylock(&mutex) == 0;
#endif
}


void Mutex::release()
{	
#if defined(_WIN32)
	LeaveCriticalSection(&mutex);
#else
	pthread_mutex_unlock(&mutex);
#endif
}
