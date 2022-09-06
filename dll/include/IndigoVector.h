/*=====================================================================
IndigoVector.h
--------------
Copyright Glare Technologies Limited 2022 -
=====================================================================*/
#pragma once


#include "IndigoAllocation.h"
#include <assert.h>
#include <stddef.h> // For size_t
#include <new>
#include <memory>


namespace Indigo
{


///
/// Vector
///
/// A resizable array class.
/// Similar to std::vector.
///
template <typename T>
class Vector
{
public:
	USE_INDIGO_SDK_ALLOCATOR

	inline Vector();
	explicit inline Vector(size_t count);
	inline Vector(size_t count, const T& val);
	inline Vector(const Vector& other);
	inline ~Vector();

	inline Vector& operator=(const Vector& other);

	/// Make sure capacity is at least N.
	inline void reserve(size_t N);
	inline void resize(size_t N);
	inline void resize(size_t N, const T& val);
	inline size_t capacity() const { return capacity_; }
	inline size_t size() const;
	inline bool empty() const;
	/// Set size to zero, but also frees actual array memory.
	inline void clearAndFreeMem();

	inline void clearAndSetCapacity(size_t N);

	inline void concatWith(const Vector& other);

	inline bool operator == (const Vector& other) const;
	inline bool operator != (const Vector& other) const;

	inline void push_back(const T& t);
	inline void pop_back();
	inline const T& back() const;
	inline T& back();
	inline T& operator[](size_t index);
	inline const T& operator[](size_t index) const;
	
	inline T* data() { return e; }
	inline const T* data() const { return e; }

	typedef T* iterator;
	typedef const T* const_iterator;

	inline iterator begin();
	inline iterator end();
	inline const_iterator begin() const;
	inline const_iterator end() const;

	void erase(size_t index);

private:

	static inline void copy(const T* const src, T* dst, size_t num);

	T* e;
	size_t size_;
	size_t capacity_;
};


void testIndigoVector();


template <typename T>
Vector<T>::Vector() : e(0), size_(0), capacity_(0) { }


template <typename T>
Vector<T>::Vector(size_t count)
:	e(0),
	size_(count),
	capacity_(count)
{
	if(count > 0)
	{
		// Allocate new memory
		e = (T*)indigoSDKAlloc(sizeof(T) * count);

		// Initialise elements
		for(T* elem=e; elem<e + count; ++elem)
			::new (elem) T();
	}
}


template <typename T>
Vector<T>::Vector(size_t count, const T& val)
:	e(0),
	size_(count),
	capacity_(count)
{
	if(count > 0)
	{
		e = (T*)indigoSDKAlloc(sizeof(T) * count);

		// Initialise elements as a copy of val.
		for(T* elem=e; elem<e + count; ++elem)
			::new (elem) T(val);
	}
}


template <typename T>
Vector<T>::Vector(const Vector<T>& other)
:	e(0),
	size_(0),
	capacity_(0)
{
	*this = other;
}


template <typename T>
Vector<T>::~Vector()
{
	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);

	// Destroy objects
	for(size_t i=0; i<size_; ++i)
		e[i].~T();

	indigoSDKFree(e);
}


template <typename T>
Vector<T>& Vector<T>::operator=(const Vector& other)
{
	assert(capacity_ >= size_);

	if(this == &other)
		return *this;

	if(capacity_ >= other.size_)
	{
		// Destroy existing objects
		for(size_t i=0; i<size_; ++i)
			e[i].~T();

		// Copy elements over from other
		if(e) 
			std::uninitialized_copy(other.e, other.e + other.size_, e);

		size_ = other.size_;
	}
	else
	{
		// Destroy existing objects
		for(size_t i=0; i<size_; ++i)
			e[i].~T();

		// Free existing mem
		indigoSDKFree(e);

		// Allocate new memory
		e = static_cast<T*>(indigoSDKAlloc(sizeof(T) * other.size_));

		// Copy elements over from other
		if(e)
			std::uninitialized_copy(other.e, other.e + other.size_, e);

		size_ = other.size_;
		capacity_ = other.size_;
	}

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);

	return *this;
}


template <typename T>
void Vector<T>::reserve(size_t n)
{
	assert(capacity_ >= size_);

	if(n > capacity_) // If need to expand capacity
	{
		// Allocate new memory
		T* new_e = static_cast<T*>(indigoSDKAlloc(sizeof(T) * n));

		// Copy-construct new objects from existing objects.
		// e[0] to e[size_-1] will now be proper initialised objects.
		std::uninitialized_copy(e, e + size_, new_e);
		
		if(e)
		{
			// Destroy old objects
			for(size_t i=0; i<size_; ++i)
				e[i].~T();

			indigoSDKFree(e); // Free old buffer.
		}
		e = new_e;
		capacity_ = n;
	}

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}


template <typename T>
void Vector<T>::resize(size_t n)
{
	assert(capacity_ >= size_);

	if(n > size_)
	{
		if(n > capacity_)
		{
			const size_t newcapacity = (n > (2 * capacity_)) ? n : (2 * capacity_); // max(n, 2 * capacity_);
			reserve(newcapacity);
		}

		// Initialise elements e[size_] to e[n-1]
		for(T* elem=e + size_; elem<e + n; ++elem)
		{
			::new (elem) T();
		}
	}
	else if(n < size_)
	{
		// Destroy elements e[n] to e[size-1]
		for(size_t i=n; i<size_; ++i)
			e[i].~T();
	}

	size_ = n;

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}


