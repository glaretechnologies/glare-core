/*=====================================================================
MyThread.cpp
------------
Code By Nicholas Chapman.
=====================================================================*/
#include "mythread.h"


#include <cassert>
#if defined(WIN32) || defined(WIN64)
// Stop windows.h from defining the min() and max() macros
#define NOMINMAX
#include <windows.h>
#include <process.h>
#endif
#include "stringutils.h"
#include <iostream>


MyThread::MyThread()
{
	thread_handle = 0;
	autodelete = false;
}


MyThread::~MyThread()
{
}


void MyThread::launch(bool autodelete_)
{
	assert(thread_handle == NULL);

	autodelete = autodelete_;

#if defined(WIN32) || defined(WIN64)
	thread_handle = (HANDLE)_beginthread(
		threadFunction,
		0, // Stack size
		this // arglist
		);
#else
	pthread_create(
		&thread_handle, // Thread
		NULL, // attr
		threadFunction, // start routine
		this // arg
		);
#endif
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

	the_thread->run();

	if(the_thread->autodelete)
		delete the_thread;

#if !(defined(WIN32) || defined(WIN64))
	return NULL;
#endif
}


void MyThread::join() // Wait for thread termination
{
#if defined(WIN32) || defined(WIN64)
	const DWORD result = ::WaitForSingleObject(thread_handle, INFINITE);
#else
	const int result = pthread_join(thread_handle, NULL);
#endif
}


void MyThread::setPriority(Priority p)
{
#if defined(WIN32) || defined(WIN64)
	int pri = THREAD_PRIORITY_NORMAL;
	if(p == Priority_Normal)
		pri = THREAD_PRIORITY_NORMAL;
	else if(p == Priority_BelowNormal)
		pri = THREAD_PRIORITY_BELOW_NORMAL;
	else
	{
		assert(0);
	}
	const BOOL res = ::SetThreadPriority(thread_handle, pri);
	if(res == 0)
		throw MyThreadExcep("SetThreadPriority failed: " + toString((unsigned int)GetLastError()));
#else
	throw MyThreadExcep("Can't change thread priority after creation on Linux or OS X");
	/*// Get current priority
	int policy;
	sched_param param;
	const int ret = pthread_getschedparam(thread_handle, &policy, &param);
	if(ret != 0)
		throw MyThreadExcep("pthread_getschedparam failed: " + toString(ret));
	std::cout << "param.sched_priority: " << param.sched_priority << std::endl;

	std::cout << "SCHED_OTHER: " << SCHED_OTHER << std::endl;
	std::cout << "policy: " << policy << std::endl;


	//sched_param param;
	param.sched_priority = -1;
	const int res = pthread_setschedparam(thread_handle, policy, &param);
	if(res != 0)
		throw MyThreadExcep("pthread_setschedparam failed: " + toString(res));*/
#endif
}
