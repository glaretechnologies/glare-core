/*=====================================================================
DiscreteDistribution.h
----------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "../utils/Vector.h"
#include "../utils/Platform.h"
#include <vector>
#include <algorithm>


/*=====================================================================
DiscreteDistribution
-------------------
Discrete probability distribution.
Uses signed 32 bit offsets, so can have a maximum of 2^31 elements.
=====================================================================*/
class DiscreteDistribution
{
public:
	typedef float Real;

	DiscreteDistribution();
	DiscreteDistribution(const std::vector<Real>& data);
	~DiscreteDistribution();

	void build(const std::vector<Real>& data);


	inline uint32 sample(Real unitsample, Real& p_out) const;
	uint32 sampleReference(Real unitsample) const;

	inline Real probability(uint32 i) const;

	const std::vector<Real>& getCDF() const { return cdf; }
	int getN() const { return N; }; // num items in distribution.
	Real getRecipN() const { return recip_N; };


	static void test();

private:
	std::vector<Real> cdf;
	std::vector<int> buckets;
	int num_buckets;
	int N;
	Real recip_N;
};


uint32 DiscreteDistribution::sample(Real unitsample, Real& p_out) const
{
	assert(unitsample >= 0.0f && unitsample < 1.0f);

	// If unitsample is < 0, then upper_bound will return 0, resulting in an access to cdf[-1], which we need to avoid.
	/*if(unitsample < 0)
	{
		p_out = 1;
		return 0;
	}*/

	// Do bucket lookup
	const int b_i = myMin((int)(unitsample * (float)num_buckets), num_buckets-1);
	//const int b_i = myClamp((int)(unitsample * (float)num_buckets), 0, num_buckets-1);

	int upper = buckets[b_i]; // Upper is the index of the first CDF value > bucket upper bound.

	int lower;
	// TODO: could remove this branch with a bit of care.
	if(b_i == 0)
		lower = 0;
	else
		lower = buckets[b_i - 1] - 1;

//	assert(cdf[lower] <= unitsample && cdf[upper] >= unitsample);

	const std::vector<Real>::const_iterator it = std::upper_bound(cdf.begin() + lower, cdf.begin() + upper + 1, unitsample);

	unsigned int i = (unsigned int)(it - cdf.begin());
	assert(i > 0); // cdf[0] = 0, and unitsample >= 0, so 'i' should be > 0.

	// If unitsample >= 1, unitsample will be greater than all cdfs, so will return end() = cdf.size() = N + 1
	/*if(i > (unsigned int)N)
	{
		p_out = 1;
		return N - 1;
	}*/

	unsigned int prev_i = i - 1;

	p_out = cdf[i] - cdf[prev_i];

	return prev_i;
}


DiscreteDistribution::Real DiscreteDistribution::probability(uint32 i) const
{
	return cdf[i+1] - cdf[i];
}
