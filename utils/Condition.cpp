/*=====================================================================
Condition.cpp
-------------
File created by ClassTemplate on Tue Jun 14 06:56:49 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "Condition.h"


#include "Platform.h"
#include "Mutex.h"
#include <assert.h>
#include <cmath>


#if defined(_WIN32)
#else
#include <errno.h>
#include <sys/time.h>
#endif


Condition::Condition()
{
#if defined(_WIN32)
	InitializeConditionVariable(&condition);
#else
	pthread_cond_init(&condition, NULL);
#endif
}


Condition::~Condition()
{
#if !defined(_WIN32)
	pthread_cond_destroy(&condition);
#endif
}


///Calling thread is suspended until condition is met.
bool Condition::wait(Mutex& mutex, bool infinite_wait_time, double wait_time_seconds)
{
#if defined(_WIN32)
	const BOOL result = ::SleepConditionVariableCS(&condition, &mutex.mutex, infinite_wait_time ? INFINITE : (DWORD)(wait_time_seconds * 1000.0));
	if(result) // if function succeeded:
		return true;
	else
	{
		const DWORD last_error = GetLastError();
		if(last_error == ERROR_TIMEOUT)
			return false;
		else
		{
			assert(0);
			return false;
		}
	}
#else
	// This automatically release the associated mutex in pthreads.
	// Re-locks mutex on return.
	if(infinite_wait_time)
	{
		pthread_cond_wait(&condition, &mutex.mutex);
		return true;
	}
	else
	{
		double integer_seconds;
		const double fractional_seconds = std::modf(wait_time_seconds, &integer_seconds);

		struct timeval now;
		gettimeofday(&now,NULL);
		
		// Convert from a timeval to a timespec
		struct timespec ts;
		ts.tv_sec = now.tv_sec;
		ts.tv_nsec = now.tv_usec * 1000;

		// Add on the time specified
		ts.tv_sec += (time_t)integer_seconds;
		ts.tv_nsec += (long)(fractional_seconds * 1.0e9);
		if(ts.tv_nsec >= 1000000000){
			ts.tv_sec++;
			ts.tv_nsec -= 1000000000;
		}

		//std::cout << "t.tv_sec: " << t.tv_sec << std::endl;
		//std::cout << "t.tv_nsec: " << t.tv_nsec << std::endl;

		const int result = pthread_cond_timedwait(&condition, &mutex.mutex, &ts);
		if(result == 0)
			return true;
		else if(result == ETIMEDOUT)
			return false;
		else
		{
			// WTF?
			assert(0);
			return false;
		}
	}
#endif
}


// Condition has been met: wake up one suspended thread.
void Condition::notify()
{
#if defined(_WIN32)
	WakeConditionVariable(&condition);
#else
	pthread_cond_signal(&condition);
#endif
}


void Condition::notifyAll()
{
#if defined(_WIN32)
	WakeAllConditionVariable(&condition);
#else
	pthread_cond_broadcast(&condition);
#endif
}
