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
#include "ParallelFor.h"
#include <cmath>


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



class SimpleThread : public MyThread
{
public:
	virtual void run()
	{
	}
};


#if (BUILD_TESTS)
void ThreadTests::test()
{

	// Create and run a single thread.  Wait for it to finish.
	{
		SimpleThread* t = new SimpleThread();
		t->launch(false);
		t->join();
		delete t;
	}

	// Create and run a single thread.  Let it auto-delete itself.
	{
		SimpleThread* t = new SimpleThread();
		t->launch(true);
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
		/*
		// TEMP doesn't compile on linux :(
		struct TestComputation
		{
			TestComputation(std::vector<double>& v_) : v(v_) {}
			std::vector<double>& v;
			void operator() (int i)
			{
				v[i] = (1.0 / std::pow(2.0, (double)i));
			}
		};

		timer.reset();
		TestComputation t(v);
		ParallelFor::exec<TestComputation>(t, 0, N);

		const double parallel_for_elapsed = timer.elapsed();

		// Compute sum
		double sum_1 = 0;
		for(int i=0; i<N; ++i)
			sum_1 += v[i];*/


		/////////// Try single threaded performance /////////////
		timer.reset();

		for(int i=0; i<N; ++i)
			v[i] = (1.0 / std::pow(2.0, (double)i));

		const double singlethread_elapased = timer.elapsed();

		// Compute sum
		double sum_2 = 0;
		for(int i=0; i<N; ++i)
			sum_2 += v[i];

		/*testAssert(sum_1 == sum_2);
		testAssert(sum_1 == sum_3);

		conPrint("sum_1: " + toString(sum_1));
		conPrint("parallel_for_elapsed: " + toString(parallel_for_elapsed));
		conPrint("singlethread_elapased: " + toString(singlethread_elapased));
		conPrint("openmp_elapsed: " + toString(openmp_elapsed));*/
	}
	
	//exit(0);//TEMP








	{
		TestReaderThread* reader = NULL;
		TestWriterThread* writer = NULL;
		try
		{
			ThreadSafeQueue<int> queue;
			Timer timer;
			reader = new TestReaderThread(&queue); 
			writer = new TestWriterThread(&queue); 

			// We can't autodelete these threads because we want to join on them, and they might be killed before we call join() if autodelete is enabled.
			reader->launch(
				false // autodelete
			);
			writer->launch(
				false // autodelete			
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

		delete reader;
		delete writer;
	}

	
	{
		ThreadSafeQueue<int> queue;
		Timer timer;
		TestReaderThread2* reader = new TestReaderThread2(&queue); 
		reader->launch(false);

		TestWriterThread* writer = new TestWriterThread(&queue); 
		writer->launch(false);

		reader->join();
		writer->join();

		delete reader;
		delete writer;

		testAssert(queue.empty());
		conPrint(toString(timer.getSecondsElapsed()));
	}


	{
		ThreadSafeQueue<int> queue;
		Timer timer;
		TestReaderThread* reader = new TestReaderThread(&queue); 
		reader->launch(false);

		TestWriterThread* writer = new TestWriterThread(&queue); 
		writer->launch(false);
		TestWriterThread* writer2 = new TestWriterThread(&queue); 
		writer2->launch(false);

		reader->join();
		writer->join();
		writer2->join();

		delete reader;
		delete writer;
		delete writer2;

		conPrint(toString(timer.getSecondsElapsed()));
	}
}
#endif




