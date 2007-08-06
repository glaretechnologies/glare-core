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

#ifdef WIN32
	InitializeCriticalSection(&mutex);
#else
	//int pthread_mutex_init (mutex, attr)
	//pthread_mutex_t *mutex;
	//pthread_mutexattr_t *attr;
	pthread_mutex_init(&mutex, NULL);
#endif
//	created = true;
}

Mutex::~Mutex()
{
//	created = false;

#ifdef WIN32
	DeleteCriticalSection(&mutex);
#else
	pthread_mutex_destroy(&mutex);//, NULL);
#endif
}

void Mutex::acquire()
{
/*	assert(created);
#ifdef CYBERSPACE
	if(!created)
		::fatalError("tried to acquire an uninitialised mutex.");
#endif*/

#ifdef WIN32
	EnterCriticalSection(&mutex);
#else
	pthread_mutex_lock(&mutex);
#endif
}

void Mutex::release()
{	
/*	assert(created);
#ifdef CYBERSPACE
	if(!created)
		::fatalError("tried to release an uninitialised mutex.");
#endif*/

#ifdef WIN32
	LeaveCriticalSection(&mutex);
#else
	pthread_mutex_unlock(&mutex);
#endif
}


