/*=====================================================================
BitField.h
-------------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <cassert>


/*=====================================================================
BitField
-------------------
Bit field on an underlying unsigned integer type.
Similar to std::bitset<T>, but lower-level and faster.
=====================================================================*/
template <class T>
class BitField
{
public:
	inline BitField() {}
	inline explicit BitField(T t) : v(t) {}
	
	// Return the value of the bit at the given index.  Returns 0 or 1.
	inline uint32 getBit(uint32 index) const;

	// Isolates the given bit with a mask, but does not shift it down to index 0.
	// Will return zero if the bit was zero, or non-zero otherwise.
	// For example if the bit at index 3 has value 1, getBitMasked(3) will return 8 (1000b).
	inline uint32 getBitMasked(uint32 index) const;

	// Return the value of the bit pair at bits (index + 1, index).
	// Returns a value in [0, 4).
	inline uint32 getBitPair(uint32 index) const;
	inline uint32 getBitPairICEWorkaround(uint32 index) const; // Workaround for internal compiler error in VS2015.

	inline void setBitToZero(uint32 index);
	inline void setBitToOne(uint32 index);
	inline void setBit(uint32 index, uint32 newval); // newval should be 0 or 1.
	inline void setBitPair(uint32 index, uint32 newval); // newval should be in [0, 4)

	T v;
};


void bitFieldTests();


template <class T>
inline uint32 BitField<T>::getBit(uint32 index) const
{
	assert(index >= 0 && index < sizeof(T) * 8);

	return (v >> index) & 1u;
}


template <class T>
inline uint32 BitField<T>::getBitMasked(uint32 index) const
{
	assert(index >= 0 && index < sizeof(T) * 8);

	return v & (1u << index);
}


template <class T>
inline uint32 BitField<T>::getBitPair(uint32 index) const
{
	assert(index >= 0 && index < sizeof(T) * 8);

	return (v >> index) & 3u;
}


template <class T>
inline uint32 BitField<T>::getBitPairICEWorkaround(uint32 index) const
{
	assert(index >= 0 && index < sizeof(T) * 8);

	return ((int)v >> index) & 3u; // The cast to int here prevents the ICE.
}


template <class T>
inline void BitField<T>::setBitToZero(uint32 index)
{
	assert(index >= 0 && index < sizeof(T) * 8);

	v &= ~((T)1u << index);
}


template <class T>
inline void BitField<T>::setBitToOne(uint32 index)
{
	assert(index >= 0 && index < sizeof(T) * 8);

	v |= ((T)1u << index);
}


template <class T>
inline void BitField<T>::setBit(uint32 index, uint32 newval)
{
	assert(index >= 0 && index < sizeof(T) * 8);
	assert(newval == 0 || newval == 1);

	// Zero the bit at index in v, then
	// get the lowst bit of newval, AND it with 1, shift it left to the correct index, then OR it with v.
	v = (v & ~(1u << index)) | (newval << index);
}


// Set bit at index and index + 1.
template <class T>
inline void BitField<T>::setBitPair(uint32 index, uint32 newval)
{
	assert(index >= 0 && index + 1 < sizeof(T) * 8);
	assert(newval >= 0 && newval < 4);

	v = (v & ~(3u << index)) | (newval << index);
}
