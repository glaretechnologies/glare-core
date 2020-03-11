/*=====================================================================
MemAlloc.cpp
------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "MemAlloc.h"


#include "../maths/mathstypes.h"
#include <new>


namespace MemAlloc
{
	
void* alignedMalloc(size_t amount, size_t alignment)
{
	assert(Maths::isPowerOfTwo(alignment));

	// The general idea here is to call original_addr = malloc(), then round the result down to largest multiple of alignment <= original_addr.
	// We then add alignment bytes, to get an address > original_addr, that is also aligned.
	// Assuming malloc returns a 4-byte aligned address, then the rounding down then adding alignemnt process should give
	// A resulting address that is aligned, but also >= 4 bytes from original address.  
	// We then store the offset from this aligned address to the original address in the 4 bytes preceding the aligned address, then return the aligned address.
	// When freeing the mem, we can look in the 4 bytes preceding the pointed to address, to recover the offset and hence the original address.
	//
	// The maximum amount the returned address can be above original_address is alignment bytes, therefore malloc'ing alignment extra bytes should suffice.

	// Suppose alignment is 16 = 0x10, so alignment - 1 = 15 = 0xF, also suppose original_addr = 0x01234567 
	// original_addr & (alignment - 1) = 0x01234567 & 0xF = 0x00000007
	// rounded_down_addr = original_addr - (original_addr & (alignment - 1)) = 0x01234567 - 0x00000007 = 0x01234560
	// Then 
	// returned_addr = rounded_down_addr + alignment = 0x01234560 + 0x10 = 0x01234570

	alignment = myMax(sizeof(unsigned int), alignment);

	void* original_addr = malloc(amount + alignment);
	if(!original_addr)
		throw std::bad_alloc();
	assert((uintptr_t)original_addr % 4 == 0);

	// Snap the address down to the largest multiple of alignment <= original_addr
	const uintptr_t rounded_down_addr = (uintptr_t)original_addr - ((uintptr_t)original_addr & (alignment - 1));

	assert(rounded_down_addr % alignment == 0); // Check aligned
	assert((rounded_down_addr <= (uintptr_t)original_addr) && ((uintptr_t)original_addr - rounded_down_addr < alignment));

	void* returned_addr = (void*)(rounded_down_addr + alignment);

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
#include "ConPrint.h"
#include "StringUtils.h"
#include "Timer.h"
#include "CycleTimer.h"


void MemAlloc::test()
{
	// Test myAlignedMalloc, myAlignedFree
	for(int i=0; i<1000; ++i) // allocation length
	{
		for(size_t a=0; a<10; ++a)
		{
			const size_t alignment = 1uLL << a;

			void* m = MemAlloc::alignedMalloc(i, alignment);

			testAssert((size_t)(unsigned char*)m % alignment == 0); // Check result aligned

			// Write to the memory
			for(int z=0; z<i; ++z)
			{
				((unsigned char*)m)[z] = 0;
			}

			MemAlloc::alignedFree(m);
		}
	}

	//TEMP: Perf test
#if 0
	std::vector<Plotter::DataSet> datasets;
	datasets.push_back(Plotter::DataSet("MemAlloc::alignedMalloc time"));
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
					void* m = MemAlloc::alignedMalloc(alloc_size, alignment);

					// Write to the memory
					/*for(int z=0; z<i; ++z)
					{
						((unsigned char*)m)[z] = 0;
					}*/

					MemAlloc::alignedFree(m);
					//}
				}
				best_time = myMin(best_time, timer.elapsed());
			}
			double each_time_ns = 1.0e9 * best_time/N;
			conPrint("MemAlloc::alignedMalloc: " + toString(each_time_ns) + " ns");
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
}


#endif // BUILD_TESTS
