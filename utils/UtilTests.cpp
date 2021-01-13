/*=====================================================================
UtilTests.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Jul 14 11:41:12 +1200 2010
=====================================================================*/
#include "UtilTests.h"


#include "Timer.h"
#include "CycleTimer.h"
#include "StringUtils.h"
#include "PlatformUtils.h"
#include "../indigo/globals.h"
#include "../utils/FileHandle.h"
#include "../utils/Exception.h"
#include "../utils/TestUtils.h"


UtilTests::UtilTests()
{

}


UtilTests::~UtilTests()
{

}

/*
#include <intrin.h>
#pragma intrinsic(__rdtsc)
typedef unsigned __int64 ticks;
#define getticks __rdtsc
*/


#if BUILD_TESTS


void UtilTests::test()
{
	if(false)
	{
		{
			conPrint("============ Timer =============");
			// Test speed of Timer.
			const int N = 10000000;
			double sum_elapsed_time = 0;
			CycleTimer cycle_timer;
			Timer sum_timer;

			Timer t;
			for(int i=0; i<N; ++i)
			{
				t.reset();
				sum_elapsed_time += t.elapsed();
			}

			const double sum_time = sum_timer.elapsed();
			//ticks end_ticks = getticks();

			//const ticks elapsed_ticks = end_ticks - start_ticks;
			//conPrint("elapsed_ticks: " + toString(elapsed_ticks));
			//conPrint("per-timer ticks: " + toString(elapsed_ticks / N));

			const CycleTimer::CYCLETIME_TYPE cycles = cycle_timer.getCyclesElapsed();
			conPrint("Elapsed cycles: " + toString((uint64)cycles));
			conPrint("per-timer cycles: " + toString((uint64)cycles / N) + " cycles");

			conPrint("sum_time: " + toString(sum_time) + " s");
			conPrint("per-timer time: " + toString(1.0e9 * sum_time / N) + " ns");

			conPrint("sum_elapsed_time: " + toString(sum_elapsed_time));
		}
		{
			conPrint("============ CycleTimer =============");
			// Test speed of CycleTimer.
			const int N = 10000000;
			CycleTimer::CYCLETIME_TYPE sum_elapsed_cycles = 0;
			CycleTimer sum_cycle_timer;
			Timer sum_timer;

			CycleTimer t;

			for(int i=0; i<N; ++i)
			{
				t.reset();	
				sum_elapsed_cycles += t.getCyclesElapsed();
			}

			const double sum_time = sum_timer.elapsed();

			const CycleTimer::CYCLETIME_TYPE cycles = sum_cycle_timer.getCyclesElapsed();
			conPrint("Elapsed cycles: " + toString((uint64)cycles));
			conPrint("per-timer cycles: " + toString((uint64)cycles / N) + " cycles");

			conPrint("sum_time: " + toString(sum_time) + " s");
			conPrint("per-timer time: " + toString(1.0e9 * sum_time / N) + " ns");

			conPrint("sum_elapsed_cycles: " + toString((uint64)sum_elapsed_cycles));
		}

		//exit(0);
	}


	// Test FileHandle
	try
	{
		int n = 10;
		for(int i=0; i<n; ++i)
		{
			FileHandle f(PlatformUtils::getTempDirPath() + "/testfiles_indigo_bleh", "w");
			fputc('a', f.getFile());
		}

		for(int i=0; i<n; ++i)
		{
			FileHandle f;
			f.open(PlatformUtils::getTempDirPath() + "/testfiles_indigo_bleh", "w");
			fputc('a', f.getFile());
		}

		// Try a file that doesn't exist
		try
		{
			FileHandle f(PlatformUtils::getTempDirPath() + "/testfiles_indigo_bleh_idfhkjsdghkjfhgdkfj", "r");
			
			failTest("Should have thrown an exception.");
		}
		catch(glare::Exception& )
		{
		}
	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}


#endif // BUILD_TESTS