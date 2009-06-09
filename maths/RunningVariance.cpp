/*=====================================================================
RunningVariance.cpp
-------------------
File created by ClassTemplate on Thu Jun 04 15:54:34 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "RunningVariance.h"


#include "../indigo/TestUtils.h"
#include "mathstypes.h"
#include <vector>
#include "../utils/MTwister.h"

/*
RunningVariance::RunningVariance()
{
	
}


RunningVariance::~RunningVariance()
{
	
}
*/

static double mean(const std::vector<double>& d)
{
	double sum = 0.0;
	for(unsigned int i=0; i<d.size(); ++i)
		sum += d[i];
	return sum / (double)d.size();
}


static double variance(const std::vector<double>& d)
{
	const double mu = mean(d);

	double sum = 0.0;
	for(unsigned int i=0; i<d.size(); ++i)
		sum += Maths::square(mu - d[i]);
	return sum / (double)d.size();
}


static double stdDev(const std::vector<double>& d)
{
	return std::sqrt(variance(d));
}


void testRunningVariance()
{
	{
		RunningVariance<double> rv;

		testAssert(epsEqual(rv.mean(), 0.0));
		testAssert(epsEqual(rv.standardDev(), 0.0));
		testAssert(epsEqual(rv.variance(), 0.0));

		rv.update(0.0);

		testAssert(epsEqual(rv.mean(), 0.0));
		testAssert(epsEqual(rv.standardDev(), 0.0));
		testAssert(epsEqual(rv.variance(), 0.0));

		rv.update(-2.0);
		rv.update(2.0);
		rv.update(-2.0);
		rv.update(2.0);

		testAssert(epsEqual(rv.mean(), 0.0));
		testAssert(epsEqual(rv.standardDev(), 2.0));
		testAssert(epsEqual(rv.variance(), 4.0));
	}

	{
		MTwister rng(1);

		const int N = 1000000;
		std::vector<double> d(N);
		RunningVariance<double> rv;

		for(unsigned int i=0; i<N; ++i)
		{
			d[i] = (rng.unitRandom() - 0.5) * 1000.0;
			rv.update(d[i]);
		}

		{
			const double a = rv.mean();
			const double b = mean(d);
			testAssert(epsEqual(a, b));
		}
		{
			const double a = rv.variance();
			const double b = variance(d);
			testAssert(Maths::approxEq(a, b));
		}
		{
			const double a = rv.standardDev();
			const double b = stdDev(d);
			testAssert(Maths::approxEq(a, b));
		}
	}

}





