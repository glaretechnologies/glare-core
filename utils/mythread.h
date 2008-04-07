/*=====================================================================
MyThread.h
----------
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __MYTHREAD_H_666_
#define __MYTHREAD_H_666_

#if defined(WIN32) || defined(WIN64)

// Stop windows.h from defining the min() and max() macros
#define NOMINMAX

#include <windows.h>
//#include "mutex.h"
#else
#include <pthread.h>
#endif

/*=====================================================================
MyThread
--------
Thanks to someone from the COTD on flipcode for some of this code
=====================================================================*/
class MyThread
{
public:
	/*=====================================================================
	MyThread
	--------
	=====================================================================*/
	MyThread();

	virtual ~MyThread();


	virtual void run() = 0;


	//HANDLE launch(bool autodelete = true);
	void launch(bool autodelete = true);

	//HANDLE getHandle(){ return thread_handle; }



	void killThread();

	//void waitForThread();

	bool commit_suicide;

	static int getNumAliveThreads();

private:

#if defined(WIN32) || defined(WIN64)
	static void 
#else
	static void*
#endif
		/*_cdecl*/ threadFunction(void* the_thread);


#if defined(WIN32) || defined(WIN64)
	HANDLE thread_handle;
#else
	 pthread_t thread_handle;
#endif
	bool autodelete;


	static void incrNumAliveThreads();
	static void decrNumAliveThreads();

	//static Mutex alivecount_mutex;
	static int num_alive_threads;
};



#endif //__MYTHREAD_H_666_




