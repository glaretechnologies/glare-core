/*=====================================================================
threadsafequeue.h
-----------------
File created by ClassTemplate on Fri Sep 19 01:55:10 2003
Code By Nicholas Chapman.
=====================================================================*/
#ifndef __THREADSAFEQUEUE_H_666_
#define __THREADSAFEQUEUE_H_666_

// #pragma warning(disable : 4786)//disable long debug name warning

#include "mutex.h"
#include "lock.h"
#include "Condition.h"
#include <list>
#include <assert.h>


/*=====================================================================
ThreadSafeQueue
---------------

=====================================================================*/
template <class T>
class ThreadSafeQueue
{
public:
	/*=====================================================================
	ThreadSafeQueue
	---------------
	
	=====================================================================*/
	inline ThreadSafeQueue();

	inline ~ThreadSafeQueue();

	/*=====================================================================
	enqueue
	-------
	adds an item to the back of the queue

	Locks the queue.
	=====================================================================*/
	inline void enqueue(T& t);


	inline Mutex& getMutex();


	//returns true if object removed from queue.
	//NOTE: MUST have aquired lock on mutex b4 using this function
	inline bool pollDequeueUnlocked(T& t_out);

	/*=====================================================================
	pollDequeueLocked
	-----------------
	Tries to remove an object from the front of the queue.
	If the queue is non empty, then it sets t_out to the item removed
	from the queue and returns true.  Otherwise returns false.

	This version locks the queue.
	=====================================================================*/
	inline bool pollDequeueLocked(T& t_out);

	//don't call unless queue is locked.
	inline void unlockedDequeue(T& t_out);

	inline void unlockedEnqueue(T& t);

	//suspends thread until queue is non-empty
	void dequeue(T& t_out);

	//threadsafe
	inline bool empty() const;
	inline int size() const;
	void clear();

private:
	std::list<T> queue;

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
void ThreadSafeQueue<T>::enqueue(T& t)
{	
	Lock lock(mutex);//lock the queue

	queue.push_back(t);//add item to queue

	if(queue.size() == 1)//if the queue was empty
		nonempty.notify();//notify suspended threads that there is an item in the queue
}

template <class T>
void ThreadSafeQueue<T>::unlockedEnqueue(T& t)
{
	queue.push_back(t);//add item to queue

	if(queue.size() == 1)//if the queue was empty
		nonempty.notify();//notify suspended threads that there is an item in the queue
}


template <class T>
Mutex& ThreadSafeQueue<T>::getMutex()
{
	return mutex;
}

	//returns true if object removed from queue.
template <class T>
bool ThreadSafeQueue<T>::pollDequeueUnlocked(T& t_out)
{
	//Lock lock(mutex);
	
	if(queue.empty())
		return false;

	t_out = queue.front();
	queue.pop_front();
	return true;
}

	//returns true if object removed from queue.
template <class T>
bool ThreadSafeQueue<T>::pollDequeueLocked(T& t_out)
{
	Lock lock(mutex);
	
	if(queue.empty())
		return false;

	t_out = queue.front();
	queue.pop_front();
	return true;
}

	//returns true if object removed from queue.
template <class T>
void ThreadSafeQueue<T>::unlockedDequeue(T& t_out)
{
	if(queue.empty())
	{
		//uhoh, tryed to dequeue when the queue was empty...
		//t_out will be undefined.
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
int ThreadSafeQueue<T>::size() const
{
	Lock lock(mutex);

	return queue.size();
}
template <class T>
void ThreadSafeQueue<T>::clear()
{
	Lock lock(mutex);

	queue.clear();
}

template <class T>
void ThreadSafeQueue<T>::dequeue(T& t_out)
{
	while(1)
	{
		Lock lock(mutex);//lock queue

		if(!queue.empty())
		{
			//no need to wait for condition, just return the value now
			unlockedDequeue(t_out);
			return;
		}
		else //else if queue is currently empty...
		{		
			///suspend until queue is non-empty///
			nonempty.wait(mutex);

			if(queue.empty())//if empty
				continue;//try again

			unlockedDequeue(t_out);

			///reset the nonempty condition if neccessary
			if(queue.empty())
				nonempty.resetToFalse();

			return;
		}
	}
}

#endif //__THREADSAFEQUEUE_H_666_




