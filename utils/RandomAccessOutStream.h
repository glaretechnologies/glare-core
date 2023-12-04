/*=====================================================================
RandomAccessOutStream.h
-----------------------
Copyright Glare Technologies Limited 2023 -
=====================================================================*/
#pragma once


#include "OutStream.h"


/*=====================================================================
RandomAccessOutStream
---------------------
Output stream that supports random access.
=====================================================================*/
class RandomAccessOutStream : public OutStream
{
public:
	RandomAccessOutStream() {}
	virtual ~RandomAccessOutStream() {}

	virtual size_t getWriteIndex() const = 0;

	virtual void* getWritePtrAtIndex(size_t i) = 0; // Index should be < the current write index.
};
