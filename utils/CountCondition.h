/*=====================================================================
CountCondition.h
----------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "Condition.h"
#include "Mutex.h"
#include "Lock.h"
#include "Platform.h"


// This class is designed for when we have a single thread that wants to wait for other threads to do exactly N items of work.
// The waiting thread creates the count condition, specifying N, then the worker threads call increment().
// The waiting thread will block (be suspended) in the wait() call, then be notified and wake up when all the N items of work are done.
class CountCondition
{
public:
	CountCondition() : N(0), count(0) {}
	CountCondition(int64 N_) : N(N_), count(0) {}

	void setN(int64 N_) { N = N_; } // Can be called to initialise the CountCondition.
	int64 getN() const { return N; }

	// Waits until count reaches N.  Blocks.
	void wait()
	{
		Lock lock(mutex);
		assert(count >= 0 && count <= N);
		while(count < N)
			done_condition.wait(mutex);
	}

	void increment()
	{
		Lock lock(mutex);
		count++;
		assert(count <= N);
		if(count == N)
			done_condition.notify();
	}

	void decrement()
	{
		Lock lock(mutex);
		count--;
		assert(count >= 0);
	}

	Mutex mutex;
	Condition done_condition;
	int64 count;
	int64 N;
};
