/*=====================================================================
ThreadSafeQueue.h
-----------------
Copyright Glare Technologies Limited 2020 -
=====================================================================*/
#pragma once


#include "Mutex.h"
#include "Lock.h"
#include "Condition.h"
#include "CircularBuffer.h"


/*=====================================================================
ThreadSafeQueue
---------------
Test are in doThreadSafeQueueTests() in ThreadSafeQueue.cpp, 
and also in ThreadTests.cpp.
=====================================================================*/
template <class T>
class ThreadSafeQueue
{
public:
	inline ThreadSafeQueue();

	inline ~ThreadSafeQueue();

	// Adds an item to the back of the queue
	// Locks the queue mutex (threadsafe)
	inline void enqueue(const T& t);

	inline void enqueueItems(const T* items, size_t num_items);

	inline Mutex& getMutex();


	// Don't call unless queue is locked.
	inline void unlockedDequeue(T& t_out);

	// Blocks - suspends calling thread until queue is non-empty.
	void dequeue(T& t_out);

	
	// Suspends calling thread until queue is non-empty, or wait_time_seconds has elapsed.
	// Returns true if object dequeued, false if timeout occured.
	bool dequeueWithTimeout(double wait_time_seconds, T& t_out);

	// Thread-safe:
	inline bool empty() const;
	inline size_t size() const;
	inline void clear();
	inline void clearAndFreeMem();

	inline bool unlockedEmpty() const;

	typedef typename CircularBuffer<T>::iterator iterator;
	// Not thread-safe, caller needs to have the mutex first.
	iterator begin() { return queue.beginIt(); }
	iterator end() { return queue.endIt(); }

	void setAllocator(const Reference<glare::Allocator>& al) { queue.setAllocator(al); }
	
private:
	CircularBuffer<T> queue;

	mutable Mutex mutex;

	Condition nonempty;
};


void doThreadSafeQueueTests();


template <class T>
ThreadSafeQueue<T>::ThreadSafeQueue()
{
}


template <class T>
ThreadSafeQueue<T>::~ThreadSafeQueue()
{
}


/*
The other way of implementing this is to check if the queue was empty, and then notify all suspended threads in that case, or don't notify any if it wasn't empty.
That approach is more than 2x as slow however, probably due to unnecessary wakeups of so many threads.
*/
template <class T>
void ThreadSafeQueue<T>::enqueue(const T& t)
{	
	{
		Lock lock(mutex); // Lock the queue

		queue.push_back(t); // Add item to queue
	}
	
	// According to MS "It is usually better to release the lock before waking other threads to reduce the number of context switches." (https://docs.microsoft.com/en-us/windows/win32/sync/condition-variables)
	nonempty.notify(); // Notify one or more suspended threads that there is an item in the queue.
}


template <class T>
void ThreadSafeQueue<T>::enqueueItems(const T* items, size_t num_items)
{
	{
		Lock lock(mutex); // Lock the queue

		for(size_t i=0; i<num_items; ++i)
			queue.push_back(items[i]); // Add item to queue
	}

	nonempty.notifyAll(); // Notify all suspended threads that there are items in the queue.  NOTE: the use of notifyAll here is potentially slow if the number of threads is >> num_items.
}


template <class T>
Mutex& ThreadSafeQueue<T>::getMutex()
{
	return mutex;
}


template <class T>
void ThreadSafeQueue<T>::unlockedDequeue(T& t_out)
{
	t_out = queue.front();
	queue.pop_front();
}


template <class T>
bool ThreadSafeQueue<T>::empty() const
{
	Lock lock(mutex);

	return queue.empty();
}


template <class T>
size_t ThreadSafeQueue<T>::size() const
{
	Lock lock(mutex);

	return queue.size();
}


template <class T>
bool ThreadSafeQueue<T>::unlockedEmpty() const
{
	return queue.empty();
}


template <class T>
void ThreadSafeQueue<T>::clear()
{
	Lock lock(mutex);

	queue.clear();
}


template <class T>
void ThreadSafeQueue<T>::clearAndFreeMem()
{
	Lock lock(mutex);

	queue.clearAndFreeMem();
}


template <class T>
void ThreadSafeQueue<T>::dequeue(T& t_out)
{
	Lock lock(mutex); // Lock queue

	while(queue.empty())
		nonempty.wait(mutex); // Suspend until queue is non-empty, or we get a spurious wake up.

	unlockedDequeue(t_out);
}


template <class T>
bool ThreadSafeQueue<T>::dequeueWithTimeout(double wait_time_seconds, T& t_out)
{
	Lock lock(mutex); // Lock queue

	while(queue.empty())
	{
		const bool condition_signalled = nonempty.waitWithTimeout(mutex, wait_time_seconds);
		if(!condition_signalled) // Wait timed out, or there was a spurious wake up.  TODO: wait again if this was a spurious wake up?
			return false;
	}

	assert(!queue.empty()); // We have exited from while(queue.empty()) loop, and we still hold the mutex, so queue should be non-empty.
	unlockedDequeue(t_out);
	return true;
}
