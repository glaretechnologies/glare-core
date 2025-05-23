/*=====================================================================
FilterFunction.cpp
------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#include "FilterFunction.h"


#include <math.h>


FilterFunction::FilterFunction()
{
}


FilterFunction::~FilterFunction()
{
}


int FilterFunction::getFilterSpan(size_t supersample_factor) const
{
	return (int)ceil(supportRadius() * 2) * (int)supersample_factor;
}


void FilterFunction::getFilterDataVec(size_t supersample_factor, std::vector<float>& filter_data_out) const
{
	const double filter_pixel_span_f = supportRadius() * 2 * supersample_factor;
	const int filter_pixel_span  = (int)ceil(supportRadius() * 2) * (int)supersample_factor;
	const int filter_pixel_bound = filter_pixel_span / 2 - 1;

	filter_data_out.resize((filter_pixel_span - 1) * (filter_pixel_span - 1));

	double filter_sum = 0;
	for(int v = -filter_pixel_bound; v <= filter_pixel_bound; ++v)
	{
		double partial_sum = 0;

		for(int u = -filter_pixel_bound; u <= filter_pixel_bound; ++u)
		{
			const double du = u / (filter_pixel_span_f / 2);
			const double dv = v / (filter_pixel_span_f / 2);
			const double r  = sqrt(du * du + dv * dv);

			partial_sum += eval(r);
		}

		filter_sum += partial_sum;
	}

	size_t filter_addr = 0;
	for(int v = -filter_pixel_bound; v <= filter_pixel_bound; ++v)
	for(int u = -filter_pixel_bound; u <= filter_pixel_bound; ++u)
	{
		const double du = u / (filter_pixel_span_f / 2);
		const double dv = v / (filter_pixel_span_f / 2);
		const double r  = sqrt(du * du + dv * dv);

		filter_data_out[filter_addr++] = (float)(eval(r) / filter_sum);
	}
}
