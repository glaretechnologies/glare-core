/*=====================================================================
CycleTimer.cpp
--------------
File created by ClassTemplate on Mon Jul 18 03:30:45 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "CycleTimer.h"


#include <assert.h>
#include "../maths/mathstypes.h"


CycleTimer::CycleTimer()
{
	assert(sizeof(CYCLETIME_TYPE) == 8);
	
	cpuid_time = 10000000;

	for(int i=0; i<3; ++i)
	{
		reset();
		const CYCLETIME_TYPE t = getRawCyclesElapsed();
		cpuid_time = myMin(cpuid_time, t);
	}
	
	/*CYCLETIME_TYPE cpuid_time_1 = getRawCyclesElapsed();
	reset();
	CYCLETIME_TYPE cpuid_time_2 = getRawCyclesElapsed();
	reset();
	CYCLETIME_TYPE cpuid_time_3 = getRawCyclesElapsed();

	 = myMin(cpuid_time_1, cpuid_time_2, cpuid_time_3);*/
}


CycleTimer::~CycleTimer()
{
	
}


/*
void CycleTimer::test()
{

}
*/
