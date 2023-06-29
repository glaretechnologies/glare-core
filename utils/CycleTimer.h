/*=====================================================================
CycleTimer.h
------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "Platform.h"


// CycleTimer is only supported on x64 chips.
#if defined(__x86_64__) || defined(_M_X64)
#define CYCLETIMER_SUPPORTED 1
#else
#define CYCLETIMER_SUPPORTED 0
#endif


#if defined(_WIN32) && CYCLETIMER_SUPPORTED
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
	CycleTimer();

	typedef int64 CYCLETIME_TYPE;

	GLARE_STRONG_INLINE void reset();

	GLARE_STRONG_INLINE CYCLETIME_TYPE elapsed() const;
	GLARE_STRONG_INLINE CYCLETIME_TYPE getCyclesElapsed() const;
// Adjusted for execution time of CPUID.
	// NOTE: may be negative or 0.

	GLARE_STRONG_INLINE CYCLETIME_TYPE getRawCyclesElapsed() const;
	//double getSecondsElapsed() const;

	GLARE_STRONG_INLINE CYCLETIME_TYPE getRDTSCTime() const { return rdtsc_time; }
private:
	GLARE_STRONG_INLINE CYCLETIME_TYPE getCounter() const;

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
#if CYCLETIMER_SUPPORTED

#if (defined(_WIN32) || defined(_WIN64)) && !defined(__MINGW32__)
	return (CYCLETIME_TYPE)__rdtsc();
#else

// On unix systems we need to have different rdtsc implementations for different achitectures for some obscure unixy reason.
#if defined(__x86_64__)
	long n;
	asm volatile ("rdtsc\n"
		"shlq $32,%%rdx\n"
		"orq %%rdx,%%rax"
		: "=a" (n) :: "%rdx");

	return (CYCLETIME_TYPE)n;
#elif defined(__i386__)
	unsigned long long ret;
	__asm__ __volatile__("rdtsc": "=A" (ret));

	return (CYCLETIME_TYPE)ret;
#endif

#endif

#else // else if !CYCLETIMER_SUPPORTED:
	assert(0);
	return 0;
#endif
}
