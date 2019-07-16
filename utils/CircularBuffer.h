/*=====================================================================
CircularBuffer.h
-------------------
Copyright Glare Technologies Limited 2017 -
Generated at 2013-05-16 16:42:23 +0100
=====================================================================*/
#pragma once


#include "AlignedCharArray.h"
#include "../maths/mathstypes.h"
#include "../maths/SSE.h"
#include <assert.h>
#include <stddef.h>
#include <memory>


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
	inline CircularBuffer();
	inline ~CircularBuffer();

	inline void push_back(const T& t);
	
	inline void push_front(const T& t);

	// Undefined if empty.
	inline void pop_back();
	
	// Undefined if empty.
	inline void pop_front();

	inline T& front();

	inline T& back();

	inline void clear();
	inline void clearAndFreeMem();
	
	inline size_t size() const;

	inline bool empty() const { return num_items == 0; }


	typedef CircularBufferIterator<T> iterator;


	inline iterator beginIt() { return CircularBufferIterator<T>(this, begin, false); }
	inline iterator endIt() { return CircularBufferIterator<T>(this, end, !empty() && end <= begin); }


	friend void circularBufferTest();
	friend class CircularBufferIterator<T>;

private:
	INDIGO_DISABLE_COPY(CircularBuffer)
	inline void increaseSize();
	inline void invariant();
	

	size_t begin; // Index of first item in buffer
	size_t end; // Index one past last item in buffer.
	size_t num_items; // Number of items in buffer
	T* data; // Storage
	size_t data_size; // Size/capacity in number of elements of data.
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
		if(i >= buffer->data_size)
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
CircularBuffer<T>::CircularBuffer()
:	begin(0), end(0), num_items(0), data(0), data_size(0)
{
	invariant(); 
}


template <class T>
CircularBuffer<T>::~CircularBuffer()
{
	invariant();

	// Destroy first segment.
	const bool wrapped = ((end <= begin) && (num_items > 0));
	const size_t first_segment_end = wrapped ? data_size : end;
	for(size_t i=begin; i<first_segment_end; ++i)
		data[i].~T();
	// Destroy wrapped-around segment, if any.
	if(wrapped)
		for(size_t i=0; i<end; ++i)
			data[i].~T();

	SSE::alignedFree(data);
}


template <class T>
void CircularBuffer<T>::push_back(const T& t)
{
	invariant();

	// If there is no free space
	if(num_items == data_size)
		increaseSize();

	// Construct data[end] from t
	::new ((data + end)) T(t);
	end++;

	// Wrap end
	if(end >= data_size)
		end -= data_size;

	num_items++;

	invariant();
}


template <class T>
void CircularBuffer<T>::push_front(const T& t)
{
	invariant();

	// If there is no free space
	if(num_items == data_size)
		increaseSize();

	// Decrement and wrap begin
	if(begin == 0)
		begin = data_size - 1;
	else
		begin--;

	// Construct data[begin] from t
	::new ((data + begin)) T(t);

	num_items++;

	invariant();
}


template <class T>
void CircularBuffer<T>::pop_back()
{
	invariant();
	assert(num_items > 0);

	// Decrement and wrap end
	if(end == 0)
		end = data_size - 1;
	else
		end--;

	// Destroy object
	data[end].~T();

	num_items--;

	invariant();
}


template <class T>
void CircularBuffer<T>::pop_front()
{
	invariant();
	assert(num_items > 0);

	// Destroy object
	data[begin].~T();

	// Increment and wrap begin
	if(begin == data_size - 1)
		begin = 0;
	else 
		begin++;

	num_items--;

	invariant();
}


template <class T>
void CircularBuffer<T>::clear()
{
	invariant();

	// Destroy objects
	// Destroy first segment.
	const bool wrapped = ((end <= begin) && (num_items > 0));
	const size_t first_segment_end = wrapped ? data_size : end;
	for(size_t i=begin; i<first_segment_end; ++i)
		data[i].~T();
	// Destroy wrapped-around segment, if any.
	if(wrapped)
		for(size_t i=0; i<end; ++i)
			data[i].~T();
	
	begin = 0;
	end = 0;
	num_items = 0;

	invariant();
}


template <class T>
void CircularBuffer<T>::clearAndFreeMem()
{
	invariant();

	clear();

	SSE::alignedFree(data);
	data = 0;
	data_size = 0;

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
		return data[data_size - 1];
	else
		return data[end - 1];
}


template <class T>
size_t CircularBuffer<T>::size() const { return num_items; }


template <class T>
void CircularBuffer<T>::invariant()
{
#ifndef NDEBUG // If in debug mode:
	assert(num_items <= data_size);

	if(data_size == 0)
	{
		assert(begin == 0 && end == 0);
	}
	else
	{
		assert(begin < data_size);
		assert(end < data_size);
		assert(data);
	}

	if(begin < end)
	{
		// Unwrapped case:
		assert(num_items == end - begin);
	}
	else if(begin == end)
	{
		// Possibly wrapped case:
		assert(num_items == 0 || num_items == data_size);
	}
	else
	{
		// Data wrapped around case.
		assert(begin > end);
		assert((data_size - begin) + end == num_items);
	}
#endif
}


// Resize buffer.  Need to make it least twice as bug so that we can copy all the wrapped elements directly.
template <class T>
void CircularBuffer<T>::increaseSize()
{
	const size_t new_data_size = myMax<size_t>(4, data_size * 2);
	T* new_data = (T*)SSE::alignedMalloc(sizeof(T) * new_data_size, glare::AlignOf<T>::Alignment);

	// Copy-construct new objects from existing objects:
	// Copy data[begin] to data[first_segment_end], to new_data[begin] to new_data[first_segment_end]
	const bool wrapped = ((end <= begin) && (num_items > 0));
	const size_t first_segment_end = wrapped ? data_size : end;
	std::uninitialized_copy(data + begin, data + first_segment_end, new_data + begin);
	size_t new_end = end;
	if(wrapped) // If the data was wrapping before the resize:
	{
		// Copy data from [0, end) to [data_size, data_size + end)
		std::uninitialized_copy(data, data + end, new_data + data_size);

		// Update end since data is no longer wrapping.
		new_end = begin + num_items;
	}

	if(data)
	{
		// Destroy old objects:
		// Destroy first segment
		for(size_t i=begin; i<first_segment_end; ++i)
			data[i].~T();
		// Destroy wrapped-around segment, if any.
		if(wrapped)
			for(size_t i=0; i<end; ++i)
				data[i].~T();

		SSE::alignedFree(data);
	}
	data = new_data;
	end = new_end;
	data_size = new_data_size;

	assert(end == begin + num_items);
	assert(end < data_size);
}
