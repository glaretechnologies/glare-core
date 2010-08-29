/*=====================================================================
SSE.cpp
-------
File created by ClassTemplate on Sat Jun 25 08:01:25 2005
Code By Nicholas Chapman.
=====================================================================*/
#include "SSE.h"


#include "../utils/platform.h"
#include "../utils/timer.h" // just for testing
#include "../utils/CycleTimer.h" // just for testing
#include "../indigo/globals.h" // just for testing
#include "../utils/stringutils.h" // just for testing
#include "../indigo/TestUtils.h" // just for testing
#include "../maths/mathstypes.h"
#include <assert.h>
#ifdef COMPILER_MSVC
#include <intrin.h>
#else
#include <stdlib.h>
#include <errno.h>
#endif


namespace SSE
{


static inline size_t allBitsOne()
{
	if(sizeof(size_t) == 4)
		//return 0xFFFFFFFF;
    return -1;
	else if(sizeof(size_t) == 8)
		return -1; //return 0xFFFFFFFFFFFFFFFF;
	else
	{
		assert(0);
	}
	
}

void* myAlignedMalloc(size_t amount, size_t alignment)
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


void myAlignedFree(void* addr)
{
	if(addr == NULL)
		return;

	const unsigned int original_addr_offset = *(((unsigned int*)addr) - 1);

	void* original_addr = (void*)((uintptr_t)addr - (uintptr_t)original_addr_offset);

	free(original_addr);
}


void* alignedMalloc(size_t size, size_t alignment)
{
	/*
	//assert(Maths::isPowerOfTwo(alignment % sizeof(void*))); // This is required for posix_memalign
#ifdef COMPILER_MSVC
	void* result = _aligned_malloc(size, alignment);
	if(result == NULL)
		throw std::bad_alloc("Memory allocation failed.");
	return result;
#elif defined(OSX)
  return malloc(size);
#else
	void* mem_ptr;
	const int result = posix_memalign(&mem_ptr, alignment, size);
	if(result == EINVAL)
	{
		conPrint("Bad alignment: " + toString((unsigned int)alignment));
		assert(0);
		throw std::bad_alloc();
	}
	else if(result == ENOMEM)
	{
		throw std::bad_alloc();
	}
	else
		return mem_ptr;
#endif
	*/
	return myAlignedMalloc(size, alignment);
}


void alignedFree(void* mem)
{
	/*
#ifdef COMPILER_MSVC
	_aligned_free(mem);
#else
	// Apparently free() can handle aligned mem.
	// see: http://www.opengroup.org/onlinepubs/000095399/functions/posix_memalign.html
	free(mem);
#endif
	*/
	myAlignedFree(mem);
}


} // end namespace SSE


inline float horizontalMax(const __m128& v)
{
	//const __m128 v = _mm_load_ps(f); // [d, c, b, a]
	const __m128 aabb = _mm_shuffle_ps(v, v, _MM_SHUFFLE(1, 1, 0, 0)); // [b, b, a, a]
	const __m128 ccdd = _mm_shuffle_ps(v, v, _MM_SHUFFLE(3, 3, 2, 2)); // [d, d, c, c]
	const __m128 m1 = _mm_max_ps(aabb, ccdd); // [m(b,d), m(b,d), m(a, c), m(a, c)]
	const __m128 m2 = _mm_shuffle_ps(m1, m1, _MM_SHUFFLE(3, 3, 3, 3)); // [m(b,d), m(b,d), m(b,d), m(b,d)]
	const __m128 m3 = _mm_max_ps(m1, m2);

	SSE_ALIGN float x[4];
	_mm_store_ps(x, m3);
	return x[0];
}


#if (BUILD_TESTS)
void SSETest()
{
//	conPrint("SSETest()");
	{

	// Test myAlignedMalloc, myAlignedFree
	for(int i=0; i<1000; ++i)
	{
		for(int a=0; a<10; ++a)
		{
			void* m = SSE::myAlignedMalloc(i, 4 << a);

			// Write to the memory
			for(int z=0; z<i; ++z)
			{
				((unsigned char*)m)[z] = 0;
			}

			SSE::myAlignedFree(m);
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

	const double elapsed_time = timer.getSecondsElapsed();
	const double clock_freq = 2.4e9;
	const double elapsed_cycles = elapsed_time * clock_freq;
	const double cycles_per_iteration = elapsed_cycles / (double)N;

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

	conPrint("\n====================SSE horizontal max ===============");
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
	//exit(0);
}
#endif
