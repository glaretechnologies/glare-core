/*=====================================================================
Checksum.h
-------------------
Copyright Glare Technologies Limited 2011 -
Generated at 2011-10-16 17:30:01 +0100
=====================================================================*/
#pragma once


#include "Platform.h"


#include <stdlib.h>


/*=====================================================================
Checksum
-------------------

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
private:

};



