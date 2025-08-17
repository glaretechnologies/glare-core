/*=====================================================================
SmallArray.h
------------
Copyright Glare Technologies Limited 2021 -
=====================================================================*/
#pragma once


#include "AlignedCharArray.h"
#include "MemAlloc.h"
#include "../maths/mathstypes.h"
#include <assert.h>
#include <memory>
#include <new>


/*=====================================================================
SmallArray
----------
Like SmallVector, but not optimised for changing length at runtime.
The main difference is it doesn't have a capacity field.
if the number of elements stored is small (<= N), then store them directly in the object
as opposed to somewhere else in the heap.
=====================================================================*/
template <class T, int N>
class SmallArray
{
public:
	inline SmallArray(); // Initialise as an empty vector.
	//inline SmallArray(const ArrayRef<T> array_ref);
	inline SmallArray(size_t count, const T& val = T()); // Initialise with count copies of val.
	inline SmallArray(const T* begin, const T* end); // Range constructor
	inline SmallArray(const SmallArray& other); // Initialise as a copy of other
	inline ~SmallArray();

	inline SmallArray& operator=(const SmallArray& other);

	inline void resize(size_t new_size); // Resize to size N, using default constructor if N > size().
	inline void resize(size_t new_size, const T& val); // Resize to size new_size, using copies of val if new_size > size().

	inline bool operator == (const SmallArray& other) const; 

	inline size_t size() const;
	inline bool empty() const;

	inline T* data() { return e; }
	inline const T* data() const { return e; }

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
	
	inline bool storingOnHeap() const { return e != reinterpret_cast<const T*>(direct.buf); }
private:
	inline void resizeSmaller(size_t new_size);
	T* e;
	size_t size_; // Number of elements in the vector.  Elements e[0] to e[size_-1] are proper constructed objects.
	// We can't just use an array of T here as then the T objects will need to be constructed.  Instead we just want space for N T objects.
	glare::AlignedCharArray<glare::AlignOf<T>::Alignment, sizeof(T) * N> direct;
};


namespace SmallArrayTest
{
void test();
}


template <class T, int N>
SmallArray<T, N>::SmallArray()
:	e(reinterpret_cast<T*>(direct.buf)), size_(0)
{}

//
//template <class T, int N>
//SmallArray<T, N>::SmallArray(const ArrayRef<T> array_ref)
//:	size_(array_ref.size())
//{
//	if(count <= N)
//		e = reinterpret_cast<T*>(direct); // Store directly
//	else
//		e = static_cast<T*>(MemAlloc::alignedSSEMalloc(sizeof(T) * count)); // Allocate new memory on heap
//
//	std::uninitialized_copy(array_ref.data_, array_ref.data_ + other.size_, /*dest=*/e);
//}


template <class T, int N>
SmallArray<T, N>::SmallArray(size_t count, const T& val)
:	size_(count)
{
	if(count <= N)
		e = reinterpret_cast<T*>(direct.buf); // Store directly
	else
		e = static_cast<T*>(MemAlloc::alignedSSEMalloc(sizeof(T) * count)); // Allocate new memory on heap

	std::uninitialized_fill(e, e + count, val); // Construct elems
}


template <class T, int N>
SmallArray<T, N>::SmallArray(const T* begin_, const T* end_) // Range constructor
:	size_(end_ - begin_)
{
	if(size_ <= N)
		e = reinterpret_cast<T*>(direct.buf); // Store directly
	else
		e = static_cast<T*>(MemAlloc::alignedSSEMalloc(sizeof(T) * size_)); // Allocate new memory on heap

	std::uninitialized_copy(begin_, end_, /*dest=*/e);
}


template <class T, int N>
SmallArray<T, N>::SmallArray(const SmallArray<T, N>& other)
{
	if(other.size_ <= N)
		e = reinterpret_cast<T*>(direct.buf); // Store directly
	else
		e = static_cast<T*>(MemAlloc::alignedSSEMalloc(sizeof(T) * other.size_)); // Allocate new memory on heap

	// Copy-construct new objects from existing objects in 'other'.
	std::uninitialized_copy(other.e, other.e + other.size_, /*dest=*/e);
	size_ = other.size_;
}


template <class T, int N>
SmallArray<T, N>::~SmallArray()
{
	for(size_t i=0; i<size_; ++i)
		e[i].~T();

	if(storingOnHeap())
		MemAlloc::alignedFree(e);
}


template <class T, int N>
SmallArray<T, N>& SmallArray<T, N>::operator=(const SmallArray& other)
{
	if(this == &other)
		return *this;

	// Destroy existing objects
	for(size_t i=0; i<size_; ++i)
		e[i].~T();

	if(other.size_ <= N)
	{
		if(storingOnHeap())
			MemAlloc::alignedFree(e); // Free existing mem

		e = reinterpret_cast<T*>(direct.buf); // Store directly

		// Copy elements over from other
		if(other.size_ > 0) 
			std::uninitialized_copy(other.e, other.e + other.size_, e);
	}
	else // Else we need to store new elements on the heap):
	{
		assert(other.size_ > N);

		if(storingOnHeap())
			MemAlloc::alignedFree(e); // Free existing mem

		// Allocate new memory
		e = static_cast<T*>(MemAlloc::alignedSSEMalloc(sizeof(T) * other.size_));

		// Copy elements over from other
		std::uninitialized_copy(other.e, other.e + other.size_, e);
	}

	size_ = other.size_;

	return *this;
}


