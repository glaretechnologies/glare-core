/*=====================================================================
LargePageAllocation.cpp
-----------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "LargePageAllocation.h"


#include "../maths/mathstypes.h"
#include "../utils/IncludeWindows.h"
#include "../utils/PlatformUtils.h"
#include "../utils/Exception.h"
#include "../utils/StringUtils.h"


void LargePageAllocation::enableLockMemoryPrivilege()
{
#if defined(_WIN32)
	HANDLE token;
	if(!::OpenProcessToken(::GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token))
		throw Indigo::Exception("OpenProcessToken failed: " + PlatformUtils::getLastErrorString());

	TOKEN_PRIVILEGES tp;
	if (::LookupPrivilegeValue(NULL, // lpSystemName - use NULL to specify local system
		SE_LOCK_MEMORY_NAME, // privilege name
		&(tp.Privileges[0].Luid))) // privilege UID
	{
		tp.PrivilegeCount = 1;
		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		if(::AdjustTokenPrivileges(token, /*DisableAllPrivileges=*/FALSE, /*NewState=*/&tp, /*prev state buf len=*/0, /*PreviousState=*/NULL, /*ReturnLength=*/NULL))
		{
			if(GetLastError() != ERROR_SUCCESS)
				throw Indigo::Exception("AdjustTokenPrivileges failed to set all privileges: " + PlatformUtils::getLastErrorString());
		}
		else
			throw Indigo::Exception("AdjustTokenPrivileges failed: " + PlatformUtils::getLastErrorString());
	}
	else
		throw Indigo::Exception("LookupPrivilegeValue failed: " + PlatformUtils::getLastErrorString());


	::CloseHandle(token);
#endif
}


void* LargePageAllocation::allocateLargePageMem(size_t size)
{
#if defined(_WIN32)
	const size_t large_page_size = GetLargePageMinimum();

	const size_t use_alloc_size = Maths::roundUpToMultiple(size, large_page_size);

	void* mem = VirtualAlloc(
		NULL,										// _In_opt_  LPVOID lpAddress,
		use_alloc_size,								//_In_      SIZE_T dwSize,
		MEM_COMMIT | MEM_RESERVE | MEM_LARGE_PAGES,	//_In_      DWORD flAllocationType,
		PAGE_READWRITE								//_In_      DWORD flProtect
	);
	if(!mem)
		throw Indigo::Exception("Allocation of large page mem of size " + toString(use_alloc_size) + " B failed: " + PlatformUtils::getLastErrorString());
	return mem;
#else
	throw Indigo::Exception("allocateLargePageMem not implemented.");
#endif
}


void LargePageAllocation::freeLargePageMem(void* mem)
{
	if(mem)
		VirtualFree(mem, 0, MEM_RELEASE);
}


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../utils/ConPrint.h"


void LargePageAllocation::test()
{
#if defined(_WIN32)
	try
	{
		enableLockMemoryPrivilege();

		const size_t large_page_size = GetLargePageMinimum();
		printVar(large_page_size);

		{
			const size_t size = 10000000;
			void* mem = allocateLargePageMem(size);
			conPrint("Successfully allocated " + toString(size) + " B with large pages.");
			freeLargePageMem(mem);
		}
	}
	catch(Indigo::Exception& e)
	{
		// Large page allocation may fail, seemingly due to memory fragmentation, which we can't control, so don't fail the test in that case.
		conPrint("Large page test failed: " + e.what());
	}
#endif
}


#endif // BUILD_TESTS
