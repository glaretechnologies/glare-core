/*=====================================================================
ThreadSafeQueue.cpp
-------------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#include "ThreadSafeQueue.h"


#if BUILD_TESTS


#include "MyThread.h"
#include "ConPrint.h"
#include "PlatformUtils.h"
#include "Timer.h"
#include "../utils/TestUtils.h"


static const int TERMINATING_INTEGER = -1;


class ThreadSafeQueueTestReaderThread : public MyThread
{
public:
	virtual void run()
	{
		while(1)
		{
			int x;
			if(read_timeout_s == 0)
				queue->dequeue(x);
			else
			{
				while(1)
				{
					const bool read_item = queue->dequeueWithTimeout(read_timeout_s, x);
					if(read_item)
						break;
				}

				//conPrint("Dequeued (with timeout) " + toString(x));
			}
			

			if(x == TERMINATING_INTEGER)
				break;

			if(seen)
			{
				Lock lock(*seen_mutex);
				testAssert(!(*seen)[x]);
				(*seen)[x] = true;
			}
		}
	}

	double read_timeout_s;
	ThreadSafeQueue<int>* queue;
	Mutex* seen_mutex;
	std::vector<bool>* seen;
};


static void doMultipleReaderThreadTest(int num_reader_threads, int num_items, int between_enqueue_sleep_time_ms, int num_items_to_enqueue_at_once, double read_timeout_s)
{
	testAssert(num_items % num_items_to_enqueue_at_once == 0);

	ThreadSafeQueue<int> queue;

	std::vector<bool> seen(num_items);
	Mutex seen_mutex;

	std::vector<Reference<ThreadSafeQueueTestReaderThread> > reader_threads;
	for(int i=0; i<num_reader_threads; ++i)
	{
		reader_threads.push_back(new ThreadSafeQueueTestReaderThread());
		reader_threads.back()->queue = &queue;
		reader_threads.back()->read_timeout_s = read_timeout_s;
		reader_threads.back()->seen = &seen;
		reader_threads.back()->seen_mutex = &seen_mutex;

		reader_threads.back()->launch();
	}
	std::vector<int> items(num_items_to_enqueue_at_once);
	for(int i=0; i<num_items; i+=num_items_to_enqueue_at_once)
	{
		if(between_enqueue_sleep_time_ms > 0)
			PlatformUtils::Sleep(between_enqueue_sleep_time_ms);

		if(num_items_to_enqueue_at_once == 1)
			queue.enqueue(i);
		else
		{
			for(int z=0; z<num_items_to_enqueue_at_once; ++z)
				items[z] = i + z;
		
			queue.enqueueItems(items.data(), items.size());
		}
	}

	// Enqueue TERMINATING_INTEGERs to tell reader threads to terminate
	for(int i=0; i<num_reader_threads; ++i)
		queue.enqueue(TERMINATING_INTEGER);

	// Wait for reader threads to finish.
	for(int i=0; i<num_reader_threads; ++i)
		reader_threads[i]->join();

	// Check all items were actually dequeued properly.
	for(int i=0; i<num_items; ++i)
		testAssert(seen[i]);
}


static void doMultipleReaderThreadPerfTest(int num_reader_threads, int num_items)
{
	Timer timer;
	ThreadSafeQueue<int> queue;

	std::vector<Reference<ThreadSafeQueueTestReaderThread> > reader_threads;
	for(int i=0; i<num_reader_threads; ++i)
	{
		reader_threads.push_back(new ThreadSafeQueueTestReaderThread());
		reader_threads.back()->queue = &queue;
		reader_threads.back()->read_timeout_s = 0;
		reader_threads.back()->launch();
	}
	for(int i=0; i<num_items; ++i)
		queue.enqueue(i);

	// Enqueue TERMINATING_INTEGERs to tell reader threads to terminate
	for(int i=0; i<num_reader_threads; ++i)
		queue.enqueue(TERMINATING_INTEGER);

	// Wait for reader threads to finish.
	for(int i=0; i<num_reader_threads; ++i)
		reader_threads[i]->join();

	conPrint("Sending " + toString(num_items) + " items took " + timer.elapsedStringNSigFigs(4));
}


void doThreadSafeQueueTests()
{
	conPrint("doThreadSafeQueueTests()");

	// Test enqueue and dequeue
	{
		ThreadSafeQueue<int> queue;

		queue.enqueue(1);

		int x;
		queue.dequeue(x);
		testAssert(x == 1);

		queue.enqueue(2);
		queue.enqueue(3);

		queue.dequeue(x);
		testAssert(x == 2);
		queue.dequeue(x);
		testAssert(x == 3);
	}

	// Test enqueueItems
	{
		ThreadSafeQueue<int> queue;
		int items[] = { 1, 2, 3, 4 };

		queue.enqueueItems(items, staticArrayNumElems(items));

		int x;
		queue.dequeue(x);
		testAssert(x == 1);
		queue.dequeue(x);
		testAssert(x == 2);
		queue.dequeue(x);
		testAssert(x == 3);
		queue.dequeue(x);
		testAssert(x == 4);
	}

	// Do some tests with multiple threads
	{
		conPrint("testing with multiple threads.");
		doMultipleReaderThreadTest(/*num reader threads=*/1,  /*num items=*/10000, /*between_enqueue_sleep_time_ms=*/0, /*num_items_to_enqueue_at_once=*/1, /*read_timeout_s=*/0.0);
		doMultipleReaderThreadTest(/*num reader threads=*/2,  /*num items=*/10000, /*between_enqueue_sleep_time_ms=*/0, /*num_items_to_enqueue_at_once=*/1, /*read_timeout_s=*/0.0);
		doMultipleReaderThreadTest(/*num reader threads=*/10, /*num items=*/10000, /*between_enqueue_sleep_time_ms=*/0, /*num_items_to_enqueue_at_once=*/1, /*read_timeout_s=*/0.0);

		// Test enqueuing multiple items at once
		conPrint("Test enqueuing multiple items at once.");
		doMultipleReaderThreadTest(/*num reader threads=*/1,  /*num items=*/10000, /*between_enqueue_sleep_time_ms=*/0, /*num_items_to_enqueue_at_once=*/10 , /*read_timeout_s=*/0.0);
		doMultipleReaderThreadTest(/*num reader threads=*/2,  /*num items=*/10000, /*between_enqueue_sleep_time_ms=*/0, /*num_items_to_enqueue_at_once=*/10 , /*read_timeout_s=*/0.0);
		doMultipleReaderThreadTest(/*num reader threads=*/10, /*num items=*/10000, /*between_enqueue_sleep_time_ms=*/0, /*num_items_to_enqueue_at_once=*/10 , /*read_timeout_s=*/0.0);
		doMultipleReaderThreadTest(/*num reader threads=*/10, /*num items=*/10000, /*between_enqueue_sleep_time_ms=*/0, /*num_items_to_enqueue_at_once=*/100, /*read_timeout_s=*/0.0);

		// Run with a small sleep between enqueuing messages, to make sure that the reader threads are suspended.
		conPrint("Test small sleep between enqueuing messages.");
		doMultipleReaderThreadTest(/*num reader threads=*/1,  /*num items=*/100, /*between_enqueue_sleep_time_ms=*/1, /*num_items_to_enqueue_at_once=*/1 , /*read_timeout_s=*/0.0);
		doMultipleReaderThreadTest(/*num reader threads=*/2,  /*num items=*/100, /*between_enqueue_sleep_time_ms=*/1, /*num_items_to_enqueue_at_once=*/1 , /*read_timeout_s=*/0.0);
		doMultipleReaderThreadTest(/*num reader threads=*/10, /*num items=*/100, /*between_enqueue_sleep_time_ms=*/1, /*num_items_to_enqueue_at_once=*/1 , /*read_timeout_s=*/0.0);
		doMultipleReaderThreadTest(/*num reader threads=*/10, /*num items=*/100, /*between_enqueue_sleep_time_ms=*/1, /*num_items_to_enqueue_at_once=*/10, /*read_timeout_s=*/0.0);

		// Test with read timeouts
		conPrint("Test with read timeouts.");
		doMultipleReaderThreadTest(/*num reader threads=*/1,  /*num items=*/50, /*between_enqueue_sleep_time_ms=*/1, /*num_items_to_enqueue_at_once=*/1, /*read_timeout_s=*/0.001);
		doMultipleReaderThreadTest(/*num reader threads=*/2,  /*num items=*/50, /*between_enqueue_sleep_time_ms=*/1, /*num_items_to_enqueue_at_once=*/1, /*read_timeout_s=*/0.001);
		doMultipleReaderThreadTest(/*num reader threads=*/10, /*num items=*/50, /*between_enqueue_sleep_time_ms=*/1, /*num_items_to_enqueue_at_once=*/1, /*read_timeout_s=*/0.001);

		// Test with a larger timeout
		doMultipleReaderThreadTest(/*num reader threads=*/10, /*num items=*/100, /*between_enqueue_sleep_time_ms=*/1, /*num_items_to_enqueue_at_once=*/1, /*read_timeout_s=*/1.0);
		doMultipleReaderThreadTest(/*num reader threads=*/10, /*num items=*/10000, /*between_enqueue_sleep_time_ms=*/0, /*num_items_to_enqueue_at_once=*/10, /*read_timeout_s=*/1.0);
	}

	{
		conPrint("Running perf tests...");
		doMultipleReaderThreadPerfTest(/*num reader threads=*/10, /*num items=*/100000);
	}

	conPrint("doThreadSafeQueueTests() done.");
}

#endif // BUILD_TESTS
