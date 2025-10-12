/*=====================================================================
RandomAccessInStream.cpp
------------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "RandomAccessInStream.h"

#include "StringUtils.h"
#include "SharedImmutableString.h"
#include "SharedStringTable.h"




glare::SharedImmutableString RandomAccessInStream::readSharedImmutableStringLengthFirstWithTable(size_t max_string_length, glare::SharedStringTable& shared_string_table)
{
	// Read string byte size
	const uint32 size = readUInt32();
	if((size_t)size > max_string_length)
		throw glare::Exception("String length too long (length=" + toString(size) + ")");

	if(size == 0)
		return glare::SharedImmutableString();

	if(!canReadNBytes(size))
		throw glare::Exception("String length too long for remaining stream (length=" + toString(size) + ")");

	return shared_string_table.getOrMakeString((const char*)currentReadPtr(), size);
}


glare::SharedImmutableString RandomAccessInStream::readSharedImmutableStringLengthFirst(size_t max_string_length)
{
	// Read string byte size
	const uint32 size = readUInt32();

	if(size == 0)
		return glare::SharedImmutableString();

	if((size_t)size > max_string_length)
		throw glare::Exception("String length too long (length=" + toString(size) + ")");

	if(!canReadNBytes(size))
		throw glare::Exception("String length too long for remaining stream (length=" + toString(size) + ")");

	return glare::makeSharedImmutableString((const char*)currentReadPtr(), size);
}



// If shared_string_table is non-null, gets or inserts in table too.
glare::SharedImmutableString RandomAccessInStream::readSharedImmutableStringLengthFirst(size_t max_string_length, glare::SharedStringTable* shared_string_table)
{
	// Read string byte size
	const uint32 size = readUInt32();
	if((size_t)size > max_string_length)
		throw glare::Exception("String length too long (length=" + toString(size) + ")");

	if(size == 0)
	{
		//if(shared_string_table)
		//	return shared_string_table->getOrMakeEmptyString();
		//else
			return glare::makeEmptySharedImmutableString();
	}

	if(!canReadNBytes(size))
		throw glare::Exception("String length too long for remaining stream (length=" + toString(size) + ")");


	glare::SharedImmutableString str = shared_string_table ?
		shared_string_table->getOrMakeString((const char*)currentReadPtr(), size) :
		glare::makeSharedImmutableString((const char*)currentReadPtr(), size);

	advanceReadIndex(size);
	return str;
}