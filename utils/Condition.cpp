/*=====================================================================
Condition.cpp
-------------
File created by ClassTemplate on Tue Jun 14 06:56:49 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "Condition.h"

#include "mutex.h"
#include <assert.h>



Condition::Condition()
{
#if defined(WIN32) || defined(WIN64)
	condition = CreateEvent( 
        NULL,         // no security attributes
        TRUE,         // manual-reset event
        FALSE,//TRUE, // is initial state signaled?
        NULL          // object name
        ); 

	assert(condition != NULL);
#else
	pthread_cond_init(&condition, NULL);

#endif
}


Condition::~Condition()
{
#if defined(WIN32) || defined(WIN64)
	const HRESULT result = CloseHandle(condition);
	if(result == 0)//S_OK)
	{
		//Function failed.
		assert(false);
		const DWORD e = GetLastError();
	}
#else
	pthread_cond_destroy(&condition);
#endif
}


///Calling thread is suspended until conidition is met.
bool Condition::wait(Mutex& mutex, bool infinite_wait_time, double wait_time_seconds)
{
#if defined(WIN32) || defined(WIN64)

	///Release mutex///
	mutex.release();

	bool signalled = false;

	///wait on the condition event///
	if(infinite_wait_time)
	{
		::WaitForSingleObject(condition, INFINITE);
		signalled = true;
	}
	else
	{
		const DWORD result = ::WaitForSingleObject(condition, (DWORD)(wait_time_seconds * 1000.0));
		if(result == WAIT_OBJECT_0) // Object was signalled.
			signalled = true;
		else if(result == WAIT_TIMEOUT)
			signalled = false;
		else
		{
			// Uh oh, some crazy shit happened (WAIT_ABANDONED)
			assert(0);
			signalled = false;
		}
	}

	///Get the mutex again///
	mutex.acquire();

	return signalled;
#else
	//this automatically release the associated mutex in pthreads.
	//Re-locks mutex on return.
	if(infinite_wait_time)
	{
		pthread_cond_wait(&condition, &mutex.mutex);
		return true;
	}
	else
	{
		double integer_seconds;
		const double fractional_seconds = modf(wait_time_seconds, &integer_seconds);

		struct timespec t;
		t.seconds = (time_t)integer_seconds;
		t.tv_nsec = (long)(fractional_seconds * 1.0e9);

		const int result = pthread_cond_timedwait(&condition, &mutex.mutex, &t);
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


///Condition has been met: wake up one suspended thread.
void Condition::notify()
{
#if defined(WIN32) || defined(WIN64)
	///set event to signalled state
	SetEvent(condition);
#else
	///wake up a single suspended thread.
	pthread_cond_signal(&condition);
#endif
}


void Condition::resetToFalse()
{
#if defined(WIN32) || defined(WIN64)
	///set event to non-signalled state
	const BOOL result = ResetEvent(condition);
	assert(result != FALSE);
#else

#endif
}








