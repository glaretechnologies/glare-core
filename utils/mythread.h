/*=====================================================================
MyThread.h
----------
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "ThreadSafeRefCounted.h"
#include "reference.h"
#if defined(_WIN32)
// Stop windows.h from defining the min() and max() macros
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif
#include <string>


class MyThreadExcep
{
public:
	MyThreadExcep(const std::string& text_) : text(text_) {}
	const std::string& what() const { return text; }
private:
	std::string text;
};


/*=====================================================================
MyThread
--------
=====================================================================*/
class MyThread : public ThreadSafeRefCounted
{
public:
	MyThread();
	virtual ~MyThread();

	virtual void run() = 0;

	void launch(/*bool autodelete = true*/);

	// Wait for thread termination
	// It's not allowed to join an autodeleting thread, because you get a race condition - the thread may terminate and delete itself before the join method runs.
	void join();

	enum Priority
	{
		Priority_Normal,
		Priority_BelowNormal
	};
	void setPriority(Priority p); // throws MyThreadExcep

#if defined(_WIN32)
	HANDLE getHandle() { return thread_handle; }
#endif

	bool autoDelete() const { return autodelete; }

private:
#if defined(_WIN32)
	HANDLE thread_handle;
#else
	pthread_t thread_handle;
#endif

	bool autodelete;
	bool joined; // True if this thread had join() called on it.
};


typedef Reference<MyThread> MyThreadRef;
