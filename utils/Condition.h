/*=====================================================================
Condition.h
-----------
Copyright Glare Technologies Limited 2019 -
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

	// Calling thread is suspended until condition is met.
	// The mutex will be released while the thread is waiting and reaquired before the function returns.
	// Returns true if condition was signalled, or false if a timeout occurred.
	bool wait(Mutex& mutex, bool infinite_wait_time, double wait_time_seconds);

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
