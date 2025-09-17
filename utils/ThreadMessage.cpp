/*=====================================================================
ThreadMessage.cpp
-----------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#include "ThreadMessage.h"


template <>
void destroyAndFreeOb<ThreadMessage>(ThreadMessage* ob)
{
	glare::FastPoolAllocator* allocator = ob->allocator;
	if(allocator)
	{
		const int allocation_index = ob->allocation_index;
		ob->~ThreadMessage(); // Destroy object
		allocator->free(allocation_index);
	}
	else
		delete ob;
}


#if BUILD_TESTS


#include "TestUtils.h"


class TestThreadMessage : public ThreadMessage
{
public:
	TestThreadMessage(int* counter_) : counter(counter_) { (*counter)++; }
	~TestThreadMessage() { (*counter)--; }

	int* counter;
};

// Template specialisation of destroyAndFreeOb for ThreadMessage.  This is called when being freed by a Reference.
template <>
void destroyAndFreeOb<TestThreadMessage>(TestThreadMessage* ob)
{
	destroyAndFreeOb<ThreadMessage>(ob);
}

void ThreadMessage::test()
{
	glare::FastPoolAllocator allocator(sizeof(TestThreadMessage), 16, 16);
	testAssert(allocator.numAllocatedObs() == 0);
	int counter = 0;

	// Test destroying via reference to the derived TestThreadMessage class
	{
		glare::FastPoolAllocator::AllocResult alloc_res = allocator.alloc();

		Reference<TestThreadMessage> msg = new (alloc_res.ptr) TestThreadMessage(&counter);
		msg->allocator = &allocator;
		msg->allocation_index = alloc_res.index;

		testAssert(counter == 1);
		msg = nullptr;
		testAssert(counter == 0); // Check TestThreadMessage destructor was run properly.
	}

	testAssert(allocator.numAllocatedObs() == 0);


	// Test destroying via reference to the base ThreadMessage class
	{
		glare::FastPoolAllocator::AllocResult alloc_res = allocator.alloc();

		Reference<TestThreadMessage> msg = new (alloc_res.ptr) TestThreadMessage(&counter);
		msg->allocator = &allocator;
		msg->allocation_index = alloc_res.index;

		testAssert(counter == 1);

		Reference<ThreadMessage> base_ref = msg;
		msg = nullptr;

		testAssert(counter == 1);
		base_ref = nullptr;
		testAssert(counter == 0); // Check TestThreadMessage destructor was run properly.
	}

	testAssert(allocator.numAllocatedObs() == 0);
}


#endif

