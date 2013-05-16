;/*=====================================================================
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

=====================================================================*/
template <class T>
class CircularBuffer
{
public:
	inline CircularBuffer() : begin(0), end(0), num_items(0) { invariant(); }
	inline ~CircularBuffer() { invariant(); }

	inline void push_back(const T& t);
	
	inline void push_front(const T& t);

	inline void pop_back();
	
	inline void pop_front();

	inline T& front();

	inline T& back();
	
	inline size_t size() const;


	friend void circularBufferTest();

private:
	inline void increaseSize();
	inline void invariant();
	

	size_t begin, end;
	size_t num_items;
	std::vector<T> data;
};


void circularBufferTest();


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
	}
	else
	{
		// Data wrapped around case.
		assert(begin > end);
		assert((data.size() - begin) + end == num_items);
	}
}


template <class T>
void CircularBuffer<T>::increaseSize()
{
	// Resize buffer
	size_t old_size = data.size();

	data.resize(myMax<size_t>(1, old_size) * 2);

	// Copy data from [0, end) to [old_size, old_size + end)
	for(size_t i=0; i<end; ++i)
		data[old_size + i] = data[i];

	// Update end
	end = begin + num_items;
	assert(end < data.size());
}
