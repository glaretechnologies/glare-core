/*=====================================================================
SSE.cpp
-------
File created by ClassTemplate on Sat Jun 25 08:01:25 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "SSE.h"

#include "../utils/platform.h"
#include "../utils/timer.h" // just for testing
#include "../indigo/globals.h" // just for testing
#include "../utils/stringutils.h" // just for testing

#ifdef COMPILER_MSVC
#include <intrin.h>
#else

#endif


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

#ifdef COMPILER_MSVC
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
#else
	mmx_present = sse1_present = sse2_present = sse3_present = true; //TEMP HACK
#endif

	//TODO: test this shit
}


void SSETest()
{
	conPrint("SSETest()");


	// Test accuracy of divisions
	const SSE_ALIGN float a[4] = {1.0e0f, 1.0e-1f, 1.0e-2f, 1.0e-3f};
	const SSE_ALIGN float b[4] = {1.1f,1.01f,1.001f,1.0001f};
	const SSE_ALIGN float c[4] = {0.99999999999,0.99999999,0.9999999,0.999999};
	SSE_ALIGN float r1[4] = {1,1,1,1};
	SSE_ALIGN float r2[4] = {1,1,1,1};

	// r1 := a / (b-c)

	_mm_store_ps(
		r1,
		_mm_div_ps(
			_mm_load_ps(a),
			_mm_sub_ps(
				_mm_load_ps(b),
				_mm_load_ps(c)
				)
			)
		);

	for(int z=0; z<4; ++z)
	{
		r2[z] = a[z] / (b[z] - c[z]);

		conPrint(::floatToString(r1[z], 20));
		conPrint(::floatToString(r2[z], 20));
	}















	const int N = 20000000 * 4;
	float* data1 = (float*)alignedMalloc(sizeof(float) * N, 16);
	float* data2 = (float*)alignedMalloc(sizeof(float) * N, 16);
	float* res = (float*)alignedMalloc(sizeof(float) * N, 16);

	for(int i=0; i<N; ++i)
	{
		data1[i] = 1.0;
		data2[i] = (float)(i + 1);
	}

	conPrint("running divisions...");

	Timer timer;

	/*for(int i=0; i<N; ++i)
	{
		res[i] = data1[i] / data2[i];
	}*/
	for(int i=0; i<N; i+=4)
	{
		/*const __m128 d1 = _mm_load_ps(data1 + i);
		const __m128 d2 = _mm_load_ps(data2 + i);
		const __m128 r = _mm_div_ps(d1, d2);
		_mm_store_ps(res + i, r);*/
		store4Vec(div4Vec(load4Vec(data1 + i), load4Vec(data2 + i)), res + i);
	}

	const double elapsed_time = timer.getSecondsElapsed();
	const double clock_freq = 2.4e9;
	const double elapsed_cycles = elapsed_time * clock_freq;
	const double cycles_per_iteration = elapsed_cycles / (double)N;

	double sum = 0;
	for(int i=0; i<N; ++i)
		sum += (double)res[i];

	printVar(sum);
	printVar(elapsed_time);
	printVar(elapsed_cycles);
	printVar(cycles_per_iteration);

	alignedFree(data1);
	alignedFree(data2);
	alignedFree(res);
}
