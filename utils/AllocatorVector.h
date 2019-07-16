/*=====================================================================
AllocatorVector.h
-----------------
Copyright Glare Technologies Limited 2018 -
=====================================================================*/
#pragma once


#include "GlareAllocator.h"
#include "Reference.h"
#include "../maths/SSE.h"
#include "../maths/mathstypes.h"
#include <assert.h>
#include <memory>
#include <new>


namespace glare
{


/*=====================================================================
AllocatorVector
---------------
Similar to js::Vector, but stores a reference to an allocator object.
=====================================================================*/
template <class T, size_t alignment>
class AllocatorVector
{
public:
	inline AllocatorVector(); // Initialise as an empty vector.
	explicit inline AllocatorVector(size_t count); // Initialise with default initialisation.
	inline AllocatorVector(size_t count, const T& val); // Initialise with count copies of val.
	inline AllocatorVector(const T* begin, const T* end); // Range constructor
	inline AllocatorVector(const AllocatorVector& other); // Initialise as a copy of other
	inline ~AllocatorVector();

	inline AllocatorVector& operator=(const AllocatorVector& other);

	inline void reserve(size_t N); // Make sure capacity is at least N.
	inline void reserveNoCopy(size_t N); // Make sure capacity is at least N.  Don't copy existing objects.
	inline void resize(size_t N); // Resize to size N, using default constructor if N > size().
	inline void resize(size_t N, const T& val); // Resize to size N, using copies of val if N > size().
	inline void resizeNoCopy(size_t N); // Resize to size N, but don't copy existing objects.
	inline size_t capacity() const { return capacity_; }
	inline size_t size() const;
	inline size_t dataSizeBytes() const; // Return the number of bytes needed to hold size elements, e.g. sizeof(T) * size().  Ignores excess capacity and Vector member overhead.
	inline size_t capacitySizeBytes() const; // Return the number of bytes needed to hold capacity elements, e.g. sizeof(T) * capacity().  Ignores Vector member overhead.
	inline bool empty() const;
	inline void clear(); // Resize to size zero, does not free mem.
	inline void clearAndFreeMem(); // Set size to zero, but also frees actual array memory.

	inline void clearAndSetCapacity(size_t N);

	inline void push_back(const T& t);
	inline void push_back_uninitialised();
	inline void pop_back();
	inline const T& back() const;
	inline T& back();
	inline T& operator[](size_t index);
	inline const T& operator[](size_t index) const;
	inline const T* data() const; // Returns e (a pointer to the contained data).  e may be NULL.
	inline T* data(); // Returns e (a pointer to the contained data).  e may be NULL.

	inline bool operator==(const AllocatorVector& other) const;
	inline bool operator!=(const AllocatorVector& other) const;

	typedef T* iterator;
	typedef const T* const_iterator;

	inline iterator begin();
	inline iterator end();
	inline const_iterator begin() const;
	inline const_iterator end() const;

	void erase(size_t index);

	void setAllocator(const Reference<glare::Allocator>& al) { allocator = al; }
	Reference<glare::Allocator>& getAllocator() { return allocator; }

private:
	T* alloc(size_t size);
	void free(T* ptr);

	T* e; // Elements
	size_t size_; // Number of elements in the vector.  Elements e[0] to e[size_-1] are proper constructed objects.
	size_t capacity_; // This is the number of elements we have roon for in e.
	Reference<glare::Allocator> allocator;
};


template <class T, size_t alignment>
AllocatorVector<T, alignment>::AllocatorVector()
:	e(0),
	size_(0),
	capacity_(0)
{
	static_assert(alignment % GLARE_ALIGNMENT(T) == 0, "alignment template argument insuffcient");
}


template <class T, size_t alignment>
AllocatorVector<T, alignment>::AllocatorVector(size_t count)
:	e(0),
	size_(count),
	capacity_(count)
{
	static_assert(alignment % GLARE_ALIGNMENT(T) == 0, "alignment template argument insuffcient");

	if(count > 0)
	{
		// Allocate new memory
		e = static_cast<T*>(alloc(sizeof(T) * count));

		// Initialise elements
		// NOTE: We use the constructor form without parentheses, in order to avoid default (zero) initialisation of POD types. 
		// See http://stackoverflow.com/questions/620137/do-the-parentheses-after-the-type-name-make-a-difference-with-new for more info.
		for(T* elem=e; elem<e + count; ++elem)
			::new (elem) T;
	}

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}


template <class T, size_t alignment>
AllocatorVector<T, alignment>::AllocatorVector(size_t count, const T& val)
:	e(0),
	size_(count),
	capacity_(count)
{
	static_assert(alignment % GLARE_ALIGNMENT(T) == 0, "alignment template argument insuffcient");

	if(count > 0)
	{
		// Allocate new memory
		e = static_cast<T*>(alloc(sizeof(T) * count));
		
		// Initialise elements as a copy of val.
		for(T* elem=e; elem<e + count; ++elem)
			::new (elem) T(val);
	}

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}


template <class T, size_t alignment>
AllocatorVector<T, alignment>::AllocatorVector(const T* begin_, const T* end_) // Range constructor
:	size_(end_ - begin_), capacity_(end_ - begin_)
{
	// Allocate new memory
	e = static_cast<T*>(alloc(sizeof(T) * size_));

	// Copy-construct new objects from existing objects in 'other'.
	std::uninitialized_copy(begin_, end_, 
		e // dest
	);
}


template <class T, size_t alignment>
AllocatorVector<T, alignment>::AllocatorVector(const AllocatorVector<T, alignment>& other)
{
	// Allocate new memory
	e = static_cast<T*>(alloc(sizeof(T) * other.size_));

	// Copy-construct new objects from existing objects in 'other'.
	std::uninitialized_copy(other.e, other.e + other.size_, 
		e // dest
	);
		
	size_ = other.size_;
	capacity_ = other.size_;
}


template <class T, size_t alignment>
AllocatorVector<T, alignment>::~AllocatorVector()
{
	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);

