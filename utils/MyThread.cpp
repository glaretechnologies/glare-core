/*=====================================================================
MyThread.cpp
------------
Copyright Glare Technologies Limited 2014 -
=====================================================================*/
#include "MyThread.h"


#include "StringUtils.h"
#include <cassert>
#if defined(_WIN32)
#include <process.h>
#else
#include <errno.h>
#endif


MyThread::MyThread()
{
	thread_handle = 0;
	joined = false;
}


MyThread::~MyThread()
{
#if defined(_WIN32)
	if(!CloseHandle(thread_handle))
	{
		assert(0);
		//std::cerr << "ERROR: CloseHandle on thread failed." << std::endl;
	}
#else
	// Mark the thread resources as freeable if needed.
	// Each pthread must either have pthread_join() or pthread_detach() call on it.
	// So we'll call pthread_detach() only if we haven't joined it.
	// See: http://www.kernel.org/doc/man-pages/online/pages/man3/pthread_detach.3.html
	if(!joined)
	{
		
		const int result = pthread_detach(thread_handle);
		assert(result == 0);
	}
#endif
}


#if defined(_WIN32)
	static unsigned int __stdcall
#else
	void*
#endif
threadFunction(void* the_thread_)
{
	MyThread* the_thread = static_cast<MyThread*>(the_thread_);

	assert(the_thread != NULL);

	the_thread->run();

	// Decrement the reference count
	int64 new_ref_count = the_thread->decRefCount();
	assert(new_ref_count >= 0);
	if(new_ref_count == 0)
		delete the_thread;

	return 0;
}


void MyThread::launch()
{
	assert(thread_handle == 0);

	// Increment the thread reference count.
	// This is because there is a pointer to the the thread that is passed as an argument to _beginthreadex() ('this' passed as the arglist param),
	// that isn't a Reference object.
	this->incRefCount();

#if defined(_WIN32)
	thread_handle = (HANDLE)_beginthreadex(
		NULL, // security
		0, // stack size
		threadFunction, // startAddress
		this, // arglist
		0, // Initflag, 0 = running
		NULL // thread identifier out.
	);

	if(thread_handle == 0)
		throw MyThreadExcep("Thread creation failed.");
#else
	const int result = pthread_create(
		&thread_handle, // Thread
		NULL, // attr
		threadFunction, // start routine
		this // arg
		);
	if(result != 0)
	{
		if(result == EAGAIN)
			throw MyThreadExcep("Thread creation failed.  (EAGAIN)");
		else if(result == EINVAL)
			throw MyThreadExcep("Thread creation failed.  (EINVAL)");
		else if(result == EPERM)
			throw MyThreadExcep("Thread creation failed.  (EPERM)");
		else
			throw MyThreadExcep("Thread creation failed.  (Error code: " + toString(result) + ")");
	}
#endif
}


void MyThread::join() // Wait for thread termination
{
	joined = true;
#if defined(_WIN32)
	/*const DWORD result =*/ ::WaitForSingleObject(thread_handle, INFINITE);
#else
	const int result = pthread_join(thread_handle, NULL);
	assert(result == 0);
#endif
}


void MyThread::setPriority(Priority p)
{
#if defined(_WIN32)
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
