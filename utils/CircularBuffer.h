/*=====================================================================
CircularBuffer.h
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-05-16 16:42:23 +0100
=====================================================================*/
#pragma once


#include "../maths/mathstypes.h"
#include <vector>
#include <assert.h>


/*=====================================================================
CircularBuffer
-------------------
A dynamically growable circular buffer.
The interface should be similar to std::list
=====================================================================*/


template <class T> class CircularBufferIterator;


template <class T>
class CircularBuffer
{
public:
	inline CircularBuffer() : begin(0), end(0), num_items(0) { invariant(); }
	inline ~CircularBuffer() { invariant(); }

	inline void push_back(const T& t);
	
	inline void push_front(const T& t);

	// Undefined if empty.
	inline void pop_back();
	
	// Undefined if empty.
	inline void pop_front();

	inline T& front();

	inline T& back();
	
	inline size_t size() const;

	inline bool empty() const { return size() == 0; }


	typedef CircularBufferIterator<T> iterator;


	inline iterator beginIt() { return CircularBufferIterator<T>(this, begin, false); }
	inline iterator endIt() { return CircularBufferIterator<T>(this, end, !empty() && end <= begin); }


	friend void circularBufferTest();
	friend class CircularBufferIterator<T>;

private:
	inline void increaseSize();
	inline void invariant();
	

	size_t begin; // Index of first item in buffer
	size_t end; // Index one past last item in buffer.
	size_t num_items; // Number of items in buffer
	std::vector<T> data; // Underlying linear buffer.  Size will be >= num_items.
};


void circularBufferTest();


/*
Because begin=end can happen in two circumstances - num_items = 0 or num_items = data.size(), we need to distinguish between the two.
So we will keep track of if an iterator has wrapped past the end of the buffer back to the beginning.
So beginIt() == endIt() if they both index the same element, *and* they are both unwrapped or wrapped.
*/
template <class T>
class CircularBufferIterator
{
public:
	CircularBufferIterator(CircularBuffer<T>* buffer, size_t i_, bool wrapped_) : buffer(buffer), i(i_), wrapped(wrapped_) {}
	~CircularBufferIterator(){}

	bool operator == (const CircularBufferIterator& other) const
	{
		return i == other.i && wrapped == other.wrapped;
	}

	bool operator != (const CircularBufferIterator& other) const
	{
		return i != other.i || wrapped != other.wrapped;
	}

	void operator ++ ()
	{
		i++;
		if(i >= buffer->data.size())
		{
			i = 0;
			wrapped = true;
		}
	}

	T& operator * ()
	{
        return buffer->data[i];
    }
	const T& operator * () const
	{
        return buffer->data[i];
    }



	T* operator -> ()
	{
		return &buffer->data[i];
	}
	const T* operator -> () const
	{
		return &buffer->data[i];
	}

	CircularBuffer<T>* buffer;
	size_t i;
	bool wrapped;
};


template <class T>
void CircularBuffer<T>::push_back(const T& t)
{
	invariant();

	// If there is no free space
	if(num_items == data.size())
		increaseSize();

	data[end] = t;
	end++;

	// Wrap end
	if(end >= data.size())
		end -= data.size();

	num_items++;

	invariant();
}


template <class T>
void CircularBuffer<T>::push_front(const T& t)
{
	invariant();

	// If there is no free space
	if(num_items == data.size())
		increaseSize();

	if(begin == 0)
		begin = data.size() - 1;
	else
		begin--;

	data[begin] = t;

	num_items++;

	invariant();
}


template <class T>
void CircularBuffer<T>::pop_back()
{
	invariant();
	assert(num_items > 0);

	if(end == 0)
		end = data.size() - 1;
	else
		end--;

	num_items--;

	invariant();
}


template <class T>
void CircularBuffer<T>::pop_front()
{
	invariant();
	assert(num_items > 0);

	if(begin == data.size() - 1)
		begin = 0;
	else 
		begin++;

	num_items--;

	invariant();
}


template <class T>
T& CircularBuffer<T>::front()
{
	invariant();
	assert(num_items > 0);

	return data[begin];
}


template <class T>
T& CircularBuffer<T>::back()
{
	invariant();
	assert(num_items > 0);

	if(end == 0)
		return data.back();
	else
		return data[end - 1];
}


template <class T>
size_t CircularBuffer<T>::size() const { return num_items; }


template <class T>
void CircularBuffer<T>::invariant()
{
#ifndef NDEBUG // If in debug mode:
	assert(num_items <= data.size());

	if(data.size() == 0)
	{
		assert(begin == 0 && end == 0);
	}
	else
	{
		assert(begin < data.size());
		assert(end < data.size());
	}

	if(begin < end)
	{
		assert(num_items == end - begin);
	}
	else if(begin == end)
	{
		assert(num_items == 0 || num_items == data.size());
	}
	else
	{
		// Data wrapped around case.
		assert(begin > end);
		assert((data.size() - begin) + end == num_items);
	}
#endif
}


// Resize buffer.  Need to make it least twice as bug so that we can copy all the wrapped elements directly.
template <class T>
void CircularBuffer<T>::increaseSize()
{
	const size_t old_size = data.size();

	data.resize(myMax<size_t>(1, old_size) * 2);

	if(end <= begin) // If the data was wrapping before the resize:
	{
		// Copy data from [0, end) to [old_size, old_size + end)
		for(size_t i=0; i<end; ++i)
			data[old_size + i] = data[i];

		// Update end since data is no longer wrapping.
		end = begin + num_items;
	}

	assert(end == begin + num_items);
	assert(end < data.size());
}
