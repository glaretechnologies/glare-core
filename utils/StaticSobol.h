/*=====================================================================
StaticSobol.h
-------------
Copyright Glare Technologies Limited 2011 -
Generated at Mon Jul 30 23:42:17 +0100 2012
=====================================================================*/
#pragma once


#include <string>
#include "Platform.h"
#include "Vector.h"
#include "../maths/SSE.h"


/*=====================================================================
StaticSobol
-----------
Fast, static evaluation of Sobol sequences.
Based on Carsten Waechter's PhD thesis: http://vts.uni-ulm.de/query/longview.meta.asp?document_id=6265
=====================================================================*/
SSE_CLASS_ALIGN StaticSobol
{
public:
	INDIGO_ALIGNED_NEW_DELETE

	StaticSobol(const std::string& indigo_base_dir_path);
	~StaticSobol();


	const static uint32 num_dims = 21200; // We actually have 21201 dimensions but round it down


	// Evaluate the d'th dimension of the given Sobol sample number, without randomisation
	inline uint32 evalSample(const uint32 sample_num, const uint32 d) const
	{
		assert(d < num_dims);
		const __m128 * const dir_nums_SSE = (const __m128 * const)&dir_nums[0];

		const __m128 result =
			_mm_xor_ps(
				_mm_xor_ps(
					_mm_xor_ps(	_mm_and_ps(xor_LUT[sample_num         & 15], dir_nums_SSE[d * 8 + 0]),
								_mm_and_ps(xor_LUT[(sample_num >>  4) & 15], dir_nums_SSE[d * 8 + 1])),
					_mm_xor_ps(	_mm_and_ps(xor_LUT[(sample_num >>  8) & 15], dir_nums_SSE[d * 8 + 2]),
								_mm_and_ps(xor_LUT[(sample_num >> 12) & 15], dir_nums_SSE[d * 8 + 3]))),
				_mm_xor_ps(
					_mm_xor_ps(	_mm_and_ps(xor_LUT[(sample_num >> 16) & 15], dir_nums_SSE[d * 8 + 4]),
								_mm_and_ps(xor_LUT[(sample_num >> 20) & 15], dir_nums_SSE[d * 8 + 5])),
					_mm_xor_ps(	_mm_and_ps(xor_LUT[(sample_num >> 24) & 15], dir_nums_SSE[d * 8 + 6]),
								_mm_and_ps(xor_LUT[(sample_num >> 28)     ], dir_nums_SSE[d * 8 + 7]))));

		const __m128 merged = _mm_xor_ps(result, _mm_movehl_ps(result, result));
		const uint32_t x_d = ((uint32_t *)&merged)[0] ^ ((uint32_t *)&merged)[1];

		return x_d;
	}


	// Evaluate the d'th dimension of the given Sobol sample number, with simple static randomisation
	inline uint32 evalSampleRandomised(const uint64 sample_num, const uint32 d) const
	{
		const uint32 sample_num_32 = (uint32)sample_num;			// Get Sobol index from low 32bits of sample number
		const uint32 random_index  = (uint32)(sample_num >> 32);	// Get randomisation vector index from high 32bits of sample number

		// Note that there is expected uint32 wraparound in adding the randomisation vector
		return evalSample(sample_num_32, d) + static_random(random_index * 21211 + d);
	}

	void evalSampleBlock(const uint32_t base_sample_idx, const uint32 sample_block_size, const uint32 sample_depth, uint32_t * const samples_out) const;

	static void test(const std::string& indigo_base_dir_path);

	
private:

	inline float floatCast(uint32 x) { const float f = (float&)x; return f; }


	inline uint32 static_random(uint32 x) const
	{
		x  = (x ^ 12345391u) * 2654435769u;
		x ^= (x << 6) ^ (x >> 26);
		x *= 2654435769u;
		x += (x << 5) ^ (x >> 12);

		return x;
	}


	// This method is kept private since the SSE version is much faster, but having an unoptimised reference in normal C can be useful
	inline uint32 evalSampleRef(const uint32 sample_num, const uint32 d) const;

	inline uint32 evalSampleRefRandomised(const uint64 sample_num, const uint32 d) const;


	// Small lookup table to accelerate the SIMD xor-computations in evaluation.
	__m128 xor_LUT[16];

public:
	// The Sobol "direction numbers"
	js::Vector<uint32, 16> dir_nums;

	// Transposed direction numbers
	js::Vector<uint32, 16> transposed_dir_nums;
};
