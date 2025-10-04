/*=====================================================================
SharedStringTable.h
-------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "SharedImmutableString.h"
#include "ThreadSafeRefCounted.h"
#include "Reference.h"
#include "Mutex.h"
#include "string_view.h"
#include <assert.h>
#include <memory>
#include <new>


namespace glare
{


/*=====================================================================
SharedStringTable
-----------------

=====================================================================*/
class SharedStringTable : public ThreadSafeRefCounted
{
public:
	SharedStringTable(size_t num_buckets); // num_buckets must be a power of 2.
	~SharedStringTable();

	// Threadsafe
	SharedImmutableString getOrMakeString(const char* data, size_t len);
	SharedImmutableString getOrMakeString(string_view str)
	{
		return getOrMakeString(str.data(), str.size());
	}

	SharedImmutableString getOrMakeEmptyString() { return makeEmptySharedImmutableString(); }

	size_t size() const { return num_items; }

	static void test();
private:
	Mutex mutex;

	//Reference<SharedImmutableString> empty_string;

	struct Bucket
	{
		uint64 hash;
		Reference<SharedImmutableStringData> string;
	};

	Bucket* buckets;
	size_t buckets_size;
	size_t num_items;
};


typedef Reference<SharedStringTable> SharedStringTableRef;


} // end namespace glare
