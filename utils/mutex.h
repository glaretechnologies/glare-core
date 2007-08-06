/*=====================================================================
mutex.h
-------
File created by ClassTemplate on Wed Jul 24 13:24:30 2002
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MUTEX_H_666_
#define __MUTEX_H_666_


#ifdef WIN32
#include <windows.h>
#include <process.h>
#else
#include <pthread.h>
#endif

/*=====================================================================
Mutex
-----

=====================================================================*/
class Mutex
{
public:
	/*=====================================================================
	Mutex
	-----
	
	=====================================================================*/
	Mutex();

	~Mutex();

	void acquire();
	void release();

//private:
#ifdef WIN32
	CRITICAL_SECTION mutex;
#else
	pthread_mutex_t mutex;
#endif
	//TEMP DEBUG:
//	bool created;
};



#endif //__MUTEX_H_666_




