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
void Condition::wait(Mutex& mutex)
{
#if defined(WIN32) || defined(WIN64)

	///Release mutex///
	mutex.release();

	///wait on the condition event///
	::WaitForSingleObject(condition, INFINITE);

	///Get the mutex again///
	mutex.acquire();
#else
	//this automatically release the associated mutex in pthreads.
	//Re-locks mutex on return.
	pthread_cond_wait(&condition, &mutex.mutex);
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








