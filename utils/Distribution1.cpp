/*=====================================================================
Distribution1.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Sun Aug 15 02:19:14 +1200 2010
=====================================================================*/

#if (BUILD_TESTS)

#include "Distribution1.h"

#include <math.h>
#include <iostream>
#include "MTwister.h"
#include "timer.h"

void Distribution1<float>::test()
{
	int num_bins = 10;

	std::vector<uint32> bins(num_bins);
	std::vector<float> weights(num_bins);

	for(int i = 0; i < num_bins; ++i)
	{
		bins[i] = 0;
		weights[i] = 1.0f / (float)num_bins;
	}

	weights[num_bins / 2] *= 10;

	Distribution1 dist(weights);

	uint32 num_samples = 4294967296 / 16 - 1;

	MTwister rng(631579123);


	Timer timer;
	timer.reset();

	for(uint32 i = 0; i < num_samples; ++i) bins[dist.sample(rng.unitRandom())]++;

	double time_taken = timer.elapsed();


	if(false)
	{
	std::cout << "histogram:" << std::endl;
	for(int i = 0; i < num_bins; ++i)
		std::cout << "bin " << i << ": " << bins[i] << std::endl;
	}

	std::cout << "simulation took " << time_taken << " seconds." << std::endl;


	double errorsum[4] = { 0, 0, 0, 0 };
	for (uint32 i = 0; i < num_samples; ++i)
	{
		float x01_out, x_in = rng.unitRandom();
		dist.sampleLerp(x_in, x01_out);
		errorsum[i & 3] += fabs(x01_out - x_in);
	}

	std::cout << "sum of renomalisation errors: " << fabs(0.25 * (errorsum[0] + errorsum[1] + errorsum[2] + errorsum[3])) << std::endl;
}

#endif
