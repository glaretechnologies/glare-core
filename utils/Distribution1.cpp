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

template<>
void Distribution1<float>::test()
{
	int num_bins = 4096;

	std::vector<uint32> bins(num_bins);
	std::vector<uint32> resampled_bins(num_bins);

	std::vector<float> pdf(num_bins);
	for(int i = 0; i < num_bins; ++i)
	{
		bins[i] = resampled_bins[i] = 0;
		pdf[i] = 1;
	}
	pdf[num_bins / 2] = 10;


	Distribution1 dist(pdf, 0.25);


	uint32 num_samples = num_bins * 768 * 64;
	MTwister rng(631579123);


	Timer timer;
	timer.reset();

	for(uint32 i = 0; i < num_samples; ++i)
	{
		float u = rng.unitRandom();
		float resampled_u;

		bins[dist.sampleLerp(u, resampled_u)]++;
		resampled_bins[((resampled_u == 1) ? num_bins - 1 : (uint32)(resampled_u * num_bins))]++;
	}

	double time_taken = timer.elapsed();


	if(false)
	{
		for(int i = 0; i < num_bins; ++i) std::cout << "bin " << i << ":\t\t\t" << bins[i] << std::endl;
		for(int i = 0; i < num_bins; ++i) std::cout << "bin resampled " << i << ":\t" << resampled_bins[i] << std::endl;
	}

	std::cout << "simulation took " << time_taken << " seconds." << std::endl;
}

#endif
