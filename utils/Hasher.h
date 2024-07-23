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
