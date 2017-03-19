/*=====================================================================
BitVector.h
-------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include "../maths/mathstypes.h"
#include <vector>
#include <cassert>


/*=====================================================================
BitVector
-------------------
An array of bits.  Something like std::vector<bool>, but should
be more efficient.
=====================================================================*/
class BitVector
{
public:
	inline BitVector() : num_bits(0) {}

	// Bits are intialised to zero.
	inline explicit BitVector(size_t num_bits_) : v(Maths::roundedUpDivide<size_t>(num_bits_, 32), 0), num_bits(num_bits_) {}

	inline void setAllBits(uint32 newval);

	inline void resize(size_t new_num_bits);
	inline size_t size() const { return num_bits; }
	
	// Return the value of the bit at the given index.  Returns 0 or 1.
	inline uint32 getBit(size_t index) const;

	// Isolates the given bit with a mask, but does not shift it down to index 0.
	// Will return zero if the bit was zero, or non-zero otherwise.
	// For example if the bit at index 3 has value 1, getBitMasked(3) will return 8 (1000b).
	inline uint32 getBitMasked(size_t index) const;

	// Return the value of the bit pair at bits (index + 1, index).
	// Returns a value in [0, 4).
	inline uint32 getBitPair(size_t index) const;

	inline void setBitToZero(size_t index);
	inline void setBitToOne(size_t index);
	inline void setBit(size_t index, uint32 newval); // newval should be 0 or 1.
	inline void setBitPair(size_t index, uint32 newval); // newval should be in [0, 4)

	std::vector<uint32> v;
	size_t num_bits;
};


void bitVectorTests();


void BitVector::setAllBits(uint32 newval)
{
	if(newval)
	{
		for(size_t i=0; i<v.size(); ++i)
			v[i] = 0xFFFFFFFFu;
	}
	else
	{
		for(size_t i=0; i<v.size(); ++i)
			v[i] = 0;
	}
}


void BitVector::resize(size_t new_num_bits)
{
	num_bits = new_num_bits;
	v.resize(Maths::roundedUpDivide<size_t>(new_num_bits, 32));
}


inline uint32 BitVector::getBit(size_t index) const
{
	assert(index >= 0 && index < num_bits);

	size_t e = index >> 5;
	size_t i = index & 0x1F;

	return (v[e] & (1u << i)) >> i;
}


inline uint32 BitVector::getBitMasked(size_t index) const
{
	assert(index >= 0 && index < num_bits);

	size_t e = index >> 5;
	size_t i = index & 0x1F;

	return v[e] & (1u << i);
}


inline uint32 BitVector::getBitPair(size_t index) const
{
	assert(index >= 0 && index < num_bits);

	size_t e = index >> 5;
	size_t i = index & 0x1F;

	return (v[e] & (3u << i)) >> i;
}


inline void BitVector::setBitToZero(size_t index)
{
	assert(index >= 0 && index < num_bits);

	size_t e = index >> 5;
	size_t i = index & 0x1F;

	v[e] &= ~(1u << i);
}


inline void BitVector::setBitToOne(size_t index)
{
	assert(index >= 0 && index < num_bits);

	size_t e = index >> 5;
	size_t i = index & 0x1F;

	v[e] |= (1u << i);
}


inline void BitVector::setBit(size_t index, uint32 newval)
{
	assert(index >= 0 && index < num_bits);
	assert(newval == 0 || newval == 1);

	size_t e = index >> 5;
	size_t i = index & 0x1F;

	// Zero the bit at index in v[e], then
	// get the lowst bit of newval, AND it with 1, shift it left to the correct index, then OR it with v[e].
	v[e] = (v[e] & ~(1u << i)) | (newval << i);
}


// Set bit at index and index + 1.
inline void BitVector::setBitPair(size_t index, uint32 newval)
{
	assert(index >= 0 && index + 1 < num_bits);
	assert(newval >= 0 && newval < 4);

	size_t e = index >> 5;
	size_t i = index & 0x1F;

	v[e] = (v[e] & ~(3u << i)) | (newval << i);
}
