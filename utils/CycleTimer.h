/*=====================================================================
CycleTimer.h
------------
File created by ClassTemplate on Mon Jul 18 03:30:45 2005
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __CYCLETIMER_H_666_
#define __CYCLETIMER_H_666_



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

	typedef /*unsigned long long*/ __int64 CYCLETIME_TYPE;//signed 64 bit integer type

	__forceinline void reset();

	__forceinline CYCLETIME_TYPE getCyclesElapsed() const;
	//adjusted for execution time of CPUID.
	//NOTE: may be negative or 0.

	__forceinline CYCLETIME_TYPE getRawCyclesElapsed() const;
	//double getSecondsElapsed() const;

private:
	__forceinline CYCLETIME_TYPE getCounter() const;

	CYCLETIME_TYPE start_time;
	CYCLETIME_TYPE cpuid_time;
	
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
CycleTimer::CYCLETIME_TYPE CycleTimer::getCyclesElapsed() const
{
	return (getCounter() - start_time) - cpuid_time;
	/*CYCLETIME_TYPE time = getCounter() - start_time - cpuid_time;
	if(time < 0)
		return 0;
	else
		return time;*/
}

CycleTimer::CYCLETIME_TYPE CycleTimer::getCounter() const
{
	CycleTimer::CYCLETIME_TYPE numticks = 0;

	__asm
	{
		cpuid //a serialising instruction to force in order execution
		rdtsc //store counter in eax and edx
		lea ecx,numticks //load addr of numticks into ecx
		mov dword ptr [ecx],  eax //write low order bits into numticks
		mov dword ptr [ecx+4],edx //write high order bits into numticks
	}

	return numticks;
}


#endif //__CYCLETIMER_H_666_




