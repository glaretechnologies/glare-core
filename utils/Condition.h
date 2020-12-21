/*=====================================================================
Condition.h
-----------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#if defined(_WIN32)
#include "IncludeWindows.h"
#include <process.h>
#else
#include <pthread.h>
#endif
class Mutex;


/*=====================================================================
Condition
---------
Condition is false/non-signalled upon creation by default.
=====================================================================*/
class Condition
{
public:
	Condition();
	~Condition();

	// Calling code should be currently holding (have locked) the mutex.
	// Calling thread is suspended until condition is met.
	// The mutex will be released while the thread is waiting and reaquired before the function returns.
	// Returns true if condition was signalled, or false if a timeout occurred.
	bool waitWithTimeout(Mutex& mutex, double wait_time_seconds);

	// Waits without a timeout.
	void wait(Mutex& mutex);

	// Condition has been met: wake up one or more suspended threads.
	void notify();

	// Wake up all suspended threads.
	void notifyAll();

#if defined(_WIN32)
	CONDITION_VARIABLE condition;
#else
	pthread_cond_t condition;
#endif
};
