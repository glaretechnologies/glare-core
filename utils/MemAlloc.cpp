/*=====================================================================
MemAlloc.cpp
------------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#include "MemAlloc.h"


#include "../maths/mathstypes.h"
#include "ConPrint.h"
#include "StringUtils.h"
#include "AtomicInt.h"
#include <tracy/Tracy.hpp>
#include <new>


static glare::AtomicInt total_allocated_B(0); // total size of active allocations
static glare::AtomicInt num_allocations(0);
static glare::AtomicInt num_active_allocations(0);  // num active allocations that have not been freed
static glare::AtomicInt high_water_mark_B(0);


size_t MemAlloc::getTotalAllocatedB()
{
	return (size_t)total_allocated_B;
}

size_t MemAlloc::getNumAllocations()
{
	return (size_t)num_allocations;
}

size_t MemAlloc::getNumActiveAllocations()
{
	return (size_t)num_active_allocations;
}

size_t MemAlloc::getHighWaterMarkB()
{
	return (size_t)high_water_mark_B;
}


#if TRACE_ALLOCATIONS

void* MemAlloc::traceMalloc(size_t n)
{
	// Allocate the requested allocation size, plus room for a uint64, in which we will store the size of the allocation.
	void* data = malloc(n + sizeof(uint64));
	((uint64*)data)[0] = n; // Store requested allocation size


	total_allocated_B += n;
	high_water_mark_B = myMax(high_water_mark_B.getVal(), total_allocated_B.getVal());
	num_allocations++;
	num_active_allocations++;

	return (uint64*)data + 1; // return a pointer just past the stored requested allocation size
}


void MemAlloc::traceFree(void* ptr)
{
	uint64* original_ptr = ((uint64*)ptr) - 1; // Recover original ptr
	const uint64 size = *original_ptr;

	assert(total_allocated_B >= (int64)size);
	total_allocated_B -= size;
	num_active_allocations--;

	free(original_ptr);
}


// According to https://en.cppreference.com/w/cpp/memory/new/operator_new,
// we can handle all allocations by just replacing these two 'new' functions:
// "Thus, replacing the throwing single object allocation functions is sufficient to handle all allocations."

// If both operator new with and without alignment are freed with the same delete,
// we need to use alignedMalloc for both of them.
void* operator new(std::size_t count)
{
	return MemAlloc::alignedMalloc(count, 8);
	//return MemAlloc::traceMalloc(count); // This crashes for some reason.
}

#if __cplusplus >= 201703L // If c++ version is >= c++17:
void* operator new(std::size_t count, std::align_val_t al)
{
	return MemAlloc::alignedMalloc(count, (size_t)al);
}
#endif

void operator delete(void* p)
{
	MemAlloc::alignedFree(p);
	//MemAlloc::traceFree(p);
}

#endif // end if TRACE_ALLOCATIONS


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

#if TRACE_ALLOCATIONS
	void* original_addr = traceMalloc(amount + alignment);
#else
	void* original_addr = malloc(amount + alignment);
#endif
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

	TracyAllocS(original_addr, amount + alignment, /*call stack capture depth=*/10);

	return returned_addr;
}


void alignedFree(void* addr)
{
	if(addr == NULL)
		return;

	const unsigned int original_addr_offset = *(((unsigned int*)addr) - 1);

	void* original_addr = (void*)((uintptr_t)addr - (uintptr_t)original_addr_offset);

	TracyFreeS(original_addr, /*call stack capture depth=*/10);

#if TRACE_ALLOCATIONS
	traceFree(original_addr);
#else
	free(original_addr);
#endif
}


} // end namespace MemAlloc


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "StringUtils.h"
#include "Timer.h"
#include "MyThread.h"
#include "PlatformUtils.h"
#include "../maths/PCG32.h"


class MemAllocTestThread : public MyThread
{
public:
	virtual void run()
	{
		PCG32 rng(1);

		float q = 0;
		for(int i=0; i<10000; ++i)
		{
			//void* data = malloc(rng.nextUInt(4096));
			void* data = MemAlloc::alignedMalloc(rng.nextUInt(4096), 16);

			//PlatformUtils::Sleep(1);
		//	float x = 0;
		//	for(int z=0; z<100000; ++z)
		//		x += pow((float)z, 1.23434f);
		//	q += x;

			//free(data);
			MemAlloc::alignedFree(data);

			if((i % 100) == 0)
				conPrint(toString(i));
		}

		conPrint("Sleeping...");
		PlatformUtils::Sleep(10000);

		conPrint(toString(q));
	}

	double elapsed_per_op;
};


void MemAlloc::test()
{
	conPrint("MemAlloc::test()");


	if(false)
	{
		std::vector<Reference<MemAllocTestThread> > threads;
		for(size_t i=0; i<100; ++i)
		{
			Reference<MemAllocTestThread> t1 = new MemAllocTestThread(); 
			threads.push_back(t1);
			t1->launch();
		}

		for(size_t i=0; i<threads.size(); ++i)
			threads[i]->join();
	}



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

	conPrint("MemAlloc::test() done.");
}


#endif // BUILD_TESTS
