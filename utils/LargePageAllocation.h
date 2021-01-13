/*=====================================================================
LargePageAllocation.h
---------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "Platform.h"
#include <cstring> // for size_t


/*=====================================================================
LargePageAllocation
-------------------
See /wiki/index.php?title=Large_page_allocation
=====================================================================*/
class LargePageAllocation
{
public:
	static void enableLockMemoryPrivilege(); // Throws glare::Exception on failure.

	// Rounds allocated size up to a multiple of the large page size.
	static void* allocateLargePageMem(size_t size); // Throws glare::Exception on failure.

	static void freeLargePageMem(void* mem);

	static void test();
};