	// Destroy objects
	for(size_t i=0; i<size_; ++i)
		e[i].~T();

	free(e);
}


template <class T, size_t alignment>
AllocatorVector<T, alignment>& AllocatorVector<T, alignment>::operator=(const AllocatorVector& other)
{
	assert(capacity_ >= size_);

	if(this == &other)
		return *this;

	/*
	if we already have the capacity that we need, just copy objects over.

	otherwise free existing mem, alloc mem, copy elems over
	*/

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
		free(e);

		// Allocate new memory
		e = static_cast<T*>(alloc(sizeof(T) * other.size_));

		// Copy elements over from other
		if(e)
			std::uninitialized_copy(other.e, other.e + other.size_, e);

		size_ = other.size_;
		capacity_ = other.size_;
	}

	assert(size() == other.size());


	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);

	return *this;
}


template <class T, size_t alignment>
void AllocatorVector<T, alignment>::reserve(size_t n)
{
	assert(capacity_ >= size_);

	if(n > capacity_) // If need to expand capacity
	{
		// Allocate new memory
		T* new_e = static_cast<T*>(alloc(sizeof(T) * n));

		// Copy-construct new objects from existing objects.
		// e[0] to e[size_-1] will now be proper initialised objects.
		std::uninitialized_copy(e, e + size_, new_e);
		
		if(e)
		{
			// Destroy old objects
			for(size_t i=0; i<size_; ++i)
				e[i].~T();

			free(e); // Free old buffer.
		}
		e = new_e;
		capacity_ = n;
	}

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}


template <class T, size_t alignment>
void AllocatorVector<T, alignment>::reserveNoCopy(size_t n)
{
	assert(capacity_ >= size_);

	if(n > capacity_) // If need to expand capacity
	{
		// Allocate new memory
		T* new_e = static_cast<T*>(alloc(sizeof(T) * n));
		
		// Since we are not copying existing elements to new_e, just leave them uninitialised.

		// Destroy old objects
		for(size_t i=0; i<size_; ++i)
			e[i].~T();

		free(e); // Free old buffer.

		e = new_e;
		capacity_ = n;
	}

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}


