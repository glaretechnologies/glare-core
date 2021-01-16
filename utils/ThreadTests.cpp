/*=====================================================================
ThreadTests.cpp
---------------
File created by ClassTemplate on Wed Feb 25 13:44:48 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "ThreadTests.h"


#if BUILD_TESTS


#include "TestUtils.h"
#include "ConPrint.h"
#include "StringUtils.h"
#include "Timer.h"
#include "ThreadManager.h"
#include "ThreadSafeQueue.h"
#include "MyThread.h"
#include "PlatformUtils.h"
#include <cmath>


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



class SimpleThread : public MyThread
{
public:
	SimpleThread() { conPrint("SimpleThread()"); }
	~SimpleThread() { conPrint("~SimpleThread()"); }
	virtual void run()
	{
	}
};


// This class is just for testing thread sanitizer.
class RaceTestThread : public MyThread
{
public:
	RaceTestThread(int* i_): i(i_) { conPrint("RaceTestThread()"); }
	~RaceTestThread() { conPrint("~RaceTestThread()"); }
	virtual void run()
	{
		for(int z=0; z<1000; ++z)
		{
			if(z % 2 == 0)
				(*i)++;
			else
				(*i)--;
		}
	}

private:
	int* i;
};


struct TestComputation
{
	TestComputation(std::vector<double>& v_) : v(v_) {}
	std::vector<double>& v;
	void operator() (int i, int thread_index) const
	{
		v[i] = (1.0 / std::pow(2.0, (double)i));
	}
};



static GLARE_THREAD_LOCAL int test_thread_local_int = 0;


class ThreadLocalTestThread : public MyThread
{
public:
	virtual void run()
	{
		testAssert(test_thread_local_int == 0);

		for(int z=0; z<1000; ++z)
			test_thread_local_int++;

		testAssert(test_thread_local_int == 1000);
	}
};


struct ThreadLocalTestStruct
{
	int i;
};

static GLARE_THREAD_LOCAL ThreadLocalTestStruct* test_struct;


// Forced no-inline so it does TLS lookup every time.
static GLARE_NO_INLINE void doTestStructIncrement()
{
	test_struct->i++;
}


class ThreadLocalPerfTestThread : public MyThread
{
public:
	virtual void run()
	{
		test_struct = new ThreadLocalTestStruct();
		test_struct->i = 0;

		Timer timer;

		const int N = 1000000;
		for(int z=0; z<N; ++z)
			doTestStructIncrement();

		testAssert(test_struct->i == N);
		this->elapsed_per_op = timer.elapsed() / N;

		PlatformUtils::Sleep(1);

		delete test_struct;
	}

	double elapsed_per_op;
};


void ThreadTests::test()
{
	conPrint("ThreadTests::test()");


	// Test thread-local storage
	{
		testAssert(test_thread_local_int == 0);

		Reference<ThreadLocalTestThread> t1 = new ThreadLocalTestThread(); 
		Reference<ThreadLocalTestThread> t2 = new ThreadLocalTestThread(); 
		t1->launch();
		t2->launch();
		t1->join();
		t2->join();

		testAssert(test_thread_local_int == 0);
	}

	// Do thread-local storage performance test.
	{
		testAssert(test_thread_local_int == 0);

		Reference<ThreadLocalPerfTestThread> t1 = new ThreadLocalPerfTestThread(); 
		Reference<ThreadLocalPerfTestThread> t2 = new ThreadLocalPerfTestThread();
		t1->launch();
		t2->launch();
		t1->join();
		t2->join();

		testAssert(test_thread_local_int == 0);
	}

	// Do another thread-local storage performance test.
	{
		testAssert(test_thread_local_int == 0);

		std::vector<Reference<ThreadLocalPerfTestThread> > threads;
		for(size_t i=0; i<4; ++i)
		{
			Reference<ThreadLocalPerfTestThread> t1 = new ThreadLocalPerfTestThread(); 
			threads.push_back(t1);
			t1->launch();
		}

		for(size_t i=0; i<threads.size(); ++i)
			threads[i]->join();

		for(size_t i=0; i<threads.size(); ++i)
			conPrint("ThreadLocalPerfTestThread test struct deref elapsed per op: " + doubleToStringNSigFigs(1.0e9 * threads[i]->elapsed_per_op, 5) + " ns");

		testAssert(test_thread_local_int == 0);
	}

	// Create and run race test threads.  This is just for testing thread sanitizer.
	/*{
		int i = 0;
		Reference<RaceTestThread> t = new RaceTestThread(&i);
		t->launch();
		Reference<RaceTestThread> t2 = new RaceTestThread(&i);
		t2->launch();
		t->join();
		t2->join();

		printVar(i);
	}*/

	// Create and run a single thread.  Wait for it to finish.
	{
		Reference<SimpleThread> t = new SimpleThread();
		t->launch(/*false*/);
		t->join();
		//delete t;
	}

	// Create and run a single thread.  Let it auto-delete itself.
	{
		Reference<SimpleThread> t = new SimpleThread();
		t->launch(/*true*/);
	}

	{
		Reference<TestReaderThread> reader;
		Reference<TestWriterThread> writer;
		try
		{
			ThreadSafeQueue<int> queue;
			Timer timer;
			reader = new TestReaderThread(&queue); 
			writer = new TestWriterThread(&queue); 

			// We can't autodelete these threads because we want to join on them, and they might be killed before we call join() if autodelete is enabled.
			reader->launch(
				//false // autodelete
			);
			writer->launch(
				//false // autodelete			
			);

			reader->join();
			writer->join();

			testAssert(queue.empty());
			conPrint(toString(timer.getSecondsElapsed()));
		}
		catch(MyThreadExcep& )
		{		
			failTest("MyThreadExcep");
		}
		catch(...)
		{
			failTest("Other exception");
		}

		//delete reader;
		//delete writer;
	}

	
	{
		ThreadSafeQueue<int> queue;
		Timer timer;
		Reference<TestReaderThread2> reader = new TestReaderThread2(&queue); 
		reader->launch(/*false*/);

		Reference<TestWriterThread> writer = new TestWriterThread(&queue); 
		writer->launch(/*false*/);

		reader->join();
		writer->join();

		//delete reader;
		//delete writer;

		testAssert(queue.empty());
		conPrint(toString(timer.getSecondsElapsed()));
	}


	{
		ThreadSafeQueue<int> queue;
		Timer timer;
		Reference<TestReaderThread> reader = new TestReaderThread(&queue); 
		reader->launch(/*false*/);

		Reference<TestWriterThread> writer = new TestWriterThread(&queue); 
		writer->launch(/*false*/);
		Reference<TestWriterThread> writer2 = new TestWriterThread(&queue); 
		writer2->launch(/*false*/);

		reader->join();
		writer->join();
		writer2->join();

		//delete reader;
		//delete writer;
		//delete writer2;

		conPrint(toString(timer.getSecondsElapsed()));
	}

	conPrint("ThreadTests::test() done.");
}


#endif // BUILD_TESTS
