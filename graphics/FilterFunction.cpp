#include "FilterFunction.h"

#include <iostream>
#include <math.h>
#include "../maths/SSE.h"


FilterFunction::FilterFunction()
{
	cached_filter_data = 0;
	cached_supersample_factor = 0;
}


FilterFunction::~FilterFunction()
{
	_mm_free(cached_filter_data);
}


int FilterFunction::getFilterSpan(int supersample_factor)
{
	return (int)ceil(supportRadius() * 2) * supersample_factor;
}


float* FilterFunction::getFilterData(int supersample_factor)
{
	//assert(supersample_factor > 1);
	//if(supersample_factor == cached_supersample_factor) return cached_filter_data;

	const int filter_pixel_span  = (int)ceil(supportRadius() * 2) * supersample_factor;
	const int filter_pixel_bound = filter_pixel_span / 2 - 1;

	_mm_free(cached_filter_data);
	cached_filter_data = (float*)_mm_malloc((filter_pixel_span - 1) * (filter_pixel_span - 1) * sizeof(float), 64);
	cached_supersample_factor = supersample_factor;

	double filter_sum = 0;
	for(int v = -filter_pixel_bound; v <= filter_pixel_bound; ++v)
	{
		double partial_sum = 0;

		for(int u = -filter_pixel_bound; u <= filter_pixel_bound; ++u)
		{
			const double du = u / (double)(filter_pixel_span / 2);
			const double dv = v / (double)(filter_pixel_span / 2);
			const double r  = sqrt(du * du + dv * dv);

			partial_sum += eval(r);
		}

		filter_sum += partial_sum;
	}

	uint32 filter_addr = 0;
	for(int v = -filter_pixel_bound; v <= filter_pixel_bound; ++v)
	for(int u = -filter_pixel_bound; u <= filter_pixel_bound; ++u)
	{
		const double du = u / (double)(filter_pixel_span / 2);
		const double dv = v / (double)(filter_pixel_span / 2);
		const double r  = sqrt(du * du + dv * dv);

		cached_filter_data[filter_addr++] = (float)(eval(r) / filter_sum);
	}

	return cached_filter_data;
}