template <class T, size_t alignment>
void AllocatorVector<T, alignment>::resize(size_t n, const T& val)
{
	assert(capacity_ >= size_);

	if(n > size_)
	{
		if(n > capacity_)
		{
			const size_t newcapacity = myMax(n, 2 * capacity_);
			reserve(newcapacity);
		}

		// Initialise elements e[size_] to e[n-1] as a copy of val.
		//std::uninitialized_fill(e + size_, e + n, val);
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


template <class T, size_t alignment>
void AllocatorVector<T, alignment>::resize(size_t n)
{
	assert(capacity_ >= size_);

	if(n > size_)
	{
		if(n > capacity_)
		{
			const size_t newcapacity = myMax(n, 2 * capacity_);
			reserve(newcapacity);
		}

		// Initialise elements e[size_] to e[n-1]
		// NOTE: We use the constructor form without parentheses, in order to avoid default (zero) initialisation of POD types. 
		// See http://stackoverflow.com/questions/620137/do-the-parentheses-after-the-type-name-make-a-difference-with-new for more info.
		for(T* elem=e + size_; elem<e + n; ++elem)
		{
			::new (elem) T;
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


template <class T, size_t alignment>
void AllocatorVector<T, alignment>::resizeNoCopy(size_t n)
{
	assert(capacity_ >= size_);

	if(n > capacity_)
	{
		const size_t newcapacity = myMax(n, 2 * capacity_);
		reserveNoCopy(newcapacity);
	}

	size_ = n;

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}


template <class T, size_t alignment>
size_t AllocatorVector<T, alignment>::size() const
{
	return size_;
}


template <class T, size_t alignment>
inline size_t AllocatorVector<T, alignment>::dataSizeBytes() const // Return the number of bytes needed to hold size elements, e.g. sizeof(T) * size().  Ignores excess capacity and AllocatorVector member overhead.
{
	return sizeof(T) * size();
}


template <class T, size_t alignment>
inline size_t AllocatorVector<T, alignment>::capacitySizeBytes() const
{
	return sizeof(T) * capacity();
}


template <class T, size_t alignment>
bool AllocatorVector<T, alignment>::empty() const
{
	return size_ == 0;
}


template <class T, size_t alignment>
void AllocatorVector<T, alignment>::clear()
{
	// Destroy objects
	for(size_t i=0; i<size_; ++i)
		e[i].~T();

	size_ = 0;
}


template <class T, size_t alignment>
void AllocatorVector<T, alignment>::clearAndFreeMem() // Set size to zero, but also frees actual array memory.
{
	// Destroy objects
	for(size_t i=0; i<size_; ++i)
		e[i].~T();

	free(e); // Free old buffer.
	e = NULL;
	size_ = 0;
	capacity_ = 0;
}


template <class T, size_t alignment>
inline void AllocatorVector<T, alignment>::clearAndSetCapacity(size_t N)
{
	clearAndFreeMem();
	reserve(N);
}


template <class T, size_t alignment>
void AllocatorVector<T, alignment>::push_back(const T& t)
{
	assert(capacity_ >= size_);

	// Check to see if we are out of capacity:
	if(size_ == capacity_)
	{
		const size_t newcapacity = myMax(size_ + 1, 2 * capacity_);
		reserve(newcapacity);
	}

	// Construct e[size_] from t
	::new ((e + size_)) T(t);

	size_++;

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}


template <class T, size_t alignment>
void AllocatorVector<T, alignment>::push_back_uninitialised()
{
	assert(capacity_ >= size_);

	// Check to see if we are out of capacity:
	if(size_ == capacity_)
	{
		const size_t newcapacity = myMax(size_ + 1, 2 * capacity_);
		reserve(newcapacity);
	}

	// Don't construct e[size_] !

	size_++;

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}


template <class T, size_t alignment>
void AllocatorVector<T, alignment>::pop_back()
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


template <class T, size_t alignment>
const T& AllocatorVector<T, alignment>::back() const
{
	assert(capacity_ >= size_);
	assert(size_ >= 1);

	return e[size_-1];
}


template <class T, size_t alignment>
T& AllocatorVector<T, alignment>::back()
{
	assert(size_ >= 1);
	assert(capacity_ >= size_);

	return e[size_-1];
}


template <class T, size_t alignment>
T& AllocatorVector<T, alignment>::operator[](size_t index)
{
	assert(capacity_ >= size_);
	assert(index < size_);

	return e[index];
}


template <class T, size_t alignment>
const T& AllocatorVector<T, alignment>::operator[](size_t index) const
{
	assert(index < size_);
	assert(capacity_ >= size_);

	return e[index];
}


template <class T, size_t alignment>
const T* AllocatorVector<T, alignment>::data() const
{
	return e;
}


template <class T, size_t alignment>
T* AllocatorVector<T, alignment>::data()
{
	return e;
}


template <class T, size_t alignment>
bool AllocatorVector<T, alignment>::operator==(const AllocatorVector<T, alignment>& other) const
{
	assert(capacity_ >= size_);

	if(size() != other.size())
		return false;

	for(size_t i=0; i<size_; ++i)
		if(e[i] != other[i])
			return false;

	return true;
}


template <class T, size_t alignment>
bool AllocatorVector<T, alignment>::operator!=(const AllocatorVector<T, alignment>& other) const
{
	assert(capacity_ >= size_);

	if(size() != other.size())
		return true;

	for(size_t i=0; i<size_; ++i)
		if(e[i] != other[i])
			return true;

	return false;
}


template <class T, size_t alignment>
typename AllocatorVector<T, alignment>::iterator AllocatorVector<T, alignment>::begin()
{
	return e;
}


template <class T, size_t alignment>
typename AllocatorVector<T, alignment>::iterator AllocatorVector<T, alignment>::end()
{
	return e + size_;
}


template <class T, size_t alignment>
typename AllocatorVector<T, alignment>::const_iterator AllocatorVector<T, alignment>::begin() const
{
	return e;
}


template <class T, size_t alignment>
typename AllocatorVector<T, alignment>::const_iterator AllocatorVector<T, alignment>::end() const
{
	return e + size_;
}


template <class T, size_t alignment>
void AllocatorVector<T, alignment>::erase(size_t index)
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


template <class T, size_t alignment>
T* AllocatorVector<T, alignment>::alloc(size_t size)
{
	if(allocator.nonNull())
		return (T*)allocator->alloc(size, alignment);
	else
		return (T*)SSE::alignedMalloc(size, alignment);
}


template <class T, size_t alignment>
void AllocatorVector<T, alignment>::free(T* ptr)
{
	if(allocator.nonNull())
		allocator->free(ptr);
	else
		SSE::alignedFree(ptr);
}


} // end namespace glare
