/*=====================================================================
DiscreteDistribution.cpp
------------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#include "DiscreteDistribution.h"


#include "../maths/mathstypes.h"
#include "../utils/StringUtils.h"


DiscreteDistribution::DiscreteDistribution()
{

}


DiscreteDistribution::DiscreteDistribution(const std::vector<Real>& data)
{
	build(data);
}


DiscreteDistribution::~DiscreteDistribution()
{

}


void DiscreteDistribution::build(const std::vector<Real>& data)
{
	assert(data.size() >= 1);

	N = (int)data.size();
	this->recip_N = 1.f / N;
	cdf.resize(data.size() + 1);

	Real sum = 0;
	cdf[0] = 0;
	for(size_t i=0; i<data.size(); ++i)
	{
		assert(data[i] >= 0);
		sum += data[i];
		cdf[i + 1] = sum;
	}

	if(sum == 0.0f)
	{
		// Make some dummy buckets, so if this dist1 is ever sampled for some reason, it doesn't crash.
		cdf.back() = 1.f;
		num_buckets = 1;
		buckets.resize(1);
		buckets[0] = 0;
		return;
	}

	// Normalise
	const double factor = 1 / (double)sum;
	for(size_t i=0; i<cdf.size(); ++i)
		cdf[i] = (float)((double)cdf[i] * factor);

//	assert(cdf[N-1] == 1.0f);

	// Build lookup buckets.
	// Each bucket corresponds to an interval of y (cdf) values.  The interval is closed at the lower end, and
	// open at the upper end.
	// The interval for bucket i is thus [i/N_b, (i+1)/N_b), where N_b is the number of buckets.
	// Each bucket holds an integer, which is the index into the CDF array, of the lowest CDF value
	// which is > the bucket upper bound.
	num_buckets = myMax(1, (int)data.size() / 2);
	buckets.resize(num_buckets);

	int lowest = 0; // Lowest index of CDF value >= bucket_upper_bound
	for(int i=0; i<num_buckets; ++i)
	{
		Real bucket_upper_bound = (i + 1) / (Real)num_buckets;
		assert(bucket_upper_bound <= 1.0f);

		while(cdf[lowest] < bucket_upper_bound && lowest < (int)cdf.size() - 1)
			lowest++;

		buckets[i] = lowest;
	}

	// bucket[0] holds the index of the first cdf value >= bucket 0 upper bound, which is > 0, 
	// and since cdf[0] = 0, bucket[0] must be > 0.
	assert(buckets[0] > 0);
}


uint32 DiscreteDistribution::sampleReference(Real unitsample) const
{
	assert(unitsample >= 0.0 && unitsample < 1.0);

	const std::vector<Real>::const_iterator it = std::upper_bound(cdf.begin(), cdf.end(), unitsample);

	unsigned int i = (unsigned int)(it - cdf.begin());
	assert(i > 0); // cdf[0] = 0, so 'i' should be > 0.
	
	return i - 1;
}


#if BUILD_TESTS


#include "TestUtils.h"
#include "globals.h"
#include "../maths/PCG32.h"
#include "../utils/Timer.h"


void DiscreteDistribution::test()
{
	conPrint("DiscreteDistribution::test()");

	{
		std::vector<float> data(4);
		data[0] = 2;
		data[1] = 3;
		data[2] = 2;
		data[3] = 3;

		float sum = 0;
		for(size_t i=0; i<data.size(); ++i)
			sum += data[i];

		DiscreteDistribution dist(data);

		testAssert(epsEqual(dist.probability(0), 2.f / 10.f));
		testAssert(epsEqual(dist.probability(1), 3.f / 10.f));
		testAssert(epsEqual(dist.probability(2), 2.f / 10.f));
		testAssert(epsEqual(dist.probability(3), 3.f / 10.f));

		float p;
		testAssert(dist.sample(0.001f, p) == 0);
		testAssert(dist.sample(0.49f, p) == 1);
		testAssert(dist.sample(0.51f, p) == 2);
		testAssert(dist.sample(0.99f, p) == 3);


		std::vector<int> bins(4, 0);
		PCG32 rng(1);
		const int N = 300000;
		for(int i=0; i<N; ++i)
		{
			const float ur = rng.unitRandom();
			uint32 x = dist.sample(ur, p);
			testAssert(epsEqual(p, dist.probability(x)));
			uint32 ref_x = dist.sampleReference(ur);

			testAssert(x == ref_x);
			
			assert(x >= 0 && x <= 3);

			bins[x]++;
		}

		const float tolerance = N * 0.002;
		for(size_t i=0; i<data.size(); ++i)
		{
			float expected = N * data[i] / sum;
			float actual = (float)bins[i];

			testEpsEqualWithEps(expected, actual, tolerance);
		}

		testAssert(dist.sample(0.0, p) == 0);
		testAssert(dist.sample(0.9999f, p) == 3);
	}


	{
		PCG32 rng(1);

		const int sz = 1000;
		std::vector<float> data(sz);
		for(int i=0; i<sz; ++i)
			data[i] = rng.unitRandom() * 1.0e10f;

		DiscreteDistribution dist(data);

		const int N = 1000;

		for(int i=0; i<N; ++i)
		{
			float p;
			const uint32 x = dist.sample(rng.unitRandom(), p);
			testAssert(epsEqual(p, dist.probability(x)));
		}
	}


	// Peformance test
	{
		PCG32 rng(1);

		const int sz = 2048;
		std::vector<float> data(sz);
		for(int i=0; i<sz; ++i)
			data[i] = rng.unitRandom() * 1.0e10f;

		DiscreteDistribution dist(data);

		float p;
		conPrint(toString(dist.sample(0.5, p)));
	}

	// Peformance test
	{
		PCG32 rng(1);

		const int sz = 10000;
		std::vector<float> data(sz);
		for(int i=0; i<sz; ++i)
			data[i] = rng.unitRandom() * 1.0e-10f;

		DiscreteDistribution dist(data);

		float p;
		conPrint(toString(dist.sample(0.5, p)));
	}

	// Peformance test
	{
		PCG32 rng(1);

		const int sz = 10000;
		std::vector<float> data(sz);
		for(int i=0; i<sz; ++i)
			data[i] = rng.unitRandom();

		DiscreteDistribution dist(data);

		
		float result_sum = 0;
		float reference_result_sum = 0;
		const int N = 10000;

		{		
			PCG32 rng2(1);
			Timer timer;
			for(int i=0; i<N; ++i)
			{
				const float ur = rng2.unitRandom();
				float p;
				uint32 result = dist.sample(ur, p);
				assert(result >= 0 && result < (uint32)sz);
				result_sum += result;
			}

			double elapsed = timer.elapsed();
			conPrint("result_sum: " + toString(result_sum));
			conPrint("elapsed: " + toString(elapsed));
		}
		{
			PCG32 rng2(1);
			Timer timer;
			for(int i=0; i<N; ++i)
			{
				const float ur = rng2.unitRandom();
				uint32 result = dist.sampleReference(ur);
				assert(result >= 0 && result < (uint32)sz);
				reference_result_sum += result;
			}

			double elapsed = timer.elapsed();
			conPrint("reference result_sum: " + toString(reference_result_sum));
			conPrint("reference elapsed: " + toString(elapsed));
		}
	}




	/*// Test that we don't get crashes when we sample with a value not in [0, 1)
	{
		PCG32 rng(1);

		const int sz = 10;
		std::vector<float> data(sz);
		for(int i=0; i<sz; ++i)
			data[i] = rng.unitRandom() * 1.0e10f;

		DiscreteDistribution dist(data);

		// Sample with a value < 0
		float p;
		conPrint(toString(dist.sample(-0.1f, p)));
		testAssert(dist.sample(-0.1f, p) == 0);

		// Sample with a value = 1
		conPrint(toString(dist.sample(1.0f, p)));
		testAssert(dist.sample(1.0f, p) == sz - 1);
		
		// Sample with a value > 1
		conPrint(toString(dist.sample(1.1f, p)));
		testAssert(dist.sample(1.1f, p) == sz - 1);
	}*/
}


#endif
