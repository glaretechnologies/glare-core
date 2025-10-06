/*=====================================================================
STLArenaAllocator.cpp
---------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "STLArenaAllocator.h"


#if BUILD_TESTS


#include "TestUtils.h"
#include <map>
#include <set>


typedef std::basic_string<char, std::char_traits<char>, glare::STLArenaAllocator<char>> AllocatorString;
typedef std::set<char, std::less<char>, glare::STLArenaAllocator<char>> AllocatorSet;


void glare::testSTLArenaAllocator()
{
	{
		glare::ArenaAllocator arena_allocator(1024 * 1024);

		glare::STLArenaAllocator<char> allocator(&arena_allocator);

		// Test with a std::string using the allocator
		{
			testAssert(arena_allocator.currentOffset() == 0);

			// Needs to be a long string so it's stored using the allocator and not internally using the small-string optimisation.
			AllocatorString s("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", allocator);

			testAssert(s.get_allocator().arena_allocator == &arena_allocator);

			testAssert(arena_allocator.currentOffset() > 0);
		}
		arena_allocator.clear();

		// Test copy construction of one string from another using the allocator.
		{
			testAssert(arena_allocator.currentOffset() == 0);
			AllocatorString s("AAAAAAAAAAAAAAAAAAAAAAAAAAAAAA", allocator);
			testAssert(s.get_allocator().arena_allocator == &arena_allocator);

			AllocatorString s2 = s;
			testAssert(s2.get_allocator().arena_allocator == &arena_allocator); // Allocator should have been copied.

			AllocatorString s3(s, glare::STLArenaAllocator<char>());

			testAssert(s3 == s);
			testAssert(s3.get_allocator().arena_allocator == nullptr);

			// Test construction of a string used as the key in a mp.
			std::map<AllocatorString, int> m;
			m[s] = 1;
			testAssert(m.find(s)->first.get_allocator().arena_allocator == &arena_allocator);

			testAssert(arena_allocator.currentOffset() > 0);
		}
		arena_allocator.clear();

		// Test a container (set in particular) using the allocator.
		{
			AllocatorSet s(std::less<char>(), allocator);
			s.insert('a');
			testAssert(s.get_allocator().arena_allocator == &arena_allocator);
			testAssert(arena_allocator.currentOffset() > 0);

			// copy-construction of one set from another using the allocator.
			AllocatorSet s2 = s;
			testAssert(s2.get_allocator().arena_allocator == &arena_allocator);

			// assignment of one set from another using the allocator.
			AllocatorSet s3;
			testAssert(s3.get_allocator().arena_allocator == nullptr);
			s3 = s;
			testAssert(s3.get_allocator().arena_allocator == &arena_allocator);
		}
		arena_allocator.clear();
	}
}


#endif // BUILD_TESTS