template <typename T>
void Vector<T>::resize(size_t n, const T& val)
{
	assert(capacity_ >= size_);

	if(n > size_)
	{
		if(n > capacity_)
		{
			const size_t newcapacity = (n > (2 * capacity_)) ? n : (2 * capacity_); // max(n, 2 * capacity_);
			reserve(newcapacity);
		}

		// Initialise elements e[size_] to e[n-1] as a copy of val.
		for(T* elem=e + size_; elem<e + n; ++elem)
		{
			::new (elem) T(val);
		}
	}
	else if(n < size_)
	{
		// Destroy elements e[n] to e[size-1]
		for(size_t i=n; i<size_; ++i)
			e[i].~T();
	}

	size_ = n;

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}


template <typename T>
size_t Vector<T>::size() const
{
	return size_;
}


template <typename T>
bool Vector<T>::empty() const
{
	return size_ == 0;
}


template <typename T>
void Vector<T>::clearAndFreeMem() // Set size to zero, but also frees actual array memory.
{
	// Destroy objects
	for(size_t i=0; i<size_; ++i)
		e[i].~T();

	indigoSDKFree(e);
	e = NULL;
	size_ = 0;
	capacity_ = 0;
}


template <typename T>
inline void Vector<T>::clearAndSetCapacity(size_t N)
{
	clearAndFreeMem();
	reserve(N);
}


template <typename T>
inline void Vector<T>::concatWith(const Vector& other)
{
	const size_t old_size = this->size();

	this->resize(this->size() + other.size());

	for(size_t i=0; i<other.size(); ++i)
		this->e[old_size + i] = other.e[i];
}


template <typename T>
bool Vector<T>::operator == (const Vector<T>& other) const
{
	if(size() != other.size())
		return false;

	for(size_t i=0; i<size(); ++i)
		if(e[i] != other[i])
			return false;

	return true;
}


template <typename T>
bool Vector<T>::operator != (const Vector<T>& other) const
{
	if(size() != other.size())
		return true;

	for(size_t i=0; i<size(); ++i)
		if(e[i] != other[i])
			return true;

	return false;
}


template <typename T>
void Vector<T>::push_back(const T& t)
{
	assert(capacity_ >= size_);

	// Check to see if we are out of capacity:
	if(size_ == capacity_)
	{
		const size_t larger_capacity = 2*capacity_;
		const size_t newcapacity = ((size_ + 1) > larger_capacity) ? (size_ + 1) : larger_capacity;
		reserve(newcapacity);
	}

	// Construct e[size_] from t
	::new ((e + size_)) T(t);

	size_++;

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}


template <typename T>
void Vector<T>::pop_back()
{
	assert(capacity_ >= size_);
	assert(size_ >= 1);

	// It's undefined to pop_back() on an empty vector.

	// Destroy object
	e[size_ - 1].~T();
	size_--;

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}


template <typename T>
const T& Vector<T>::back() const
{
	assert(capacity_ >= size_);
	assert(size_ >= 1);

	return e[size_ - 1];
}


template <typename T>
T& Vector<T>::back()
{
	assert(size_ >= 1);
	assert(capacity_ >= size_);

	return e[size_ - 1];
}


template <typename T>
void Vector<T>::copy(const T * const src, T* dst, size_t num)
{
	assert(num == 0 || src);
	assert(num == 0 || dst);

	// Disable a bogus VS 2010 Code analysis warning: 'warning C6011: Dereferencing NULL pointer 'src'
	#ifdef _WIN32
	#pragma warning(push)
	#pragma warning(disable:6011)
	#endif
	
	for(size_t i = 0; i < num; ++i)
		dst[i] = src[i];
	
	#ifdef _WIN32
	#pragma warning(pop)
	#endif
}


template <typename T>
T& Vector<T>::operator[](size_t index)
{
	assert(capacity_ >= size_);
	assert(index < size_);

	return e[index];
}


template <typename T>
const T& Vector<T>::operator[](size_t index) const
{
	assert(index < size_);
	assert(capacity_ >= size_);

	return e[index];
}


template <typename T>
typename Vector<T>::iterator Vector<T>::begin()
{
	return e;
}


template <typename T>
typename Vector<T>::iterator Vector<T>::end()
{
	return e + size_;
}


template <typename T>
typename Vector<T>::const_iterator Vector<T>::begin() const
{
	return e;
}


template <typename T>
typename Vector<T>::const_iterator Vector<T>::end() const
{
	return e + size_;
}


template <typename T>
void Vector<T>::erase(size_t index)
{
	const size_t curr_size = size();
	if(curr_size == 0)
		return;
	assert(index < curr_size);

	// Copy all elements one index down after the deletion index.
	// This is probably slow (not very nice for the cache).
	for(size_t i = index + 1; i < curr_size; ++i)
		e[i - 1] = e[i];

	resize(curr_size - 1);
}


} // end of namespace Indigo
