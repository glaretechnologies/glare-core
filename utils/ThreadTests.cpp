/*=====================================================================
ThreadTests.cpp
---------------
File created by ClassTemplate on Wed Feb 25 13:44:48 2009
Code By Nicholas Chapman.
=====================================================================*/
#include "ThreadTests.h"


#if BUILD_TESTS


#include "../indigo/TestUtils.h"
#include "../indigo/globals.h"
#include "../utils/StringUtils.h"
#include "../utils/Timer.h"
#include "ThreadManager.h"
#include "ThreadSafeQueue.h"
#include "MyThread.h"
#include "ParallelFor.h"
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


/*class OpenMPUsingThread : public MyThread
{
public:
	virtual void run()
	{
		result = 0;
		int N = 1000;

		#pragma omp parallel for
		for(int i=0; i<N; ++i)
			result += pow(1.00001, (double)i);

		conPrint("Result: " + toString(result));
	}

	double result;
};*/


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


class ThreadLocalPerfTestThread : public MyThread
{
public:
	virtual void run()
	{
		testAssert(test_thread_local_int == 0);

		Timer timer;

		const int N = 10000;
		for(int z=0; z<N; ++z)
			test_thread_local_int++;

		conPrint("ThreadLocalPerfTestThread elapsed per op: " + doubleToStringNSigFigs(1.0e9 * timer.elapsed() / N, 5) + " ns");

		testAssert(test_thread_local_int == N);
	}
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
	// NOTE: What's a good way of doing a meaningful performance test of thread local storage?
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

	// Test OpenMP from another thread:
	/*{
		for(int i=0; i<100; ++i)
		{
			OpenMPUsingThread* t = new OpenMPUsingThread();
			t->launch(false);
			t->join();
			delete t;

			Sleep(1000);
		}
	}*/

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


	// Test parallel For
	for(int z=0; z<1; ++z)
	{
		const int N = 1000000;
		std::vector<double> v(N, 1.0f);

		//////////// Do pass to warm up caches //////////////
		for(int i=0; i<N; ++i)
			v[i] = (1.0 / std::pow(2.0, (double)i));

		// Compute sum
		double dummy = 0;
		for(int i=0; i<N; ++i)
			dummy += v[i];


		Timer timer;

		////////////// Try OpenMP ////////////////////////
		timer.reset();

		//#ifndef INDIGO_NO_OPENMP
		//#pragma omp parallel for
		//#endif
		for(int i=0; i<N; ++i)
			v[i] = (1.0 / std::pow(2.0, (double)i));

		const double openmp_elapsed = timer.elapsed();

		// Compute sum
		double sum_3 = 0;
		for(int i=0; i<N; ++i)
			sum_3 += v[i];


		//////////// Run ParallelFor ////////////////////
		

		timer.reset();
		TestComputation t(v);
		ParallelFor::exec<TestComputation>(t, 0, N);

		const double parallel_for_elapsed = timer.elapsed();

		// Compute sum
		double sum_1 = 0;
		for(int i=0; i<N; ++i)
			sum_1 += v[i];


		/////////// Try single threaded performance /////////////
		timer.reset();

		for(int i=0; i<N; ++i)
			v[i] = (1.0 / std::pow(2.0, (double)i));

		const double singlethread_elapased = timer.elapsed();

		// Compute sum
		double sum_2 = 0;
		for(int i=0; i<N; ++i)
			sum_2 += v[i];

		testAssert(sum_1 == sum_2);
		testAssert(sum_1 == sum_3);

		conPrint("sum_1: " + toString(sum_1));
		conPrint("parallel_for_elapsed: " + toString(parallel_for_elapsed));
		conPrint("singlethread_elapased: " + toString(singlethread_elapased));
		conPrint("openmp_elapsed: " + toString(openmp_elapsed));
	}
	
	//exit(0);//TEMP








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
