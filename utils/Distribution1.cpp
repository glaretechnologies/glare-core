/*=====================================================================
Distribution1.cpp
-------------------
Copyright Glare Technologies Limited 2010 -
Generated at Sun Aug 15 02:19:14 +1200 2010
=====================================================================*/
#include "Distribution1.h"


#include <iostream>
#include "../maths/SSE.h"


Distribution1::Distribution1()
{
	table_size = 0;

	cdfTable = 0;
	guideTable = 0;
}

Distribution1::Distribution1(const std::vector<Real>& pdf)
{
	table_size = (uint32)pdf.size() + 1;

	cdfTable = (Real*)_mm_malloc(table_size * sizeof(Real), 64);
	guideTable = (uint32*)_mm_malloc(table_size * sizeof(uint32), 64);

	// Compute the CDF table.
	//std::cout << "CDF" << std::endl;
	double running_cdf = 0;
	for(uint32 i = 0; i < pdf.size(); ++i)
	{
		running_cdf += (double)pdf[i];
		cdfTable[i]  = running_cdf;

		//std::cout << "i = " << i << ", CDF = " << running_cdf << std::endl;
	}
	cdfTable[pdf.size()] = 1;
	//std::cout << "i = " << pdf.size() << ", CDF = " << 1 << std::endl;

	// Build the guide table.
	//std::cout << "guide table" << std::endl;
	for(uint32 i = 0; i < table_size; ++i)
	{
		const double target_cdf = (double)i / (double)pdf.size();

		uint32 j = 0;
		while(cdfTable[j] < target_cdf) ++j;

		guideTable[i] = j;

		//std::cout << "i = " << i << ", target CDF = " << target_cdf << ", max j = " << j << std::endl;
	}
}

Distribution1::~Distribution1()
{
	_mm_free(cdfTable);
	_mm_free(guideTable);
}

#include <algorithm>

uint32 Distribution1::sample(const Real x) const
{
	assert(x >= 0 && x <= 1);

#if 1
	//uint32 i = 0;
	//for( ; i < table_size; ++i)
	//	if (cdfTable[i] >= x) break;

	const float* p = std::lower_bound(cdfTable, cdfTable + table_size, x);
	const uint32 i = (uint32)(p - cdfTable);

#else
	uint32 i = guideTable[(uint32)(x * table_size)] + 1;
	while((i > 0) && (cdfTable[i - 1]) >= x)
		--i;
#endif

	return i;
}

#if (BUILD_TESTS)

#include <iostream>
#include "MTwister.h"
#include "timer.h"

void Distribution1::test()
{
	int num_bins = 1024;

	std::vector<uint32> bins(num_bins);
	std::vector<Distribution1::Real> weights(num_bins);

	for(int i = 0; i < num_bins; ++i)
	{
		bins[i] = 0;
		weights[i] = 1.0f / (float)num_bins;
	}

	Distribution1 dist(weights);

	uint32 num_samples = 4294967296 / 16 - 1;

	MTwister rng(631579123);

	Timer timer;
	timer.reset();

	for(uint32 i = 0; i < num_samples; ++i)
	{
		const float  u = rng.unitRandom();
		const uint32 j = dist.sample(u);

		if(j >= num_bins)
		{
			std::cout << "invalid bin " << j << " for u = " << u << " at sample " << i << std::endl;
			return;
		}

		++bins[j];
	}

	double time_taken = timer.elapsed();
	std::cout << "simulation took " << time_taken << " seconds." << std::endl;

	//std::cout << "histogram:" << std::endl;
	//for(int i = 0; i < num_bins; ++i)
	//	std::cout << "bin " << i << ": " << bins[i] << std::endl;
}

#endif
