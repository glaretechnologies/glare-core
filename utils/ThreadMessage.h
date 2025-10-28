/*=====================================================================
ThreadMessage.h
---------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "ThreadSafeRefCounted.h"
#include "FastPoolAllocator.h"
#include "Reference.h"
#include <string>


/*=====================================================================
ThreadMessage
-------------
Base class for messages passed to and from threads.
ThreadMessages are expected to be immutable.
Therefore they don't have to be cloned to send to multiple threads.
=====================================================================*/
class ThreadMessage : public ThreadSafeRefCounted
{
public:
	ThreadMessage() : allocator(nullptr), id(-1) {}
	ThreadMessage(int id_) : allocator(nullptr), id(id_) {}

	virtual ~ThreadMessage() {}

	virtual const std::string debugName() const { return "ThreadMessage"; }

	static void test();

	glare::FastPoolAllocator* allocator; // non-null if this object was allocated from a pool allocator.
	int allocation_index; // Uses for freeing from the pool allocator.

	int id;
};


typedef Reference<ThreadMessage> ThreadMessageRef;


// Template specialisation of destroyAndFreeOb for ThreadMessage.  This is called when being freed by a Reference.
template <>
void destroyAndFreeOb<ThreadMessage>(ThreadMessage* ob);
