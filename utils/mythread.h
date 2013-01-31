/*=====================================================================
mythread.h
-------------------
Copyright Glare Technologies Limited 2013 -
=====================================================================*/
#pragma once


#include "ThreadSafeRefCounted.h"
#include "Reference.h"
#include "IncludeWindows.h"
#if !defined(_WIN32)
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
		Priority_BelowNormal
	};
	// Set the priority of this thread
	void setPriority(Priority p); // throws MyThreadExcep

#if defined(_WIN32)
	HANDLE getHandle() { return thread_handle; }
#endif

	// bool autoDelete() const { return autodelete; }

private:
#if defined(_WIN32)
	HANDLE thread_handle;
#else
	pthread_t thread_handle;
#endif

	bool joined; // True if this thread had join() called on it.
};


typedef Reference<MyThread> MyThreadRef;
