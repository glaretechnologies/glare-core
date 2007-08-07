/*=====================================================================
SSE.cpp
-------
File created by ClassTemplate on Sat Jun 25 08:01:25 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "SSE.h"

#include <intrin.h>


/*
void alignedFree(void* mem)
{
	_aligned_free(mem);
}
*/
/*
Uses the CPU_ID instruction to check for the presence of SSE
*/
void SSE::checkForSSE(bool& mmx_present, bool& sse1_present, bool& sse2_present, bool& sse3_present)
{
	/*unsigned int cpeinfo;
	unsigned int cpsse3;
	unsigned int temp_ebx;
	__asm 
	{
	mov temp_ebx, ebx //save ebx
	mov eax, 01h 
	cpuid 
	mov cpeinfo, edx 
	mov cpsse3, ecx 
	mov ebx, temp_ebx; //restore ebx
	}

	const unsigned int MMX_FLAG = 0x0800000;
	const unsigned int SSE_FLAG = 0x2000000;
	const unsigned int SSE2_FLAG = 0x4000000;
	const unsigned int SSE3_FLAG = 0x0000001;
	const unsigned int SSSE3_FLAG = 0x0000200;

	mmx_present = (cpeinfo & MMX_FLAG ) != 0;
	sse1_present = (cpeinfo & SSE_FLAG ) != 0;
	sse2_present = (cpeinfo & SSE2_FLAG ) != 0;
	sse3_present = (cpsse3 & SSE3_FLAG ) != 0;*/
	int CPUInfo[4];
	__cpuid(
		CPUInfo, 
		1 //infotype
		);

	const int MMX_FLAG = 1 << 23;
	const int SSE_FLAG = 1 << 25;
	const int SSE2_FLAG = 1 << 26;
	const int SSE3_FLAG = 1;
	mmx_present = (CPUInfo[3] & MMX_FLAG ) != 0;
	sse1_present = (CPUInfo[3] & SSE_FLAG ) != 0;
	sse2_present = (CPUInfo[3] & SSE2_FLAG ) != 0;
	sse3_present = (CPUInfo[2] & SSE3_FLAG ) != 0;

	//TODO: test this shit
}


