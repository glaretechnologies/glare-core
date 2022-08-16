/*=====================================================================
Mutex.cpp
---------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "Mutex.h"


#include <assert.h>


Mutex::Mutex()
{
#if defined(_WIN32)
	InitializeCriticalSection(&mutex);
#else
	// Initialise mutex attributes
	pthread_mutexattr_t mutex_attributes;
	int result = pthread_mutexattr_init(&mutex_attributes);
	assertOrDeclareUsed(result == 0);
	result = pthread_mutexattr_settype(&mutex_attributes, PTHREAD_MUTEX_RECURSIVE);
	assertOrDeclareUsed(result == 0);

	// Initialise mutex
	result = pthread_mutex_init(&mutex, &mutex_attributes);
	assertOrDeclareUsed(result == 0);
	
	// Destroy mutex attributes
	result = pthread_mutexattr_destroy(&mutex_attributes);
	assertOrDeclareUsed(result == 0);
#endif
}


Mutex::~Mutex()
{
#if defined(_WIN32)
	DeleteCriticalSection(&mutex);
#else
	int result = pthread_mutex_destroy(&mutex);
	assertOrDeclareUsed(result == 0);
#endif
}


void Mutex::acquire()
{
#if defined(_WIN32)
	EnterCriticalSection(&mutex);
#else
	int result = pthread_mutex_lock(&mutex);
	assertOrDeclareUsed(result == 0);
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
	int result = pthread_mutex_unlock(&mutex);
	assertOrDeclareUsed(result == 0);
#endif
}
