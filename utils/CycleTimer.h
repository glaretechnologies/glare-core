/*=====================================================================
CycleTimer.h
------------
File created by ClassTemplate on Mon Jul 18 03:30:45 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __CYCLETIMER_H_666_
#define __CYCLETIMER_H_666_


#include "platform.h"


#if (defined(_WIN32) || defined(_WIN64)) && !defined(__MINGW32__)
#include <intrin.h>
#pragma intrinsic(__rdtsc)
#endif


/*=====================================================================
CycleTimer
----------
Uses the rdtsc (read time-stamp counter) instruction for timing with cycle
precision.

See Intel's 'Using the RDTSC Instruction for Performance Monitoring':
http://www.math.uwaterloo.ca/~jamuir/rdtscpm1.pdf
=====================================================================*/
class CycleTimer
{
public:
	/*=====================================================================
	CycleTimer
	----------
	
	=====================================================================*/
	CycleTimer();

	~CycleTimer();

	typedef int64_t CYCLETIME_TYPE;

	INDIGO_STRONG_INLINE void reset();

	INDIGO_STRONG_INLINE CYCLETIME_TYPE elapsed() const;
	INDIGO_STRONG_INLINE CYCLETIME_TYPE getCyclesElapsed() const;
// Adjusted for execution time of CPUID.
	// NOTE: may be negative or 0.

	INDIGO_STRONG_INLINE CYCLETIME_TYPE getRawCyclesElapsed() const;
	//double getSecondsElapsed() const;

	INDIGO_STRONG_INLINE CYCLETIME_TYPE getRDTSCTime() const { return rdtsc_time; }
private:
	INDIGO_STRONG_INLINE CYCLETIME_TYPE getCounter() const;

	CYCLETIME_TYPE start_time;
	CYCLETIME_TYPE rdtsc_time;
};


void CycleTimer::reset()
{
	start_time = getCounter();
}


CycleTimer::CYCLETIME_TYPE CycleTimer::getRawCyclesElapsed() const
{
	return getCounter() - start_time;
}


//NOTE: may be negative or 0.
CycleTimer::CYCLETIME_TYPE CycleTimer::elapsed() const
{
	return (getCounter() - start_time) - rdtsc_time;
}


//NOTE: may be negative or 0.
CycleTimer::CYCLETIME_TYPE CycleTimer::getCyclesElapsed() const
{
	return (getCounter() - start_time) - rdtsc_time;
}


CycleTimer::CYCLETIME_TYPE CycleTimer::getCounter() const
{
#if (defined(_WIN32) || defined(_WIN64)) && !defined(__MINGW32__)
	return (CYCLETIME_TYPE)__rdtsc();
#else
	unsigned long long ret;
	__asm__ __volatile__("rdtsc": "=A" (ret));

	return (CYCLETIME_TYPE)ret;
#endif
}


#endif //__CYCLETIMER_H_666_