template <class T, int N>
void SmallArray<T, N>::resizeSmaller(size_t new_size)
{
	assert(new_size < size_);

	if(new_size <= N)
	{
		if(storingOnHeap()) // If we are currently storing on heap:
		{
			T* new_e = reinterpret_cast<T*>(direct.buf);

			// Copy-construct new objects from existing objects.
			// e[0] to e[new_size-1] will now be proper initialised objects.
			std::uninitialized_copy(e, e + new_size, new_e);

			// Destroy old objects in heap buffer.
			for(size_t i=0; i<size_; ++i)
				e[i].~T();

			MemAlloc::alignedFree(e); // Free old buffer.
			e = new_e;
		}
		else
		{
			// Destroy elements e[new_size] to e[size-1]
			for(size_t i=new_size; i<size_; ++i)
				(e + i)->~T();
		}
	}
	else // else new_size > N (so can't store directly, must use heap), but also new_size <= size_, so we are already using heap.
	{
		assert(storingOnHeap());

		// Destroy elements e[new_size] to e[size-1]
		for(size_t i=new_size; i<size_; ++i)
			(e + i)->~T();
	}

	size_ = new_size;
}


template <class T, int N>
void SmallArray<T, N>::resize(size_t new_size)
{
	if(new_size <= size_)
	{
		if(new_size < size_)
			resizeSmaller(new_size);
	}
	else // Else if new_size > size_:
	{
		if(new_size <= N) // If we can fit in small non-heap storage:
		{ 
			assert(!storingOnHeap()); // size_ < new_size <= N, so size_ < N.

			// Initialise elements e[size_] to e[new_size-1]
			// NOTE: We use the constructor form without parentheses, in order to avoid default (zero) initialisation of POD types. 
			// See http://stackoverflow.com/questions/620137/do-the-parentheses-after-the-type-name-make-a-difference-with-new for more info.
			for(T* elem=e + size_; elem<e + new_size; ++elem)
				::new (elem) T;
		}
		else // else new_size > N, so we need to allocate heap storage:
		{
			// Allocate new memory
			T* new_e = static_cast<T*>(MemAlloc::alignedSSEMalloc(sizeof(T) * new_size));

			// Copy-construct new objects from existing objects.
			// new_e[0] to new_e[size_-1] will now be proper initialised objects.
			std::uninitialized_copy(e, e + size_, /*dest=*/new_e);

			// Initialise elements new_e[size_] to new_e[new_size-1]
			// NOTE: We use the constructor form without parentheses, in order to avoid default (zero) initialisation of POD types. 
			// See http://stackoverflow.com/questions/620137/do-the-parentheses-after-the-type-name-make-a-difference-with-new for more info.
			for(T* elem=new_e + size_; elem<new_e + new_size; ++elem)
				::new (elem) T;

			// Destroy old objects
			for(size_t i=0; i<size_; ++i)
				e[i].~T();

			if(storingOnHeap())
				MemAlloc::alignedFree(e); // Free old buffer.

			e = new_e;
		}

		size_ = new_size;
	}
}


template <class T, int N>
void SmallArray<T, N>::resize(size_t new_size, const T& val)
{
	if(new_size <= size_)
	{
		if(new_size < size_)
			resizeSmaller(new_size);
	}
	else // Else if new_size > size_:
	{
		if(new_size <= N) // If we can fit in small non-heap storage:
		{
			assert(!storingOnHeap()); // size_ < new_size <= N, so size_ < N.

			// Initialise elements e[size_] to e[new_size-1]
			std::uninitialized_fill(e + size_, e + new_size, val);
		}
		else
		{
			// Allocate new memory
			T* new_e = static_cast<T*>(MemAlloc::alignedSSEMalloc(sizeof(T) * new_size));

			// Copy-construct new objects from existing objects.
			// new_e[0] to new_e[size_-1] will now be proper initialised objects.
			std::uninitialized_copy(e, e + size_, /*dest=*/new_e);

			// Construct any new elems
			assert(new_size > size_);
			std::uninitialized_fill(new_e + size_, new_e + new_size, val);
		
			// Destroy old objects
			for(size_t i=0; i<size_; ++i)
				e[i].~T();

			if(storingOnHeap())
				MemAlloc::alignedFree(e); // Free old buffer.

			e = new_e;
		}

		size_ = new_size;
	}
}


template<class T, int N>
inline bool SmallArray<T, N>::operator==(const SmallArray& other) const
{
	if(size_ != other.size_)
		return false;

	for(size_t i=0; i<size_; ++i)
		if((*this)[i] != other[i])
			return false;
	
	return true;
}


template <class T, int N>
size_t SmallArray<T, N>::size() const
{
	return size_;
}


template <class T, int N>
bool SmallArray<T, N>::empty() const
{
	return size_ == 0;
}


template <class T, int N>
const T& SmallArray<T, N>::back() const
{
	assert(size_ >= 1);
	return e[size_ - 1];
}


template <class T, int N>
T& SmallArray<T, N>::back()
{
	assert(size_ >= 1);
	return e[size_ - 1];
}


template <class T, int N>
T& SmallArray<T, N>::operator[](size_t index)
{
	assert(index < size_);
	return e[index];
}


template <class T, int N>
const T& SmallArray<T, N>::operator[](size_t index) const
{
	assert(index < size_);
	return e[index];
}


template <class T, int N>
typename SmallArray<T, N>::iterator SmallArray<T, N>::begin()
{
	return e;
}


template <class T, int N>
typename SmallArray<T, N>::iterator SmallArray<T, N>::end()
{
	return e + size_;
}


template <class T, int N>
typename SmallArray<T, N>::const_iterator SmallArray<T, N>::begin() const
{
	return e;
}


template <class T, int N>
typename SmallArray<T, N>::const_iterator SmallArray<T, N>::end() const
{
	return e + size_;
}
