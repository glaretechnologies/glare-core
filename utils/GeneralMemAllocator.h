/*=====================================================================
GeneralMemllocator.h
--------------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "GlareAllocator.h"
#include "BestFitAllocator.h"
#include "Mutex.h"
#include <vector>


namespace glare
{


class GeneralMemAllocator : public glare::Allocator
{
public:
	GeneralMemAllocator(size_t arena_size_B);
	~GeneralMemAllocator();

	virtual void* alloc(size_t size, size_t alignment) override;
	virtual void free(void* ptr) override;

	std::string getDiagnostics() const;

	static void test();
private:
	void allocNewArena()																REQUIRES(mutex);
	void* tryAllocFromArena(size_t size, size_t alignment, size_t start_arena_index)	REQUIRES(mutex);

	class Arena
	{
	public:
		Arena(size_t size) : best_fit_allocator(size) {}

		BestFitAllocator best_fit_allocator;
		void* data;
	};

	std::vector<Arena*> arenas		GUARDED_BY(mutex);
	mutable Mutex mutex;
	size_t arena_size_B;
};


} // End namespace glare
