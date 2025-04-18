/*=====================================================================
CircularBuffer.h
----------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "GlareAllocator.h"
#include "Reference.h"
#include "AlignedCharArray.h"
#include "MemAlloc.h"
#include "../maths/mathstypes.h"
#include <assert.h>
#include <stddef.h>
#include <memory>


template <class T> class CircularBufferIterator;


/*=====================================================================
CircularBuffer
--------------
A dynamically-growable circular buffer.
The interface should be similar to std::list


non-wrapped case (begin <= end), size()=7:

 data[0]                                           data[data_size-1]
  v                                                   v
|   |   |---|---|---|---|---|---|---|   |   |   |   |   |
          ^                           ^
        begin                        end

Wrapped case (end < begin):

|---|---|---|---|   |   |   |   |   |   |   |---|---|---|
                  ^                           ^
                 end                         begin

case with size()=0:

|   |   |   |   |   |   |   |   |   |   |   |   |   |   |
                  ^                           
                begin, end


If we were to have size() == data_size, then end would wrap to begin, so we wouldn't be able to distinguish from the size()=0 case.
Therefore, we will always have data_size > size(), except for when data_size = size() = 0.
=====================================================================*/
template <class T>
class CircularBuffer
{
public:
	inline CircularBuffer();
	inline ~CircularBuffer();

	inline void operator = (const CircularBuffer<T>& other);

	inline void push_back(const T& t);
	
	inline void push_front(const T& t);

	// Undefined if empty.
	inline void pop_back();
	
	// Undefined if empty.
	inline void pop_front();

	inline T& front();

	inline T& back();

	//inline T& operator [] (const size_t offset); // Returns a reference to the item at the given offset from the front (*beginIt()).

	inline void clear();
	inline void clearAndFreeMem();

	inline size_t size() const;

	inline bool empty() const { return begin == end; }

	inline bool nonEmpty() const { return begin != end; }

	inline void popFrontNItems(T* dest, size_t N); // Copies N items to dest.  N must be <= size()

	// Remove items from front without copying them to a buffer.
	inline void popFrontNItems(size_t N); // N must be <= size()

	inline void pushBackNItems(const T* src, size_t N);

	inline size_t getFirstSegmentSize() const { return (begin <= end) ? (end - begin) : (data_size - begin); } // Number of items in segment from begin to either end or where it wraps (data_size).

	typedef CircularBufferIterator<T> iterator;


	inline iterator beginIt() { return CircularBufferIterator<T>(this, begin); }
	inline iterator endIt() { return CircularBufferIterator<T>(this, end); }


	friend void circularBufferTest();
	friend class CircularBufferIterator<T>;

	void setAllocator(const Reference<glare::Allocator>& al) { allocator = al; }
	Reference<glare::Allocator>& getAllocator() { return allocator; }

private:
	CircularBuffer(const CircularBuffer& other);
	void destroyAndFreeData();
	inline void increaseDataSize(size_t requested_new_data_size);
	inline void invariant();
	inline T* alloc(size_t size, size_t alignment);
	inline void free(T* ptr);
	

	size_t begin; // Index of first item in buffer, wrapped to [0, data_size), unless data_size = 0.
	size_t end; // Index one past last item in buffer, wrapped to [0, data_size), unless data_size = 0.
	T* data; // Storage
	size_t data_size; // Size/capacity in number of elements of data.
	Reference<glare::Allocator> allocator;
};


void circularBufferTest();


template <class T>
class CircularBufferIterator
{
public:
	CircularBufferIterator(CircularBuffer<T>* buffer, size_t i_) : buffer(buffer), i(i_) {}
	~CircularBufferIterator(){}

	bool operator == (const CircularBufferIterator& other) const
	{
		return i == other.i;
	}

	bool operator != (const CircularBufferIterator& other) const
	{
		return i != other.i;
	}

