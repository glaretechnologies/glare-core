/*=====================================================================
zKey.h
------
Copyright Glare Technologies Limited 2010 -
Generated at Fri Oct 29 14:08:44 +1300 2010
=====================================================================*/
#pragma once


#include "Platform.h"


/*=====================================================================
zKey
----

=====================================================================*/
template<int dimensions>
class zKey
{
public:
	zKey(){}

	zKey(const uint32_t _coords[dimensions], const uint32_t _val)
	{
		computeKey(_coords);
		val = _val;
	}

	~zKey(){}

	uint32_t key[dimensions];
	uint32_t val;

	void computeKey(const uint32_t _coords[dimensions])
	{
		for(uint32_t d = 0; d < dimensions; ++d) key[d] = 0;

		for(uint32_t d = 0; d < dimensions; ++d)
			interleaveBits(d, _coords[d]);
	}

	void computeKeyPtr(const uint32_t* const _coords)
	{
		for(uint32_t d = 0; d < dimensions; ++d) key[d] = 0;

		for(uint32_t d = 0; d < dimensions; ++d)
			interleaveBits(d, _coords[d]);
	}

	inline bool operator < (const zKey& rhs) const
	{
		for(int d = dimensions - 1; d >= 0; --d)
			if(key[d] != rhs.key[d]) return key[d] < rhs.key[d];
		return false;
	}

private:

	inline void interleaveBits(const uint32_t dim_idx, const uint32_t dim_val)
	{
		for(uint32_t i = 0; i < 32; ++i)
		{
			uint32_t bit_index = i * dimensions + dim_idx;
			key[bit_index / 32] |= ((dim_val >> i) & 1) << (bit_index % 32);
		}
	}
};
