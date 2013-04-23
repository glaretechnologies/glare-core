/*=====================================================================
DeltaSigmaSampler.cpp
---------------------
Copyright Glare Technologies Limited 2010 -
Generated at Fri Jul 22 18:12:30 +0100 2011
=====================================================================*/
#include "DeltaSigmaSampler.h"

#include "stringutils.h"

#include <iostream>

template <>
void DeltaSigmaSampler<RampFunction, RampFunction>::test()
{
	//int num_primes = 50;
	int primes[50] =
	{
		  2,   3,   5,   7,  11,  13,  17,  19,  23,  29,	// 10
		 31,  37,  41,  43,  47,  53,  59,  61,  67,  71,	// 20
		 73,  79,  83,  89,  97, 101, 103, 107, 109, 113,	// 30
		127, 131, 137, 139, 149, 151, 157, 163, 167, 173,	// 40
		179, 181, 191, 193, 197, 199, 211, 223, 227, 229	// 50
	};

	const int num_dims = 2;
	const int start_dim = 0;
	int dimension_primes[2] = { start_dim + 0, start_dim + 1 };

	std::vector<int> dimension_prime_vec(num_dims);
	for (int i = 0; i < num_dims; ++i)
		dimension_prime_vec[i] = primes[dimension_primes[i]];

	std::cout << "using bases " << dimension_prime_vec[0] << " x " << dimension_prime_vec[1] << std::endl;


	const int final_samples = dimension_prime_vec[0] * dimension_prime_vec[1];
	const double oversample_factor = 4;

	const double ideal_res = pow(final_samples * oversample_factor, 1.0 / num_dims);
	std::cout << "ideal grid res for " << (int)(final_samples * oversample_factor) << " samples: " << ideal_res << std::endl;

	std::cout << "grid res = ";
	std::vector<int> dim_res(num_dims);
	for (int d = 0; d < num_dims; ++d)
	{
		const int exp_d = (int)(log(ideal_res) / log((double)dimension_prime_vec[d]) + 0.5);
		dim_res[d] = (int)pow((double)dimension_prime_vec[d], (double)exp_d);

		std::cout << dim_res[d] << ((d != num_dims-1) ? " x " : "\n");
	}

	DeltaSigmaSampler<RampFunction, RampFunction> dss;


	const double integral = dss.integrate(RampFunction(), RampFunction(), dimension_prime_vec, dim_res, final_samples);
	std::cout << "approximated integral = " << integral << std::endl;



}