	void operator ++ ()
	{
		i++;
		if(i >= buffer->data_size)
			i = 0;
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
};


template <class T>
CircularBuffer<T>::CircularBuffer()
:	begin(0), end(0), data(0), data_size(0)
{
	invariant(); 
}


template <class T>
CircularBuffer<T>::~CircularBuffer()
{
	invariant();

	destroyAndFreeData();
}


template <class T>
void CircularBuffer<T>::destroyAndFreeData()
{
	if(data)
	{
		// Destroy first segment.
		const bool wrapped = end < begin;
		const size_t first_segment_end = wrapped ? data_size : end;
		for(size_t i=begin; i<first_segment_end; ++i)
			data[i].~T();
		// Destroy wrapped-around segment, if any.
		if(wrapped)
			for(size_t i=0; i<end; ++i)
				data[i].~T();

		free(data);
	}
}


template <class T>
void CircularBuffer<T>::operator = (const CircularBuffer<T>& other)
{
	invariant();

	destroyAndFreeData();

	const size_t other_size = other.size();
	begin = 0;
	end = other_size;
	data_size = myMax<size_t>(4, other_size + 1);
	data = alloc(sizeof(T) * data_size, glare::AlignOf<T>::Alignment);

	size_t other_i = other.begin;
	for(size_t i=0; i<other_size; ++i)
	{
		::new (data + i) T(other.data[other_i]); // Construct data[i] from other.data[other_i]
		other_i++;
		if(other_i >= other.data_size)
			other_i = 0;
	}

	allocator = other.allocator;

	invariant();
}


template <class T>
void CircularBuffer<T>::push_back(const T& t)
{
	invariant();

	// If there is no free space
	if(size() + 1 >= data_size)
		increaseDataSize(myMax(size() + 2, data_size * 2)); // new data size should be > size + 1 (as we are adding an item below).

	// Construct data[end] from t
	::new (data + end) T(t);
	end++;

	// Wrap end
	if(end >= data_size)
		end = 0;

	invariant();
}


template <class T>
void CircularBuffer<T>::push_front(const T& t)
{
	invariant();

	// If there is no free space
	if(size() + 1 >= data_size)
		increaseDataSize(myMax(size() + 2, data_size * 2)); // new data size should be > size + 1 (as we are adding an item below).

	// Decrement and wrap begin
	if(begin == 0)
		begin = data_size - 1;
	else
		begin--;

	// Construct data[begin] from t
	::new ((data + begin)) T(t);

	invariant();
}


template <class T>
void CircularBuffer<T>::pop_back()
{
	invariant();
	assert(size() > 0);

	// Decrement and wrap end
	if(end == 0)
		end = data_size - 1;
	else
		end--;

	// Destroy object
	data[end].~T();

	invariant();
}


template <class T>
void CircularBuffer<T>::pop_front()
{
	invariant();
	assert(size() > 0);

	// Destroy object
	data[begin].~T();

	// Increment and wrap begin
	if(begin == data_size - 1)
		begin = 0;
	else 
		begin++;

	invariant();
}


template <class T>
void CircularBuffer<T>::clear()
{
	invariant();

	// Destroy objects

	// Destroy first segment.
	const bool wrapped = end < begin;
	const size_t first_segment_end = wrapped ? data_size : end;
	for(size_t i=begin; i<first_segment_end; ++i)
		data[i].~T();

	// Destroy wrapped-around segment, if any.
	if(wrapped)
		for(size_t i=0; i<end; ++i)
			data[i].~T();
	
	begin = 0;
	end = 0;

	invariant();
}


template <class T>
void CircularBuffer<T>::clearAndFreeMem()
{
	invariant();

	clear();

	free(data);
	data = 0;
	data_size = 0;

	invariant();
}


template <class T>
T& CircularBuffer<T>::front()
{
	invariant();
	assert(size() > 0);

	return data[begin];
}


template <class T>
T& CircularBuffer<T>::back()
{
	invariant();
	assert(size() > 0);

	if(end == 0)
		return data[data_size - 1];
	else
		return data[end - 1];
}


//template <class T>
//T& CircularBuffer<T>::operator [] (const size_t offset)
//{
//	size_t i = begin + offset;
//	if(i >= data_size)
//		i -= data_size;
//	return data[i];
//}


/*
Unwrapped case:
|   |   |---|---|---|---|---|---|---|   |   |   |   |   |
          ^                           ^
        begin                        end

Wrapped case:
|---|---|---|---|   |   |   |   |   |   |   |---|---|---|
                  ^                           ^
                 end                         begin
*/
template <class T>
size_t CircularBuffer<T>::size() const { return (begin <= end) ? (end - begin) : (data_size - begin + end); }


template <class T>
void CircularBuffer<T>::invariant()
{
#ifndef NDEBUG // If in debug mode:
	if(data_size == 0)
	{
		assert(begin == 0 && end == 0);
	}
	else
	{
		assert(data_size > size());
		assert(begin < data_size); // begin should never be == data_size, it should wrap to 0.
		assert(end   < data_size); // end should never be == data_size, it should wrap to 0.
		assert(data);
	}
#endif
}


// Resize buffer.  Need to make it least twice as big so that we can copy all the wrapped elements directly.
template <class T>
void CircularBuffer<T>::increaseDataSize(size_t requested_new_data_size)
{
	assert(requested_new_data_size >= data_size * 2); // We should always be at least doubling the data_size, so we can unwrap data when copying.

	const size_t cur_size = size();
	const size_t new_data_size = myMax<size_t>(4, requested_new_data_size);
	T* new_data = alloc(sizeof(T) * new_data_size, glare::AlignOf<T>::Alignment);

	// Copy-construct new objects from existing objects:
	
	const bool wrapped = end < begin;
	const size_t first_segment_end = wrapped ? data_size : end;

	// Copy data [begin, first_segment_end) to new_data [begin, first_segment_end)
	std::uninitialized_copy(/*src first=*/data + begin, /*src last=*/data + first_segment_end, /*dest first=*/new_data + begin);

	size_t new_end = end;
	if(wrapped) // If the data was wrapping before the resize:
	{
		assert(new_data_size - data_size >= end); // We should have enough space in new_data to copy the wrapped segment [0, end).

		// Copy from data [0, end) to new_data [data_size, data_size + end)
		std::uninitialized_copy(/*src first=*/data, /*src last=*/data + end, /*dest first=*/new_data + data_size);

		// Update end since data is no longer wrapping.
		new_end = begin + cur_size;
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

		free(data);
	}
	data = new_data;
	end = new_end;
	data_size = new_data_size;

	assert(end <= data_size);
}


template <class T>
T* CircularBuffer<T>::alloc(size_t size, size_t alignment)
{
	if(allocator.nonNull())
		return (T*)allocator->alloc(size, alignment);
	else
		return (T*)MemAlloc::alignedMalloc(size, alignment);
}


template <class T>
void CircularBuffer<T>::free(T* ptr)
{
	if(allocator.nonNull())
		allocator->free(ptr);
	else
		MemAlloc::alignedFree(ptr);
}


template <class T>
void CircularBuffer<T>::popFrontNItems(T* dest, size_t N) // N must be <= size()
{
	assert(N <= size());
	const size_t first_range_size = myMin(N, data_size - begin);
	for(size_t i=0; i<first_range_size; ++i)
	{
		dest[i] = data[begin + i];
		data[begin + i].~T(); // Destroy object
	}

	size_t new_begin = begin + first_range_size;

	if(N > data_size - begin) // If part of the data being popped wraps around:
	{
		const size_t second_range_size = N - (data_size - begin);
		for(size_t i=0; i<second_range_size; ++i)
		{
			dest[first_range_size + i] = data[i];
			data[i].~T(); // Destroy object
		}

		new_begin = second_range_size;
	}

	begin = new_begin;
	if(begin == data_size)
		begin = 0; // Wrap begin if needed.

	invariant();
}


template <class T>
void CircularBuffer<T>::popFrontNItems(size_t N) // N must be <= size()
{
	assert(N <= size());
	const size_t first_range_size = myMin(N, data_size - begin);
	for(size_t i=0; i<first_range_size; ++i)
	{
		data[begin + i].~T(); // Destroy object
	}

	size_t new_begin = begin + first_range_size;

	if(N > data_size - begin) // If part of the data being popped wraps around:
	{
		const size_t second_range_size = N - (data_size - begin);
		for(size_t i=0; i<second_range_size; ++i)
		{
			data[i].~T(); // Destroy object
		}

		new_begin = second_range_size;
	}

	begin = new_begin;
	if(begin == data_size)
		begin = 0; // Wrap begin if needed.

	invariant();
}


template <class T>
void CircularBuffer<T>::pushBackNItems(const T* src, size_t N)
{
	if(size() + N >= data_size)
		increaseDataSize(myMax(size() + N + 1, data_size * 2));

	const size_t end_capacity = data_size - end;
	const size_t first_range_size = myMin(N, end_capacity);
	for(size_t i=0; i<first_range_size; ++i)
	{
		// Construct data[end + i] from src[i]
		::new (data + end + i) T(src[i]);
	}

	size_t new_end = end + first_range_size;

	if(N > end_capacity) // If part of the data being pushed wraps around:
	{
		const size_t second_range_size = N - end_capacity;
		for(size_t i=0; i<second_range_size; ++i)
		{
			// Construct data[i] from src[first_range_size + i];
			::new (data + i) T(src[first_range_size + i]);
		}

		new_end = second_range_size;
	}

	end = new_end;
	if(end == data_size)
		end = 0; // Wrap end if needed.

	invariant();
}
