/*=====================================================================
Condition.h
-----------
File created by ClassTemplate on Tue Jun 14 06:56:49 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __CONDITION_H_666_
#define __CONDITION_H_666_


#ifdef WIN32
#include <windows.h>
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
	/*=====================================================================
	Condition
	---------
	
	=====================================================================*/
	Condition();

	~Condition();

	///Calling thread is suspended until conidition is met.
	//The mutex will be released while the thread is waiting and reaquired before the function returns.
	void wait(Mutex& mutex);

	///Condition has been met: wake up one or more suspended threads.
	void notify();

	///Resets condition so that threads will block on wait() again.
	void resetToFalse();

#ifdef WIN32
	HANDLE condition;
#else
	pthread_cond_t condition;
#endif
};



#endif //__CONDITION_H_666_




