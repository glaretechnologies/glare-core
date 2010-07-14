/*=====================================================================
UtilTests.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Wed Jul 14 11:41:12 +1200 2010
=====================================================================*/
#include "UtilTests.h"


#include "timer.h"
#include "stringutils.h"
#include "../indigo/globals.h"


UtilTests::UtilTests()
{

}


UtilTests::~UtilTests()
{

}


void UtilTests::test()
{
	const int N = 10000000;
	double sum_elapsed_time = 0;

	Timer sum_timer;

	for(int i=0; i<N; ++i)
	{
		Timer t;
		sum_elapsed_time += t.elapsed();
	}

	const double sum_time = sum_timer.elapsed();

	conPrint("sum_time: " + toString(sum_time));
	conPrint("per-timer time: " + toString(1.0e9 * sum_time / N) + " ns");
	conPrint("sum_elapsed_time: " + toString(sum_elapsed_time));

	exit(0);
}
