/*=====================================================================
RandomAccessInStream.h
----------------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "InStream.h"

namespace glare { class SharedImmutableString; }
namespace glare { class SharedStringTable; }


/*=====================================================================
RandomAccessInStream
--------------------
Input stream that supports random access.
=====================================================================*/
class RandomAccessInStream : public InStream
{
public:
	RandomAccessInStream() {}
	virtual ~RandomAccessInStream() {}

	virtual bool canReadNBytes(size_t N) const = 0;
	virtual void setReadIndex(size_t i) = 0;
	virtual void advanceReadIndex(size_t n) = 0;
	virtual size_t getReadIndex() const = 0;

	virtual size_t size() const = 0;

	virtual const void* currentReadPtr() const = 0;


	[[nodiscard]] glare::SharedImmutableString readSharedImmutableStringLengthFirstWithTable(size_t max_string_length, glare::SharedStringTable& shared_string_table);
	[[nodiscard]] glare::SharedImmutableString readSharedImmutableStringLengthFirst(size_t max_string_length);

	// If shared_string_table is non-null, gets or inserts in table too.
	[[nodiscard]] glare::SharedImmutableString readSharedImmutableStringLengthFirst(size_t max_string_length, glare::SharedStringTable* shared_string_table);
};
