/*=====================================================================
SharedImmutableArray.h
----------------------
Copyright Glare Technologies Limited 2025 -
=====================================================================*/
#pragma once


#include "MemAlloc.h"
#include "ThreadSafeRefCounted.h"
#include "Reference.h"
#include <assert.h>
#include <memory>
#include <new>


namespace glare
{


/*=====================================================================
SharedImmutableArray
--------------------
Fixed-length, heap allocated array.
Shouldn't be changed after creation and initialisation (immutable).
Designed to be shared for read-only access from different threads.

Based on Array.h
=====================================================================*/
template <class T>
class SharedImmutableArray : public ThreadSafeRefCounted
{
public:
	inline SharedImmutableArray(); // Initialise as an empty vector.
	inline SharedImmutableArray(size_t count, const T& val = T()); // Initialise with count copies of val.
	inline SharedImmutableArray(const T* begin, const T* end); // Range constructor
	inline SharedImmutableArray(const SharedImmutableArray& other); // Initialise as a copy of other
	inline ~SharedImmutableArray();

	inline SharedImmutableArray& operator=(const SharedImmutableArray& other);

	inline void resizeNoCopy(size_t new_size); // Resize to size N, using default constructor.

	inline size_t size() const;
	inline size_t dataSizeBytes() const;
	inline bool empty() const;
	inline bool nonEmpty() const;

	inline bool operator == (const SharedImmutableArray& other); 

	inline T* data() { return e; }
	inline const T* data() const { return e; }

	inline const T& back() const;

	inline T& operator[](size_t index);
	inline const T& operator[](size_t index) const;

	typedef const T* const_iterator;

	inline const_iterator begin() const;
	inline const_iterator end() const;
private:
	T* e;
	size_t size_; // Number of elements in the vector.  Elements e[0] to e[size_-1] are proper constructed objects.
};


// void testSharedImmutableArray();


template <class T>
SharedImmutableArray<T>::SharedImmutableArray()
:	e(nullptr), size_(0)
{}


template <class T>
SharedImmutableArray<T>::SharedImmutableArray(size_t count, const T& val)
:	size_(count)
{
	e = static_cast<T*>(MemAlloc::alignedSSEMalloc(sizeof(T) * count)); // Allocate new memory on heap

	std::uninitialized_fill(e, e + count, val); // Construct elems
}


template <class T>
SharedImmutableArray<T>::SharedImmutableArray(const T* begin_, const T* end_) // Range constructor
:	size_(end_ - begin_)
{
	e = static_cast<T*>(MemAlloc::alignedSSEMalloc(sizeof(T) * size_)); // Allocate new memory on heap

	std::uninitialized_copy(begin_, end_, /*dest=*/e);
}


template <class T>
SharedImmutableArray<T>::SharedImmutableArray(const SharedImmutableArray<T>& other)
{
	e = static_cast<T*>(MemAlloc::alignedSSEMalloc(sizeof(T) * other.size_)); // Allocate new memory on heap

	// Copy-construct new objects from existing objects in 'other'.
	std::uninitialized_copy(other.e, other.e + other.size_, /*dest=*/e);
	size_ = other.size_;
}


template <class T>
SharedImmutableArray<T>::~SharedImmutableArray()
{
	for(size_t i=0; i<size_; ++i)
		e[i].~T();

	MemAlloc::alignedSSEFree(e);
}


template <class T>
SharedImmutableArray<T>& SharedImmutableArray<T>::operator=(const SharedImmutableArray& other)
{
	if(this == &other)
		return *this;

	// Destroy existing objects
	for(size_t i=0; i<size_; ++i)
		e[i].~T();

	MemAlloc::alignedSSEFree(e); // Free existing mem

	// Allocate new memory
	e = static_cast<T*>(MemAlloc::alignedSSEMalloc(sizeof(T) * other.size_));

	// Copy elements over from other
	std::uninitialized_copy(other.e, other.e + other.size_, e);

	size_ = other.size_;

	return *this;
}


template <class T>
void SharedImmutableArray<T>::resizeNoCopy(size_t new_size)
{
	// Allocate new memory
	T* new_e = static_cast<T*>(MemAlloc::alignedSSEMalloc(sizeof(T) * new_size));

	// Initialise elements
	// NOTE: We use the constructor form without parentheses, in order to avoid default (zero) initialisation of POD types. 
	// See http://stackoverflow.com/questions/620137/do-the-parentheses-after-the-type-name-make-a-difference-with-new for more info.
	for(T* elem=new_e; elem<new_e + new_size; ++elem)
		::new (elem) T;

	// Destroy old objects
	for(size_t i=0; i<size_; ++i)
		e[i].~T();

	if(e)
		MemAlloc::alignedSSEFree(e); // Free old buffer.

	e = new_e;
	size_ = new_size;
}


template <class T>
size_t SharedImmutableArray<T>::size() const
{
	return size_;
}


template <class T>
size_t SharedImmutableArray<T>::dataSizeBytes() const
{
	return size_ * sizeof(T);
}


template <class T>
bool SharedImmutableArray<T>::empty() const
{
	return size_ == 0;
}


template<class T>
inline bool SharedImmutableArray<T>::nonEmpty() const
{
	return size_ != 0;
}


template<class T>
inline bool SharedImmutableArray<T>::operator==(const SharedImmutableArray& other)
{
	if(size_ != other.size_)
		return false;

	for(size_t i=0; i<size_; ++i)
		if(e[i] != other[i])
			return false;
	
	return true;
}


template <class T>
const T& SharedImmutableArray<T>::back() const
{
	assert(size_ >= 1);
	return e[size_ - 1];
}


template <class T>
T& SharedImmutableArray<T>::operator[](size_t index)
{
	assert(index < size_);
	return e[index];
}


template <class T>
const T& SharedImmutableArray<T>::operator[](size_t index) const
{
	assert(index < size_);
	return e[index];
}

 
template <class T>
typename SharedImmutableArray<T>::const_iterator SharedImmutableArray<T>::begin() const
{
	return e;
}


template <class T>
typename SharedImmutableArray<T>::const_iterator SharedImmutableArray<T>::end() const
{
	return e + size_;
}


} // end namespace glare
