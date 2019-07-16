/*=====================================================================
SmallVector.h
-------------------
Copyright Glare Technologies Limited 2016 -
Generated at 2013-03-07 14:27:12 +0000
=====================================================================*/
#pragma once


#include "AlignedCharArray.h"
#include "../maths/SSE.h"
#include "../maths/mathstypes.h"
#include <assert.h>
#include <memory>
#include <new>


/*=====================================================================
SmallVector
-------------------
Like js::Vector but does the small vector optimisation - 
if the number of elements stored is small (<= N), then store them directly in the object
as opposed to somewhere else in the heap.
=====================================================================*/
template <class T, int N>
class SmallVector
{
public:
	inline SmallVector(); // Initialise as an empty vector.
	inline SmallVector(size_t count, const T& val = T()); // Initialise with count copies of val.
	inline SmallVector(const SmallVector& other); // Initialise as a copy of other
	inline ~SmallVector();

	inline SmallVector& operator=(const SmallVector& other);

	inline void reserve(size_t M); // Make sure capacity is at least M.
	inline void resize(size_t new_size); // Resize to size N, using default constructor if N > size().
	inline void resize(size_t new_size, const T& val); // Resize to size new_size, using copies of val if new_size > size().
	inline size_t capacity() const;
	inline size_t size() const;
	inline bool empty() const;

	inline T* data() { return e; }
	inline const T* data() const { return e; }

	inline void push_back(const T& t);
	inline void pop_back();
	inline const T& back() const;
	inline T& back();
	inline T& operator[](size_t index);
	inline const T& operator[](size_t index) const;

	typedef T* iterator;
	typedef const T* const_iterator;

	inline iterator begin();
	inline iterator end();
	inline const_iterator begin() const;
	inline const_iterator end() const;
	
private:
	inline bool storingOnHeap() const { return capacity_ > N; }

	T* e;
	size_t size_; // Number of elements in the vector.  Elements e[0] to e[size_-1] are proper constructed objects.
	size_t capacity_;
	// We can't just use an array of T here as then the T objects will need to be constructed.  Instead we just want space for N T objects.
	glare::AlignedCharArray<glare::AlignOf<T>::Alignment, sizeof(T) * N> direct;
};


namespace SmallVectorTest
{
void test();
}


template <class T, int N>
SmallVector<T, N>::SmallVector()
:	size_(0), e(reinterpret_cast<T*>(direct.buf)), capacity_(N)
{}


template <class T, int N>
SmallVector<T, N>::SmallVector(size_t count, const T& val)
:	size_(count)
{
	if(count <= N)
	{
		e = reinterpret_cast<T*>(direct.buf); // Store directly
		capacity_ = N;
	}
	else
	{
		e = static_cast<T*>(SSE::alignedSSEMalloc(sizeof(T) * count)); // Allocate new memory on heap
		capacity_ = count;
	}
	std::uninitialized_fill(e, e + count, val); // Construct elems
}


template <class T, int N>
SmallVector<T, N>::SmallVector(const SmallVector<T, N>& other)
{
	if(other.size_ <= N)
	{
		e = reinterpret_cast<T*>(direct.buf); // Store directly
		capacity_ = N;
	}
	else
	{
		e = static_cast<T*>(SSE::alignedSSEMalloc(sizeof(T) * other.size_)); // Allocate new memory on heap
		capacity_ = other.size_;
	}

	// Copy-construct new objects from existing objects in 'other'.
	std::uninitialized_copy(other.e, other.e + other.size_, /*dest=*/e);
	size_ = other.size_;
}


template <class T, int N>
SmallVector<T, N>::~SmallVector()
{
	for(size_t i=0; i<size_; ++i)
		e[i].~T();

	if(storingOnHeap())
		SSE::alignedFree(e);
}


template <class T, int N>
SmallVector<T, N>& SmallVector<T, N>::operator=(const SmallVector& other)
{
	if(this == &other)
		return *this;

	// Destroy existing objects
	for(size_t i=0; i<size_; ++i)
		e[i].~T();

	if(other.size_ <= capacity_)
	{
		// Copy elements over from other
		if(other.size_ > 0) 
			std::uninitialized_copy(other.e, other.e + other.size_, e);

		size_ = other.size_;
	}
	else // Else we don't have the capacity to store (and will need to store new elements on the heap):
	{
		assert(other.size_ > N); // other.size must be > N, otherwise we would have had capacity for it (as capacity is always >= N).

		if(storingOnHeap())
			SSE::alignedFree(e); // Free existing mem

		// Allocate new memory
		e = static_cast<T*>(SSE::alignedSSEMalloc(sizeof(T) * other.size_));

		// Copy elements over from other
		std::uninitialized_copy(other.e, other.e + other.size_, e);

		size_ = other.size_;
		capacity_ = other.size_;
	}

	return *this;
}


