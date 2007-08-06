/*=====================================================================
CycleTimer.cpp
--------------
File created by ClassTemplate on Mon Jul 18 03:30:45 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "CycleTimer.h"

#include <assert.h>





CycleTimer::CycleTimer()
{
	assert(sizeof(CYCLETIME_TYPE) == 8);
	
	/*__asm 
	{
		cpuid
		cpuid
		cpuid
	}*/

	reset();
	cpuid_time = getRawCyclesElapsed();
	reset();
	cpuid_time = getRawCyclesElapsed();
	reset();
	cpuid_time = getRawCyclesElapsed();
}


CycleTimer::~CycleTimer()
{
	
}




/*double CycleTimer::getSecondsElapsed() const
{

}*/



