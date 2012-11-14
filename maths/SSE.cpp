/*=====================================================================
SSE.cpp
-------
File created by ClassTemplate on Sat Jun 25 08:01:25 2005
Copyright Glare Technologies Limited 2012 -
=====================================================================*/
#include "SSE.h"


#include "../utils/platform.h"
#include "../maths/mathstypes.h"
#include <assert.h>
#include <new>
#ifdef COMPILER_MSVC
#include <intrin.h>
#else
#include <stdlib.h>
#include <errno.h>
#endif


namespace SSE
{


void* alignedMalloc(size_t amount, size_t alignment)
{
	assert(Maths::isPowerOfTwo(alignment));
	
	// Suppose alignment is 16 = 0x10, so alignment - 1 = 15 = 0xF, also suppose original_addr = 0x01234567 
	// original_addr & (alignment - 1) = 0x01234567 & 0xF = 0x00000007
	// original_addr - (original_addr & (alignment - 1)) = 0x01234567 - 0x00000007 = 0x01234560

	const size_t padding = myMax(alignment, sizeof(unsigned int)) * 2;

	void* original_addr = malloc(amount + padding); // e.g. m = 0x01234567

	if(!original_addr)
		throw std::bad_alloc();

	// Snap the address down to the largest multiple of alignment <= original_addr
	const uintptr_t snapped_addr = (uintptr_t)original_addr - ((uintptr_t)original_addr & (alignment - 1));

	assert(snapped_addr % alignment == 0); // Check aligned
	assert(snapped_addr <= (uintptr_t)original_addr);
	assert((uintptr_t)original_addr - snapped_addr < alignment);

	void* returned_addr = (void*)(snapped_addr + padding); // e.g. 0x01234560 + 0x10 =  + 0xF = 0x01234570

	assert((uintptr_t)returned_addr - (uintptr_t)original_addr >= sizeof(unsigned int));

	*(((unsigned int*)returned_addr) - 1) = (unsigned int)((uintptr_t)returned_addr - (uintptr_t)original_addr); // Store offset from original address to returned address

	return returned_addr;
}


void alignedFree(void* addr)
{
	if(addr == NULL)
		return;

	const unsigned int original_addr_offset = *(((unsigned int*)addr) - 1);

	void* original_addr = (void*)((uintptr_t)addr - (uintptr_t)original_addr_offset);

	free(original_addr);
}


} // end namespace SSE


#if BUILD_TESTS


#include "../indigo/globals.h"
#include "../indigo/TestUtils.h"
#include "../utils/stringutils.h"
#include "../utils/timer.h"
#include "../utils/CycleTimer.h"


