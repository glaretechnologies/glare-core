/*=====================================================================
MyThread.h
----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "ThreadSafeRefCounted.h"
#include "Reference.h"
#include "Exception.h"
#include "IncludeWindows.h"
#if !defined(_WIN32)
#include <pthread.h>
#endif
#include <string>


class MyThreadExcep : public glare::Exception
{
public:
	MyThreadExcep(const std::string& text_) : glare::Exception(text_) {}
};


/*=====================================================================
MyThread
--------
A thread class.  Reference counted.
=====================================================================*/
class MyThread : public ThreadSafeRefCounted
{
public:
	MyThread();
	virtual ~MyThread();

	// Implement this method to do work in the thread.
	virtual void run() = 0;

	// Start executing the thread.
	void launch();

	// Wait for thread termination.
	void join();


	enum Priority
	{
		Priority_Normal,
		Priority_BelowNormal,
		Priority_Lowest,
		Priority_Idle
	};
	// Set the priority of this thread
	void setPriority(Priority p); // throws MyThreadExcep

#if defined(_WIN32)
	HANDLE getHandle() { return thread_handle; }

	void setAffinity(int32 group, uint64 proc_affinity_mask); // throws MyThreadExcep
#endif

private:
#if defined(_WIN32)
	HANDLE thread_handle;
#else
	pthread_t thread_handle;
#endif

	bool joined; // True if this thread had join() called on it.
};


typedef Reference<MyThread> MyThreadRef;
