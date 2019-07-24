/*=====================================================================
ThreadSafeQueue.h
-----------------
File created by ClassTemplate on Fri Sep 19 01:55:10 2003
Code By Nicholas Chapman.
=====================================================================*/
#pragma once


#include "Mutex.h"
#include "Lock.h"
#include "Condition.h"
#include "CircularBuffer.h"
//#include <vector>
#include <cassert>


/*=====================================================================
ThreadSafeQueue
---------------

=====================================================================*/
template <class T>
class ThreadSafeQueue
{
public:
	inline ThreadSafeQueue();

	inline ~ThreadSafeQueue();

	/*=====================================================================
	enqueue
	-------
	adds an item to the back of the queue

	Locks the queue.
	=====================================================================*/
	inline void enqueue(const T& t);

	inline void enqueueItems(const T* items, size_t num_items);

	inline Mutex& getMutex();


	//returns true if object removed from queue.
	//NOTE: MUST have aquired lock on mutex b4 using this function
	//inline bool pollDequeueUnlocked(T& t_out);

	/*=====================================================================
	pollDequeueLocked
	-----------------
	Tries to remove an object from the front of the queue.
	If the queue is non empty, then it sets t_out to the item removed
	from the queue and returns true.  Otherwise returns false.

	This version locks the queue.
	=====================================================================*/
	//inline bool pollDequeueLocked(T& t_out);

	//don't call unless queue is locked.
	inline void unlockedDequeue(T& t_out);

	//inline void unlockedEnqueue(const T& t);

	// Suspends thread until queue is non-empty
	void dequeue(T& t_out);

	/*
	Suspends thread until queue is non-empty, or wait_time_seconds has elapsed.
	Returns true if object dequeued, false if timeout occured.
	*/
	bool dequeueWithTimeout(double wait_time_seconds, T& t_out);

	// Threadsafe, appends all elements in queue to vec_out.
	//void dequeueAppendToVector(std::vector<T>& vec_out);

	//threadsafe
	inline bool empty() const;
	inline size_t size() const;
	inline void clear();
	inline void clearAndFreeMem();

	inline bool unlockedEmpty() const;

	//const std::list<T>& getQueueDebug() const { return queue; }

	typedef typename CircularBuffer<T>::iterator iterator;
	// Not threadsafe, caller needs to have the mutex first.
	iterator begin() { return queue.beginIt(); }
	iterator end() { return queue.endIt(); }

	void setAllocator(const Reference<glare::Allocator>& al) { queue.setAllocator(al); }
private:
	CircularBuffer<T> queue;

	mutable Mutex mutex;

	Condition nonempty;
};


template <class T>
ThreadSafeQueue<T>::ThreadSafeQueue()
{
}


template <class T>
ThreadSafeQueue<T>::~ThreadSafeQueue()
{
}


template <class T>
void ThreadSafeQueue<T>::enqueue(const T& t)
{	
	Lock lock(mutex); // Lock the queue

	queue.push_back(t); // Add item to queue

	nonempty.notify(); // Notify suspended threads that there is an item in the queue.
}


template <class T>
void ThreadSafeQueue<T>::enqueueItems(const T* items, size_t num_items)
{
	Lock lock(mutex); // Lock the queue

	for(size_t i=0; i<num_items; ++i)
		queue.push_back(items[i]); // Add item to queue

	nonempty.notifyAll(); // Notify suspended threads that there are items in the queue.
}


/*template <class T>
void ThreadSafeQueue<T>::unlockedEnqueue(const T& t)
{
	queue.push_back(t); // Add item to queue

	if(queue.size() == 1) // If the queue was empty
		nonempty.notify(); // Notify suspended threads that there is an item in the queue.
}*/


template <class T>
Mutex& ThreadSafeQueue<T>::getMutex()
{
	return mutex;
}


//returns true if object removed from queue.
/*template <class T>
bool ThreadSafeQueue<T>::pollDequeueUnlocked(T& t_out)
{
	//Lock lock(mutex);
	
	if(queue.empty())
		return false;

	t_out = queue.front();
	queue.pop_front();
	return true;
}*/


//returns true if object removed from queue.
/*template <class T>
bool ThreadSafeQueue<T>::pollDequeueLocked(T& t_out)
{
	Lock lock(mutex);
	
	if(queue.empty())
		return false;

	t_out = queue.front();
	queue.pop_front();
	return true;
}*/


template <class T>
void ThreadSafeQueue<T>::unlockedDequeue(T& t_out)
{
	if(queue.empty())
	{
		// Uhoh, tried to dequeue when the queue was empty...
		// t_out will be undefined.
		assert(0);
		return;
	}

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
	while(1)
	{
		Lock lock(mutex); // Lock queue

		if(!queue.empty())
		{
			// No need to wait for condition, just return the value now
			unlockedDequeue(t_out);
			return;
		}
		else // Else if queue is currently empty...
		{		
			// Suspend until queue is non-empty.
			nonempty.wait(
				mutex, 
				true, // infinite wait time
				0.0);

			if(queue.empty()) // If some sneaky other thread was woken up as well and snaffled the item...
				continue; // Try again

			unlockedDequeue(t_out);
			return;
		}
	}
}


template <class T>
bool ThreadSafeQueue<T>::dequeueWithTimeout(double wait_time_seconds, T& t_out)
{
	while(1)
	{
		Lock lock(mutex); // Lock queue

		if(!queue.empty())
		{
			// No need to wait for condition, just return the value now
			unlockedDequeue(t_out);
			return true;
		}
		else // Else if queue is currently empty...
		{
			// Suspend thread until there is something in the queue
			const bool condition_signalled = nonempty.wait(
				mutex,
				false, // infinite wait time
				wait_time_seconds
				);

			if(condition_signalled)
			{
				if(queue.empty()) // If empty
					continue; // Try again

				unlockedDequeue(t_out);
				return true;
			}
			else // Else wait timed out.
			{
				return false;
			}
		}
	}
}


/*template <class T>
void ThreadSafeQueue<T>::dequeueAppendToVector(std::vector<T>& vec_out)
{
	Lock lock(mutex); // Lock queue

	while(!queue.empty())
	{
		vec_out.push_back(queue.front());
		queue.pop_front();
	}

	nonempty.resetToFalse();
}*/
