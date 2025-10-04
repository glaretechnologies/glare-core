/*=====================================================================
SharedStringTable.cpp
---------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "SharedStringTable.h"


#include "IncludeXXHash.h"
#include "StringUtils.h"
#include "Lock.h"
#include "RuntimeCheck.h"
#include "MemAlloc.h"
#include "../maths/mathstypes.h"


glare::SharedStringTable::SharedStringTable(size_t num_buckets)
{
	runtimeCheck(Maths::isPowerOfTwo(num_buckets));

	//empty_string = makeSharedImmutableString("");

	buckets_size = num_buckets;
	buckets = (Bucket*)MemAlloc::alignedMalloc(sizeof(Bucket) * buckets_size, 16);
	num_items = 0;

	// Construct buckets
	for(Bucket* elem=buckets; elem<buckets + buckets_size; ++elem)
		::new (elem) Bucket();
}


glare::SharedStringTable::~SharedStringTable()
{
	// Destroy bucket data
	for(size_t i=0; i<buckets_size; ++i)
		(buckets + i)->~Bucket();
	MemAlloc::alignedFree(buckets);
}


glare::SharedImmutableString glare::SharedStringTable::getOrMakeString(const char* data, size_t len)
{
	if(len == 0)
	{
		return getOrMakeEmptyString();
		//return SharedImmutableStringHandle(empty_string);
		//return Reference<glare::SharedImmutableString>(); // empty_string;
	}

	const uint64 hash = XXH64(data, len, /*seed=*/1);

	Lock lock(mutex);
	
	size_t bucket_i = hash & (buckets_size - 1); // Get hash modulo buckets_size

	// Search for bucket item is in
	while(1)
	{
		Bucket& bucket = buckets[bucket_i];

		if(!bucket.string)
		{
			// We came to an empty bucket.  Therefore there is no such string in hash set.  Make it and add it.
			Reference<glare::SharedImmutableStringData> new_string = makeSharedImmutableStringData(data, len);
			bucket.hash = hash;
			bucket.string = new_string;

			// Check for table resize:
			num_items++;

			if(num_items * 4 >= buckets_size * 3)
			{
				// Resize table
				size_t new_buckets_size = buckets_size * 2;
				Bucket* new_buckets = (Bucket*)MemAlloc::alignedMalloc(sizeof(Bucket) * new_buckets_size, 16);

				// Construct buckets
				for(Bucket* elem=new_buckets; elem<new_buckets + new_buckets_size; ++elem)
					::new (elem) Bucket();

				// Insert strings into new table
				for(Bucket* old_bucket=buckets; old_bucket<buckets + buckets_size; ++old_bucket)
					if(old_bucket->string)
					{
						size_t new_i = old_bucket->hash & (new_buckets_size - 1);

						// Advance until we find an empty bucket
						while(new_buckets[new_i].string)
							new_i = (new_i + 1) & (new_buckets_size - 1);

						assert(!new_buckets[new_i].string);
						new_buckets[new_i].hash   = old_bucket->hash;
						new_buckets[new_i].string = old_bucket->string;
					}
				
				// Destroy old bucket data
				for(size_t i=0; i<buckets_size; ++i)
					(buckets + i)->~Bucket();
				MemAlloc::alignedFree(buckets);

				buckets = new_buckets;
				buckets_size = new_buckets_size;
			}

			//return new_string;
			return SharedImmutableString(new_string);
		}

		assert(bucket.string);
		if((bucket.hash == hash) &&
			(len == buckets[bucket_i].string->size()) &&
			(std::memcmp(data, buckets[bucket_i].string->data(), len) == 0))
		{
			// We found the string already in the hash set, return a reference to it.
			return SharedImmutableString(buckets[bucket_i].string);
		}

		// Else advance to next bucket, with wrap-around
		bucket_i = (bucket_i + 1) & (buckets_size - 1); // (bucket_i + 1) % buckets.size();
	}
}


#if BUILD_TESTS


#include "ConPrint.h"
#include "TestUtils.h"


void glare::SharedStringTable::test()
{
	{
		Reference<SharedStringTable> table = new SharedStringTable(/*num buckets=*/8);

		SharedImmutableString s1 = table->getOrMakeString("hello");
		SharedImmutableString s2 = table->getOrMakeString("hello");

		testAssert(s1.string.ptr() == s2.string.ptr());
		testAssert(table->size() == 1);
	}

	// Test hash table resizing
	{
		Reference<SharedStringTable> table = new SharedStringTable(/*num buckets=*/4);

		SharedImmutableString s1 = table->getOrMakeString("hello");
		SharedImmutableString s2 = table->getOrMakeString("thar");
		SharedImmutableString s3 = table->getOrMakeString("my");
		SharedImmutableString s4 = table->getOrMakeString("friend");

		testAssert(table->getOrMakeString("hello") == s1);
		testAssert(table->getOrMakeString("thar") == s2);
		testAssert(table->getOrMakeString("my") == s3);
		testAssert(table->getOrMakeString("friend") == s4);
	}

	conPrint("glare::SharedStringTable::test() done.");
}


#endif // BUILD_TESTS
