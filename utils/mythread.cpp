/*=====================================================================
MyThread.cpp
------------
Code By Nicholas Chapman.
=====================================================================*/
#include "mythread.h"

#include <assert.h>
#if defined(WIN32) || defined(WIN64)

// Stop windows.h from defining the min() and max() macros
#define NOMINMAX

#include <windows.h>
#include <process.h>


//TEMP HACK:
//unsigned long _beginthread( void( __cdecl *start_address )( void * ),
//		unsigned stack_size, void *arglist );
		
#endif
//#include "lock.h"

MyThread::MyThread()
{
	thread_handle = 0;//NULL;
	autodelete = false;
	commit_suicide = false;
}


MyThread::~MyThread()
{
	
}



void MyThread::launch(bool autodelete_)
{
	assert(thread_handle == NULL);

	autodelete = autodelete_;


#if defined(WIN32) || defined(WIN64)

	const int stacksize = 0;//TEMP HACK

	thread_handle = (HANDLE)_beginthread(threadFunction, stacksize, this);

#else
	//int pthread_create (pthread_t *thread, const pthread_attr_t *attr, void
	//*(*start_routine) (void), void *arg) ;
	pthread_create(&thread_handle, NULL, threadFunction, this);
#endif

	//return thread_handle;
}

#if defined(WIN32) || defined(WIN64)
	void 
#else
	void*
#endif
/*_cdecl*/ MyThread::threadFunction(void* the_thread_)
{
	MyThread* the_thread = static_cast<MyThread*>(the_thread_);

	assert(the_thread != NULL);

	MyThread::incrNumAliveThreads();

	the_thread->run();

	if(the_thread->autodelete)
		delete the_thread;

	MyThread::decrNumAliveThreads();

#if !(defined(WIN32) || defined(WIN64))
	return NULL;
#endif
}

void MyThread::killThread()
{
	//_endthread(
}

int MyThread::getNumAliveThreads()
{
	//Lock lock(alivecount_mutex);

	return num_alive_threads;
}

void MyThread::incrNumAliveThreads()
{
	num_alive_threads++;
}

void MyThread::decrNumAliveThreads()
{
	//Lock lock(alivecount_mutex);

	num_alive_threads--;
	
	assert(num_alive_threads >= 0);
}

/*void MyThread::waitForThread()
{
	const DWORD result = WaitForSingleObject(
		thread_handle,
		INFINITE // time out interval
		);

	assert(result != WAIT_FAILED);
}*/

//Mutex MyThread::alivecount_mutex;
int MyThread::num_alive_threads = 0;
