/*=====================================================================
Array.h
-------
Copyright Glare Technologies Limited 2024 -
=====================================================================*/
#pragma once


#include "MemAlloc.h"
#include <assert.h>
#include <memory>
#include <new>


namespace glare
{


/*=====================================================================
Array
-----
Fixed-length, heap allocated array.
Like Vector, but not optimised for changing length at runtime.
The main difference is it doesn't have a capacity field.
=====================================================================*/
template <class T>
class Array
{
public:
	inline Array(); // Initialise as an empty vector.
	inline Array(size_t count, const T& val = T()); // Initialise with count copies of val.
	inline Array(const T* begin, const T* end); // Range constructor
	inline Array(const Array& other); // Initialise as a copy of other
	inline ~Array();

	inline Array& operator=(const Array& other);

	inline void resizeNoCopy(size_t new_size); // Resize to size N, using default constructor.

	inline size_t size() const;
	inline bool empty() const;
	inline bool nonEmpty() const;

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
private:
	T* e;
	size_t size_; // Number of elements in the vector.  Elements e[0] to e[size_-1] are proper constructed objects.
};


void testArray();


template <class T>
Array<T>::Array()
:	e(nullptr), size_(0)
{}


template <class T>
Array<T>::Array(size_t count, const T& val)
:	size_(count)
{
	e = static_cast<T*>(MemAlloc::alignedSSEMalloc(sizeof(T) * count)); // Allocate new memory on heap

	std::uninitialized_fill(e, e + count, val); // Construct elems
}


template <class T>
Array<T>::Array(const T* begin_, const T* end_) // Range constructor
:	size_(end_ - begin_)
{
	e = static_cast<T*>(MemAlloc::alignedSSEMalloc(sizeof(T) * size_)); // Allocate new memory on heap

	std::uninitialized_copy(begin_, end_, /*dest=*/e);
}


template <class T>
Array<T>::Array(const Array<T>& other)
{
	e = static_cast<T*>(MemAlloc::alignedSSEMalloc(sizeof(T) * other.size_)); // Allocate new memory on heap

	// Copy-construct new objects from existing objects in 'other'.
	std::uninitialized_copy(other.e, other.e + other.size_, /*dest=*/e);
	size_ = other.size_;
}


template <class T>
Array<T>::~Array()
{
	for(size_t i=0; i<size_; ++i)
		e[i].~T();

	MemAlloc::alignedFree(e);
}


template <class T>
Array<T>& Array<T>::operator=(const Array& other)
{
	if(this == &other)
		return *this;

	// Destroy existing objects
	for(size_t i=0; i<size_; ++i)
		e[i].~T();

	MemAlloc::alignedFree(e); // Free existing mem

	// Allocate new memory
	e = static_cast<T*>(MemAlloc::alignedSSEMalloc(sizeof(T) * other.size_));

	// Copy elements over from other
	std::uninitialized_copy(other.e, other.e + other.size_, e);

	size_ = other.size_;

	return *this;
}


template <class T>
void Array<T>::resizeNoCopy(size_t new_size)
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
		MemAlloc::alignedFree(e); // Free old buffer.

	e = new_e;
	size_ = new_size;
}


template <class T>
size_t Array<T>::size() const
{
	return size_;
}


template <class T>
bool Array<T>::empty() const
{
	return size_ == 0;
}


template<class T>
inline bool Array<T>::nonEmpty() const
{
	return size_ != 0;
}


template <class T>
const T& Array<T>::back() const
{
	assert(size_ >= 1);
	return e[size_ - 1];
}


template <class T>
T& Array<T>::back()
{
	assert(size_ >= 1);
	return e[size_ - 1];
}


template <class T>
T& Array<T>::operator[](size_t index)
{
	assert(index < size_);
	return e[index];
}


template <class T>
const T& Array<T>::operator[](size_t index) const
{
	assert(index < size_);
	return e[index];
}


template <class T>
typename Array<T>::iterator Array<T>::begin()
{
	return e;
}


template <class T>
typename Array<T>::iterator Array<T>::end()
{
	return e + size_;
}


template <class T>
typename Array<T>::const_iterator Array<T>::begin() const
{
	return e;
}


template <class T>
typename Array<T>::const_iterator Array<T>::end() const
{
	return e + size_;
}


} // end namespace glare
