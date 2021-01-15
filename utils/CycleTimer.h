/*=====================================================================
CycleTimer.h
------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"


#if (defined(_WIN32) || defined(_WIN64)) && !defined(__MINGW32__)
#include <intrin.h>
#pragma intrinsic(__rdtsc)
#endif

// We want to make sure that either __x86_64__ or __i386__ is defined when building on non windows platforms.
#if !defined(_WIN32) && (!defined(__x86_64__) && !defined(__i386__))
#error Either __x86_64__ or __i386_ need to be defined!
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

	typedef int64_t CYCLETIME_TYPE;

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
}
