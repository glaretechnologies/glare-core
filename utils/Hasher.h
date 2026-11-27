/*=====================================================================
Hasher.h
--------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "Platform.h"


// Implemented using FNV-1a hash.
// See https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
// FNV is a good hash function for smaller key lengths.
// For longer key lengths, use something like xxhash.
// See http://aras-p.info/blog/2016/08/02/Hash-Functions-all-the-way-down/ for a performance comparison.
//
static inline size_t hashBytes(const uint8* data, size_t len)
{
#if defined(__x86_64__) || defined(__ia64__) || defined(_M_X64) // If 64-bit:
	
	uint64 hash = 14695981039346656037ull;
	for(size_t i=0; i<len; ++i)
		hash = (hash ^ data[i]) * 1099511628211ull;
	return hash;

#else

	uint32 hash = 2166136261u;
	for(size_t i=0; i<len; ++i)
		hash = (hash ^ data[i]) * 16777619u;
	return hash;

#endif
}


template <class T>
class HasherOb
{
public:
	inline size_t operator() (const T& v)
	{
		//static_assert(std::is_trivial<T>, "HasherOb can only handle trivial types.");

		return hashBytes((const uint8*)&v, sizeof(T));
	}
};


// Adapted from https://prng.di.unimi.it/splitmix.c and https://zimbry.blogspot.com/2011/09/better-bit-mixing-improving-on.html.
// We will use the Mix13 parameters from https://zimbry.blogspot.com/2011/09/better-bit-mixing-improving-on.html.
// This was measured to be about 2.7x faster than hashBytes() on a uint64.

/*  Written in 2015 by Sebastiano Vigna (vigna@acm.org)

To the extent possible under law, the author has dedicated all copyright
and related and neighboring rights to this software to the public domain
worldwide.

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR
IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */
inline uint64 hashUInt64(uint64 x)
{
	x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
	x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
	return x ^ (x >> 31);
}
