/*=====================================================================
StaticSobol.cpp
---------------
Copyright Glare Technologies Limited 2011 -
Generated at Mon Jul 30 23:42:17 +0100 2012
=====================================================================*/
#include "StaticSobol.h"


#include "../indigo/SobolData.h"
#include "Exception.h"
#include "FileUtils.h"
#include "BitUtils.h"
#include <vector>
#include <fstream>


inline static float floatCast(const uint32 x)
{
	float f;
	memcpy(&f, &x, sizeof(uint32)); // Let's hope the compiler optimises this silly little dance away...

	return f;
}


StaticSobol::StaticSobol()
{
	std::vector<uint32>::const_iterator dir_iter = ::getGlobalSobolData()->direction_nums.begin();


	// Allocate temporary memory used during direction number computation
	uint32 **V = new uint32*[num_dims];
	for(uint32 i = 0; i < num_dims; ++i)
		V[i] = new uint32[32 + 1];


	// Compute direction numbers V[1] to V[L], scaled by pow(2,32)
	for(uint32 i = 1; i <= 32; i++)
		V[0][i] = 1 << (32 - i); // all m's = 1

	// ----- Read V for other dimensions -----
	std::vector<uint32> m(1024);
	for(uint32 j = 1; j <= num_dims - 1; j++)
	{
		// Read in parameters from file 
		dir_iter++; // Skip unused "d" parameter
		const uint32 s = *dir_iter++, a = *dir_iter++;

		for(uint32 i = 1; i <= s; i++)
			m[i] = *dir_iter++;
	    
		// Compute direction numbers V[1] to V[L], scaled by pow(2,32)
		if(32 <= s)
		{
			for(uint32 i = 1; i <= 32; i++)
				V[j][i] = m[i] << (32 - i);
		}
		else
		{
			for(uint32 i = 1; i <= s; i++)
				V[j][i] = m[i] << (32 - i);

			for(uint32 i = s + 1; i <= 32; i++)
			{
				V[j][i] = V[j][i - s] ^ (V[j][i - s] >> s);

				for(uint32 k = 1; k <= s - 1; k++)
					V[j][i] ^= (((a >> (s - 1 - k)) & 1) * V[j][i - k]);
			}
		}
	}

	// "Flatten" the direction numbers into an aligned 1D array, suitable for SSE access
	// This also removes the extra array dimension (32 instead of 33)
	dir_nums.resize(num_dims * 32);
	for(uint32 d = 0; d < num_dims; ++d)
	for(uint32 z = 0; z < 32; ++z)
		dir_nums[d * 32 + z] = V[d][z + 1];

	transposed_dir_nums.resize(num_dims * 32);
	for (int i = 0; i < 32; ++i)
	for (int d = 0; d < num_dims; ++d)
		transposed_dir_nums[i * num_dims + d] = V[d][i + 1];

	// Free temporary memory used in intialising the Sobol direction numbers
	for(uint32 j = 0; j < num_dims; ++j)
		delete[] V[j];
	delete[] V;


	// Initialise the xor-table
	for(int c = 0; c < 16; ++c)
	{
		const int i[4] = { -(c & 1), -((c >> 1) & 1), -((c >> 2) & 1), -(c >> 3) };

		xor_LUT[c] = _mm_set_ps(floatCast(i[3]), floatCast(i[2]), floatCast(i[1]), floatCast(i[0]));
	}
}


StaticSobol::~StaticSobol()
{

}


