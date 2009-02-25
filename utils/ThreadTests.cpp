/*=====================================================================
ThreadTests.cpp
---------------
File created by ClassTemplate on Wed Feb 25 13:44:48 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "ThreadTests.h"


#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"
#include "../utils/stringutils.h"
#include "../utils/timer.h"
#include "ThreadManager.h"
#include "threadsafequeue.h"
#include "mythread.h"


ThreadTests::ThreadTests()
{
	
}


ThreadTests::~ThreadTests()
{
	
}


static const int TERMINATING_INTEGER = 10000;


class TestReaderThread : public MyThread
{
public:
	TestReaderThread(ThreadSafeQueue<int>* queue_) : queue(queue_) {}
	
	virtual void run()
	{
		while(1)
		{
			int x;
			queue->dequeue(x);
			//conPrint("Got " + toString(x));

			if(x == TERMINATING_INTEGER)
				break;
		}
		conPrint("TestReaderThread finished");
	}

	ThreadSafeQueue<int>* queue;
};


class TestReaderThread2 : public MyThread
{
public:
	TestReaderThread2(ThreadSafeQueue<int>* queue_) : queue(queue_) {}
	
	virtual void run()
	{
		bool done = false;
		while(!done)
		{
			Lock lock(queue->getMutex());

			while(!queue->unlockedEmpty())
			{
				int x;
				queue->unlockedDequeue(x);
				//conPrint("Got " + toString(x));

				if(x == TERMINATING_INTEGER)
					done = true;
			}
		}
		conPrint("TestReaderThread2 finished");
	}

	ThreadSafeQueue<int>* queue;
};


class TestWriterThread : public MyThread
{
public:
	TestWriterThread(ThreadSafeQueue<int>* queue_) : queue(queue_) {}
	
	virtual void run()
	{
		for(int x=0; x<=TERMINATING_INTEGER; ++x)
		{
			queue->enqueue(x);
		}
		conPrint("TestWriterThread finished");
	}

	ThreadSafeQueue<int>* queue;
};


void ThreadTests::test()
{
	{
		ThreadSafeQueue<int> queue;
		Timer timer;
		TestReaderThread* reader = new TestReaderThread(&queue); 
		reader->launch();

		TestWriterThread* writer = new TestWriterThread(&queue); 
		writer->launch();

		reader->join();
		writer->join();

		testAssert(queue.empty());
		conPrint(toString(timer.getSecondsElapsed()));
	}

	
	{
		ThreadSafeQueue<int> queue;
		Timer timer;
		TestReaderThread2* reader = new TestReaderThread2(&queue); 
		reader->launch();

		TestWriterThread* writer = new TestWriterThread(&queue); 
		writer->launch();

		reader->join();
		writer->join();

		testAssert(queue.empty());
		conPrint(toString(timer.getSecondsElapsed()));
	}


	{
		ThreadSafeQueue<int> queue;
		Timer timer;
		TestReaderThread* reader = new TestReaderThread(&queue); 
		reader->launch();

		TestWriterThread* writer = new TestWriterThread(&queue); 
		writer->launch();
		TestWriterThread* writer2 = new TestWriterThread(&queue); 
		writer2->launch();

		reader->join();
		writer->join();
		writer2->join();

		conPrint(toString(timer.getSecondsElapsed()));
	}
}