void SSETest()
{
	conPrint("SSETest()");
	{

	// Test myAlignedMalloc, myAlignedFree
	for(int i=0; i<1000; ++i)
	{
		for(int a=0; a<10; ++a)
		{
			void* m = SSE::alignedMalloc(i, 4 << a);

			// Write to the memory
			for(int z=0; z<i; ++z)
			{
				((unsigned char*)m)[z] = 0;
			}

			SSE::alignedFree(m);
		}
	}


	// Test accuracy of divisions
	const SSE_ALIGN float a[4] = {1.0e0f, 1.0e-1f, 1.0e-2f, 1.0e-3f};
	const SSE_ALIGN float b[4] = {1.1f,1.01f,1.001f,1.0001f};
	const SSE_ALIGN float c[4] = {0.99999999999f,0.99999999f,0.9999999f,0.999999f};
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

		//conPrint(::floatToString(r1[z], 20));
		//conPrint(::floatToString(r2[z], 20));
	}


	const int N = 200000 * 4;
	float* data1 = (float*)SSE::alignedMalloc(sizeof(float) * N, 16);
	float* data2 = (float*)SSE::alignedMalloc(sizeof(float) * N, 16);
	float* res = (float*)SSE::alignedMalloc(sizeof(float) * N, 16);

	for(int i=0; i<N; ++i)
	{
		data1[i] = 1.0;
		data2[i] = (float)(i + 1);
	}

	//conPrint("running divisions...");

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

	//const double elapsed_time = timer.getSecondsElapsed();
	//const double clock_freq = 2.67e9;
	//const double elapsed_cycles = elapsed_time * clock_freq;
	//const double cycles_per_iteration = elapsed_cycles / (double)N;

	double sum = 0;
	for(int i=0; i<N; ++i)
		sum += (double)res[i];

	//printVar(sum);
	//printVar(elapsed_time);
	//printVar(elapsed_cycles);
	//printVar(cycles_per_iteration);

	SSE::alignedFree(data1);
	SSE::alignedFree(data2);
	SSE::alignedFree(res);

	}


	{
		CycleTimer cycle_timer;

		const float N = 100000;
		int actual_num_cycles = 0;

		SSE_ALIGN float c[4];

		float f;
		for( f=0; f<N; f += 1.f)
		{
			__m128 i_v = _mm_load1_ps(&f);

			_mm_store_ps(c, i_v);

			actual_num_cycles++;
		}

		const uint64 cycles = cycle_timer.elapsed();
		printVar((float)cycles);
		printVar((float)cycles / N);
		printVar(actual_num_cycles);
		printVar(f);
		printVar(c[0]);
	}

	/*conPrint("\n====================Scalar max ===============");
	{
		CycleTimer cycle_timer;

		const float N = 100000;
		float sum = 0.f;

		SSE_ALIGN float fv[4] = { 1.f, 0, 20000.f, 0 };

		float f;
		for( f=0; f<N; f += 1.f)
		{
			fv[3] = f;

			sum += myMax(fv[0], myMax(fv[1], myMax(fv[2], fv[3])));
		}

		const uint64 cycles = cycle_timer.elapsed();
		printVar((float)cycles);
		printVar((float)cycles / N);
		printVar(sum);
	}*/

	conPrint("\n====================Test SSE horizontal max ===============");
	{
		CycleTimer cycle_timer;

		const float N = 100000;
		float sum = 0.f;

		//SSE_ALIGN float fv[4] = { 1.f, 0, 20000.f, 0 };

		float f;
		for( f=0; f<N; f += 1.f)
		{
			//fv[3] = f;
			//SSE_ALIGN float fv[4] = { f + 1.f, f, f + 2.f, f };

			//sum += sseGetMax(_mm_load_ps(fv));
			sum += horizontalMax(_mm_load_ps1(&f));
		}

		const uint64 cycles = cycle_timer.elapsed();
		printVar((float)cycles);
		printVar((float)cycles / N);
		printVar(sum);
	}

	{
		SSE_ALIGN float x[4] = { 1.f, 2.f, 3.f, 4.f };
		testAssert(horizontalMax(_mm_load_ps(x)) == 4.f);
	}
	{
		SSE_ALIGN float x[4] = { 5.f, 2.f, 3.f, 4.f };
		testAssert(horizontalMax(_mm_load_ps(x)) == 5.f);
	}
	{
		SSE_ALIGN float x[4] = { 5.f, 6.f, 3.f, 4.f };
		testAssert(horizontalMax(_mm_load_ps(x)) == 6.f);
	}
	{
		SSE_ALIGN float x[4] = { 5.f, 6.f, 7.f, 4.f };
		testAssert(horizontalMax(_mm_load_ps(x)) == 7.f);
	}

	conPrint("\n====================Test SSE horizontal min ===============");
	{
		SSE_ALIGN float x[4] = { 1.f, 2.f, 3.f, 4.f };
		testAssert(horizontalMin(_mm_load_ps(x)) == 1.f);
	}
	{
		SSE_ALIGN float x[4] = { 5.f, 2.f, 3.f, 4.f };
		testAssert(horizontalMin(_mm_load_ps(x)) == 2.f);
	}
	{
		SSE_ALIGN float x[4] = { 5.f, 6.f, 3.f, 4.f };
		testAssert(horizontalMin(_mm_load_ps(x)) == 3.f);
	}
	{
		SSE_ALIGN float x[4] = { 5.f, 6.f, 7.f, 4.f };
		testAssert(horizontalMin(_mm_load_ps(x)) == 4.f);
	}
}


#endif // BUILD_TESTS