void StaticSobol::evalSampleBlock(const uint32 base_sample_idx, const uint32 sample_block_size, const uint32 sample_depth, uint32 * const samples_out) const
{
	assert(sample_block_size % 16 == 0); // Because we unroll the sample computation with SSE

	// Compute the first sample via "random access" evaluation in greycode order
	{
		const uint32 graycode_sample_idx = base_sample_idx ^ (base_sample_idx / 2);
		for(uint32 z = 0; z < sample_depth; ++z)
			samples_out[z] = evalSample(graycode_sample_idx, z);
	}

	for(uint32 i = 1; i < sample_block_size; ++i)
	{
		const uint32 sample_idx = base_sample_idx + i;
		const uint32 bit_index = BitUtils::lowestZeroBitIndex(sample_idx);

		float const * const dir_nums_ = (float const * const)&transposed_dir_nums[bit_index * StaticSobol::num_dims];

		for (uint32 d = 0; d < sample_depth; d += 16)
		{
			const __m128 s0 = _mm_load_ps((const float *)&samples_out[(i-1) * sample_depth + d +  0]);
			const __m128 s1 = _mm_load_ps((const float *)&samples_out[(i-1) * sample_depth + d +  4]);
			const __m128 s2 = _mm_load_ps((const float *)&samples_out[(i-1) * sample_depth + d +  8]);
			const __m128 s3 = _mm_load_ps((const float *)&samples_out[(i-1) * sample_depth + d + 12]);

			const __m128 v0 = _mm_load_ps((const float *)&dir_nums_[d +  0]);
			const __m128 v1 = _mm_load_ps((const float *)&dir_nums_[d +  4]);
			const __m128 v2 = _mm_load_ps((const float *)&dir_nums_[d +  8]);
			const __m128 v3 = _mm_load_ps((const float *)&dir_nums_[d + 12]);

			_mm_store_ps((float *)&samples_out[i * sample_depth + d +  0], _mm_xor_ps(s0, v0));
			_mm_store_ps((float *)&samples_out[i * sample_depth + d +  4], _mm_xor_ps(s1, v1));
			_mm_store_ps((float *)&samples_out[i * sample_depth + d +  8], _mm_xor_ps(s2, v2));
			_mm_store_ps((float *)&samples_out[i * sample_depth + d + 12], _mm_xor_ps(s3, v3));
		}
	}
}


inline uint32 StaticSobol::evalSampleRef(const uint32 sample_num, const uint32 d) const
{
	assert(d < num_dims);
	uint32 x_d = 0;

	uint32_t k = (uint32_t)sample_num;
	for(uint32 j = 0; k; k >>= 1, j++)
		if(k & 1)
			x_d ^= dir_nums[d * 32 + j];

	return x_d;
}


inline uint32 StaticSobol::evalSampleRefRandomised(const uint64 sample_num, const uint32 d) const
{
	const uint32 sample_num_32 = (uint32)sample_num;			// Get Sobol index from low 32bits of sample number
	const uint32 random_index  = (uint32)(sample_num >> 32);	// Get randomisation vector index from high 32bits of sample number

	// Note that there is expected uint32 wraparound in adding the randomisation vector
	return evalSampleRef(sample_num_32, d) + static_random(random_index * 21211 + d);
}


#if BUILD_TESTS


#include "StringUtils.h"
#include "../indigo/SobolSequence.h" // For testing against reference Sobol implementation
#include "../utils/TestUtils.h"


void StaticSobol::test(const std::string& indigo_base_dir_path)
{
	try
	{
		StaticSobol sobol;

		// Test that we get the desired Sobol sequence
		{
			const uint32 max_dims = 32;
			const uint32 test_samples = 32;
			SobolSequence sobol_ref(max_dims);

			for(uint32 s = 0; s < test_samples; ++s)
			{
				SSE_ALIGN uint32 ref_sample[max_dims];
				sobol_ref.evalPoints(ref_sample); // Evaluate the reference Sobol sequence sample (in greycode order)

				const uint32 greycode_sample_idx = s ^ (s / 2); // Get greycode sample index to match orig Sobol order

				for(uint32 d = 0; d < max_dims; ++d)
				{
					const uint32 x_d = sobol.evalSampleRef(greycode_sample_idx, d);
					const uint32 x_d_SSE = sobol.evalSample(greycode_sample_idx, d);

					testAssert(x_d == ref_sample[d]);
					testAssert(x_d_SSE == ref_sample[d]);
				}
			}
		}

		// TODO test block eval

	}
	catch(glare::Exception& e)
	{
		failTest(e.what());
	}
}

#endif
