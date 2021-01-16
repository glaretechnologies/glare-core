/*=====================================================================
Checksum.h
----------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <stdlib.h>


/*=====================================================================
Checksum
-------------------
Does CRC32 bit checksums.
Old and slow, do not use for new code!
Use xxhash instead.
=====================================================================*/
class Checksum
{
public:

	// Checksum a single buffer.
	static uint32 checksum(const void* data, size_t datalen);

	// Initialise a checksum value.
	static uint32 initChecksum();

	// Update a checksum value with a new buffer.
	static uint32 updateChecksum(uint32 running_checksum, const void* data, size_t datalen);

	template <class T>
	static uint32 updateChecksumBasicType(uint32 running_checksum, T t)
	{
		return updateChecksum(running_checksum, &t, sizeof(T));
	}

	static void test();
};