template <class T, int N>
void SmallVector<T, N>::reserve(size_t n)
{
	if(n > capacity_) // If need to expand capacity
	{
		// Allocate new memory
		T* new_e = static_cast<T*>(SSE::alignedSSEMalloc(sizeof(T) * n));

		// Copy-construct new objects from existing objects.
		// e[0] to e[size_-1] will now be proper initialised objects.
		std::uninitialized_copy(e, e + size_, new_e);
		
		// Destroy old objects
		for(size_t i=0; i<size_; ++i)
			e[i].~T();

		if(storingOnHeap())
			SSE::alignedFree(e); // Free old buffer.

		e = new_e;
		capacity_ = n;
	}
}


template <class T, int N>
void SmallVector<T, N>::resize(size_t new_size)
{
	if(new_size <= capacity_)
	{
		// Destroy elements e[new_size] to e[size-1]
		for(size_t i=new_size; i<size_; ++i)
			(e + i)->~T();
	}
	else
	{
		// else new_size > capacity_.  Since capacity_ >= size_, therefore new_size > size_.  So we don't have to delete any objects.
		reserve(new_size);
	}

	// Initialise elements e[size_] to e[new_size-1]
	// NOTE: We use the constructor form without parentheses, in order to avoid default (zero) initialisation of POD types. 
	// See http://stackoverflow.com/questions/620137/do-the-parentheses-after-the-type-name-make-a-difference-with-new for more info.
	for(T* elem=e + size_; elem<e + new_size; ++elem)
		::new (elem) T;

	size_ = new_size;
}


template <class T, int N>
void SmallVector<T, N>::resize(size_t new_size, const T& val)
{
	if(new_size <= capacity_)
	{
		// Destroy elements e[new_size] to e[size-1] (if any)
		for(size_t i=new_size; i<size_; ++i)
			(e + i)->~T();
	}
	else
	{
		// else new_size > capacity_.  Since capacity_ >= size_, therefore new_size > size_.  So we don't have to delete any objects.
		reserve(new_size);
	}

	// Construct any new elems
	if(new_size > size_)
		std::uninitialized_fill(e + size_, e + new_size, val);

	size_ = new_size;
}


template <class T, int N>
size_t SmallVector<T, N>::capacity() const
{
	return capacity_;
}


template <class T, int N>
size_t SmallVector<T, N>::size() const
{
	return size_;
}


template <class T, int N>
bool SmallVector<T, N>::empty() const
{
	return size_ == 0;
}


template <class T, int N>
void SmallVector<T, N>::push_back(const T& t)
{
	resize(size_ + 1, t); // TODO: write explicit code here?
}


template <class T, int N>
void SmallVector<T, N>::pop_back()
{
	assert(size_ >= 1);

	(e + size_ - 1)->~T(); // Destroy last element
	size_--;
}


template <class T, int N>
const T& SmallVector<T, N>::back() const
{
	assert(size_ >= 1);
	return e[size_ - 1];
}


template <class T, int N>
T& SmallVector<T, N>::back()
{
	assert(size_ >= 1);
	return e[size_ - 1];
}


template <class T, int N>
T& SmallVector<T, N>::operator[](size_t index)
{
	assert(index < size_);
	return e[index];
}


template <class T, int N>
const T& SmallVector<T, N>::operator[](size_t index) const
{
	assert(index < size_);
	return e[index];
}


template <class T, int N>
typename SmallVector<T, N>::iterator SmallVector<T, N>::begin()
{
	return e;
}


template <class T, int N>
typename SmallVector<T, N>::iterator SmallVector<T, N>::end()
{
	return e + size_;
}


template <class T, int N>
typename SmallVector<T, N>::const_iterator SmallVector<T, N>::begin() const
{
	return e;
}


template <class T, int N>
typename SmallVector<T, N>::const_iterator SmallVector<T, N>::end() const
{
	return e + size_;
}
