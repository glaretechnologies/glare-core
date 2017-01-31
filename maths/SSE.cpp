/*=====================================================================
SSE.cpp
-------
File created by ClassTemplate on Sat Jun 25 08:01:25 2005
Copyright Glare Technologies Limited 2012 -
=====================================================================*/
#include "SSE.h"


#include "../utils/Platform.h"
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

	const size_t padding = myMax(alignment, sizeof(unsigned int));

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


#include "../indigo/TestUtils.h"
#include "../utils/ConPrint.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "../utils/CycleTimer.h"


void SSETest()
{
	conPrint("SSETest()");
	

	// Test myAlignedMalloc, myAlignedFree
	for(int i=0; i<1000; ++i) // allocation length
	{
		for(size_t a=0; a<10; ++a)
		{
			const size_t alignment = 1uLL << a;
			
			void* m = SSE::alignedMalloc(i, alignment);

			testAssert((size_t)(unsigned char*)m % alignment == 0); // Check result aligned

			// Write to the memory
			for(int z=0; z<i; ++z)
			{
				((unsigned char*)m)[z] = 0;
			}

			SSE::alignedFree(m);
		}
	}

	//TEMP: Perf test
#if 0
	std::vector<Plotter::DataSet> datasets;
	datasets.push_back(Plotter::DataSet("SSE::alignedMalloc time"));
	datasets.push_back(Plotter::DataSet("_mm_malloc time"));
	datasets.push_back(Plotter::DataSet("malloc time"));

	const int N = 10000;
	const int NUM_TRIALS = 15;

	const size_t alignment = 8;

	for(int alloc_size = 4; alloc_size <= (1 << 18); alloc_size += 1024)
	//int alloc_size = 1 << 16;
	{
		printVar(alloc_size);
		{
			double best_time = 10000000;
			for(int z=0; z<NUM_TRIALS; ++z)
			{
				Timer timer;

				for(int i=1; i<N; ++i)
				{
					//for(int a=0; a<8; ++a)
					//{
					//const size_t size = 1 << 16;
					//const size_t alignment = 1uLL << a;
					void* m = _mm_malloc(alloc_size, alignment);
					//void* m2 = _aligned_malloc(alloc_size, alignment);

					// Write to the memory
					/*for(int z=0; z<i; ++z)
					{
					((unsigned char*)m)[z] = 0;
					}*/

					_mm_free(m);
					//}
				}
				best_time = myMin(best_time, timer.elapsed());
			}
			double each_time_ns = 1.0e9 * best_time/N;
			conPrint("_mm_malloc:         " + toString(each_time_ns) + " ns");
			datasets[1].points.push_back(Vec2f(alloc_size, each_time_ns));
		}

		{
			double best_time = 10000000;
			for(int z=0; z<NUM_TRIALS; ++z)
			{
				Timer timer;

				for(int i=1; i<N; ++i)
				{
					//for(int a=0; a<8; ++a)
					//{
						//const size_t size = 1 << 16;
						//const size_t alignment = 1uLL << a;
					void* m = SSE::alignedMalloc(alloc_size, alignment);

					// Write to the memory
					/*for(int z=0; z<i; ++z)
					{
						((unsigned char*)m)[z] = 0;
					}*/

					SSE::alignedFree(m);
					//}
				}
				best_time = myMin(best_time, timer.elapsed());
			}
			double each_time_ns = 1.0e9 * best_time/N;
			conPrint("SSE::alignedMalloc: " + toString(each_time_ns) + " ns");
			datasets[0].points.push_back(Vec2f(alloc_size, each_time_ns));
		}

		// Test plain-old malloc
		{
			double best_time = 10000000;
			for(int z=0; z<NUM_TRIALS; ++z)
			{
				Timer timer;
				for(int i=1; i<N; ++i)
				{
					void* m = malloc(alloc_size);
					free(m);
				}
				best_time = myMin(best_time, timer.elapsed());
			}
			double each_time_ns = 1.0e9 * best_time/N;
			conPrint("malloc:             " + toString(each_time_ns) + " ns");
			datasets[2].points.push_back(Vec2f(alloc_size, each_time_ns));
		}

	}

	Plotter::PlotOptions options;
	options.x_axis_log = true;
	Plotter::plot("aligned_malloc_perf.png", "Aligned mem allocation perf, alignment = 8", "allocation size (B)", "elapsed (ns)", datasets, options);
#endif

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


#if !defined(OSX)

	/*conPrint("\n==================== myAlignedMalloc vs _mm_malloc speed test ===============");
	{
		Timer timer;
		// Test myAlignedMalloc, myAlignedFree
		for(int i=0; i<1000; ++i)
		{
			for(int a=0; a<10; ++a)
			{
				void* m = SSE::alignedMalloc(i, 4 << a);

				// Write to the memory
				//for(int z=0; z<i; ++z)
				//{
				//	((unsigned char*)m)[z] = 0;
				//}

				SSE::alignedFree(m);
			}
		}

		conPrint("alignedMalloc(): " + timer.elapsedString());
	}
	{
		Timer timer;
		for(int i=0; i<1000; ++i)
		{
			for(int a=0; a<10; ++a)
			{
				void* m = _mm_malloc(i, 4 << a);

				// Write to the memory
				//for(int z=0; z<i; ++z)
				//{
				//	((unsigned char*)m)[z] = 0;
				//}

				_mm_free(m);
			}
		}

		conPrint("_mm_malloc: " + timer.elapsedString());
	}
	{
		Timer timer;
		// Test myAlignedMalloc, myAlignedFree
		for(int i=0; i<1000; ++i)
		{
			for(int a=0; a<10; ++a)
			{
				void* m = SSE::alignedMalloc(i, 4 << a);

				// Write to the memory
				//for(int z=0; z<i; ++z)
				//{
				//	((unsigned char*)m)[z] = 0;
				//}

				SSE::alignedFree(m);
			}
		}

		conPrint("alignedMalloc(): " + timer.elapsedString());
	}*/

	conPrint("\n====================Test SSE horizontal max ===============");
	//{
	//	CycleTimer cycle_timer;

	//	const float N = 100000;
	//	float sum = 0.f;

	//	//SSE_ALIGN float fv[4] = { 1.f, 0, 20000.f, 0 };

	//	float f;
	//	for( f=0; f<N; f += 1.f)
	//	{
	//		//fv[3] = f;
	//		//SSE_ALIGN float fv[4] = { f + 1.f, f, f + 2.f, f };

	//		//sum += sseGetMax(_mm_load_ps(fv));
	//		sum += horizontalMax(_mm_load_ps1(&f));
	//	}

	//	const uint64 cycles = cycle_timer.elapsed();
	//	printVar((float)cycles);
	//	printVar((float)cycles / N);
	//	printVar(sum);
	//}
#endif // !defined(OSX)

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
