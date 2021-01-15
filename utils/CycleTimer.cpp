/*=====================================================================
CycleTimer.cpp
--------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#include "CycleTimer.h"


#include <assert.h>
#include "../maths/mathstypes.h"


CycleTimer::CycleTimer()
{
	static_assert(sizeof(CYCLETIME_TYPE) == 8, "sizeof(CYCLETIME_TYPE) == 8");
	
	rdtsc_time = std::numeric_limits<CYCLETIME_TYPE>::max();

	for(int i=0; i<3; ++i)
	{
		reset();
		const CYCLETIME_TYPE t = getRawCyclesElapsed();
		rdtsc_time = myMin(rdtsc_time, t);
	}
	
	/*CYCLETIME_TYPE cpuid_time_1 = getRawCyclesElapsed();
	reset();
	CYCLETIME_TYPE cpuid_time_2 = getRawCyclesElapsed();
	reset();
	CYCLETIME_TYPE cpuid_time_3 = getRawCyclesElapsed();

	 = myMin(cpuid_time_1, cpuid_time_2, cpuid_time_3);*/
}


/*
void CycleTimer::test()
{

}
*/
