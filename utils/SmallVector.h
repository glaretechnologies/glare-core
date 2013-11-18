/*=====================================================================
SmallVector.h
-------------------
Copyright Glare Technologies Limited 2013 -
Generated at 2013-03-07 14:27:12 +0000
=====================================================================*/
#pragma once


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
	inline void resize(size_t new_size, const T& val = T()); // Resize to size new_size, using copies of val if new_size > size().
	inline size_t capacity() const;
	inline size_t size() const;
	inline bool empty() const;

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
	inline void invariant() const;
	inline void destroyExistingDirectObjects();
	inline void destroyExistingHeapObjectsAndFreeMem();

	struct SmallVectorHeap
	{
		T* e; // Elements
		size_t capacity_; // This is the number of elements we have roon for in e.
	};

	struct SmallVectorDirect
	{
		//T e[N];
		unsigned char e[sizeof(T) * N];
	};

	union SmallVectorUnion
	{
		SmallVectorHeap heap;
		SmallVectorDirect direct;
	};

	SmallVectorUnion data;

	size_t size_; // Number of elements in the vector.  Elements e[0] to e[size_-1] are proper constructed objects.
};


void SmallVectorTest();


template <class T, int N>
SmallVector<T, N>::SmallVector()
:	size_(0)
{
	invariant();
}



template <class T, int N>
SmallVector<T, N>::SmallVector(size_t count, const T& val)
:	size_(0)
{
	invariant();
	resize(count, val);
	invariant();
}


template <class T, int N>
SmallVector<T, N>::SmallVector(const SmallVector<T, N>& other)
{
	if(other.size_ <= N)
	{
		// Copy-construct new objects from existing objects in 'other'.
		std::uninitialized_copy(reinterpret_cast<const T*>(other.data.direct.e), reinterpret_cast<const T*>(other.data.direct.e) + other.size_, 
			reinterpret_cast<T*>(data.direct.e) // dest
		);

		size_ = other.size_;
	}
	else
	{
		// Allocate new memory
		data.heap.e = static_cast<T*>(SSE::alignedSSEMalloc(sizeof(T) * other.size_));

		// Copy-construct new objects from existing objects in 'other'.
		std::uninitialized_copy(other.data.heap.e, other.data.heap.e + other.size_, 
			data.heap.e // dest
		);
		
		size_ = other.size_;
		data.heap.capacity_ = other.size_;
	}

	invariant();
}


template <class T, int N>
SmallVector<T, N>::~SmallVector()
{
	invariant();

	// Destroy objects
	if(size_ <= N)
		destroyExistingDirectObjects();
	else
		destroyExistingHeapObjectsAndFreeMem();
}


template <class T, int N>
SmallVector<T, N>& SmallVector<T, N>::operator=(const SmallVector& other)
{
	invariant();

	if(this == &other)
		return *this;

	if(other.size_ <= N)
	{
		if(size_ <= N)
		{
			// If current vector is direct storing:
			destroyExistingDirectObjects();
		}
		else
		{
			// Else if current vector is heap storing:
			destroyExistingHeapObjectsAndFreeMem();
		}

		// Copy elements over from other
		if(other.size_ > 0) 
			std::uninitialized_copy(reinterpret_cast<const T*>(other.data.direct.e), reinterpret_cast<const T*>(other.data.direct.e) + other.size_, 
				reinterpret_cast<T*>(data.direct.e)
			);

		size_ = other.size_;
	}
	else
	{
		// Else we will need to store new elements on the heap:

		if(size_ <= N) // If current vector is direct storing:
		{
			destroyExistingDirectObjects();

			// Allocate new memory
			data.heap.e = static_cast<T*>(SSE::alignedSSEMalloc(sizeof(T) * other.size_));

			// Copy elements over from other
			std::uninitialized_copy(other.data.heap.e, other.data.heap.e + other.size_, data.heap.e);

			size_ = other.size_;
			data.heap.capacity_ = other.size_;
		}
		else
		{
			/*
			Else if current vector is already heap storing:
			
			if we already have the capacity that we need, just copy objects over.

			otherwise free existing mem, alloc mem, copy elems over
			*/

			if(data.heap.capacity_ >= other.size_)
			{
				// Destroy existing objects
				for(size_t i=0; i<size_; ++i)
					data.heap.e[i].~T();

				// Copy elements over from other
				//if(e) 
					std::uninitialized_copy(other.data.heap.e, other.data.heap.e + other.size_, data.heap.e);

				size_ = other.size_;
			}
			else
			{
				// Destroy existing objects
				for(size_t i=0; i<size_; ++i)
					data.heap.e[i].~T();

				// Free existing mem
				SSE::alignedFree(data.heap.e);

				// Allocate new memory
				data.heap.e = static_cast<T*>(SSE::alignedSSEMalloc(sizeof(T) * other.size_));

				// Copy elements over from other
				std::uninitialized_copy(other.data.heap.e, other.data.heap.e + other.size_, data.heap.e);

				size_ = other.size_;
				data.heap.capacity_ = other.size_;
			}
		}
	}

	assert(size() == other.size());


	invariant();

	return *this;
}


template <class T, int N>
void SmallVector<T, N>::reserve(size_t n)
{
	invariant();

	// Capacity is always >= N.

	if(size_ <= N)
	{
		// We can't expand capacity when storing directly, capacity is always N.
	}
	else // Else if storing on heap:
	{
		if(n > data.heap.capacity_) // If need to expand capacity
		{
			// Allocate new memory
			T* new_e = static_cast<T*>(SSE::alignedSSEMalloc(sizeof(T) * n));

			// Copy-construct new objects from existing objects.
			// e[0] to e[size_-1] will now be proper initialised objects.
			std::uninitialized_copy(data.heap.e, data.heap.e + size_, new_e);
		
			if(data.heap.e)
			{
				// Destroy old objects
				for(size_t i=0; i<size_; ++i)
					data.heap.e[i].~T();

				SSE::alignedFree(data.heap.e); // Free old buffer.
			}

			data.heap.e = new_e;
			data.heap.capacity_ = n;
		}
	}

	invariant();
}


template <class T, int N>
void SmallVector<T, N>::resize(size_t new_size, const T& val)
{
	invariant();

	if(new_size > size_) // If we are increasing the size:
	{
		if(new_size <= N) // If the new size still fits in direct storage:
		{
			// Initialise elements e[size_] to e[new_size-1] as a copy of val.
			std::uninitialized_fill(reinterpret_cast<T*>(data.direct.e) + size_, reinterpret_cast<T*>(data.direct.e) + new_size, val);
		}
		else
		{
			// Else elements need to go on heap now

			if(size_ <= N)
			{
				// If elements are current direct stored:

				// Allocate new memory
				const size_t newcapacity = new_size;
				T* new_e = static_cast<T*>(SSE::alignedSSEMalloc(sizeof(T) * newcapacity));

				// Copy existing elements over to new memory
				std::uninitialized_copy(reinterpret_cast<const T*>(data.direct.e), reinterpret_cast<const T*>(data.direct.e) + size_, 
					new_e);

				// Initialise elements e[size_] to e[new_size-1] as a copy of val.
				std::uninitialized_fill(new_e + size_, new_e + new_size, val);

				// Destroy old direct-stored elements
				for(size_t i=0; i<size_; ++i)
					(reinterpret_cast<T*>(data.direct.e) + i)->~T();

				// Switch to storing on heap
				data.heap.e = new_e;
				data.heap.capacity_ = newcapacity;
			}
			else
			{
				// Else if elements are current heap stored:

				if(new_size > data.heap.capacity_)
				{
					const size_t newcapacity = myMax(new_size, data.heap.capacity_ + data.heap.capacity_ / 2); // current size * 1.5
					reserve(newcapacity);
				}

				// Initialise elements e[size_] to e[n-1] as a copy of val.
				std::uninitialized_fill(data.heap.e + size_, data.heap.e + new_size, val);
			}
		}

	}
	else if(new_size < size_) // Else if we are decreasing the size:
	{
		if(size_ <= N) // If we are storing directly
		{
			// Destroy elements e[new_size] to e[size-1]
			for(size_t i=new_size; i<size_; ++i)
				(reinterpret_cast<T*>(data.direct.e) + i)->~T();
		}
		else // Else if we are currently storing on heap
		{
			if(new_size > N)
			{
				// We keep using the heap, and keep the same mem, just destroy some elems
				for(size_t i=new_size; i<size_; ++i)
					data.heap.e[i].~T();
			}
			else
			{
				// We need to change from using the heap to storing directly.

				// NOTE: This way is slow, we should move elements that we are keeping directly instead of constructing new ones and destroying old ones.

				// Copy elements we are keeping (e[0] to e[new_size-1] from the heap to direct storage
				T* heap_elems = data.heap.e;
				std::uninitialized_copy(heap_elems, heap_elems + new_size, 
					reinterpret_cast<T*>(data.direct.e)
				);

				// Destroy elements e[0] to e[size_]
				for(size_t i=0; i<size_; ++i)
					heap_elems[i].~T();

				// Free heap memory
				SSE::alignedFree(heap_elems);
			}
		}
			
	}

	size_ = new_size;

	invariant();
}


template <class T, int N>
size_t SmallVector<T, N>::capacity() const
{
	invariant();

	if(size_ <= N)
		return N;
	else
		return data.heap.capacity_;
}


template <class T, int N>
size_t SmallVector<T, N>::size() const
{
	invariant();
	return size_;
}


template <class T, int N>
bool SmallVector<T, N>::empty() const
{
	invariant();
	return size_ == 0;
}


template <class T, int N>
void SmallVector<T, N>::push_back(const T& t)
{
	invariant();

	if(size_ <= N) // if we are storing directly
	{
		if(size_ + 1 <= N) // If we have direct storage room for the new element
		{
			// Construct e[size_] from t
			::new (reinterpret_cast<T*>(data.direct.e) + size_) T(t);
			size_++;
		}
		else
		{
			resize(size_ + 1, t);
		}
	}
	else
	{
		if(size_ + 1 > data.heap.capacity_)
		{
			const size_t newcapacity = myMax(size_ + 1, data.heap.capacity_ + data.heap.capacity_ / 2); // current size * 1.5
			reserve(newcapacity);
		}

		// Construct e[size_] from t
		::new (data.heap.e + size_) T(t);
		size_++;
	}
	

	invariant();
}


template <class T, int N>
void SmallVector<T, N>::pop_back()
{
	invariant();

	if(size_ <= N) // If we are storing directly
	{
		// Destroy element e[size_ - 1]
		(reinterpret_cast<T*>(data.direct.e) + size_ - 1)->~T();
	}
	else // Else if we are currently storing on heap
	{
		if(size_ - 1 > N) // if size > N + 1
		{
			// We keep using the heap, and keep the same mem, just destroy element e[size_ - 1]
			data.heap.e[size_ - 1].~T();
		}
		else // else size_ == N + 1
		{
			assert(size_ == N + 1);

			// We need to change from using the heap to storing directly.

			// NOTE: This way is slow, we should move elements that we are keeping directly instead of constructing new ones and destroying old ones.

			// Copy elements we are keeping (e[0] to e[N]) from the heap to direct storage
			T* heap_elems = data.heap.e;
			std::uninitialized_copy(heap_elems, heap_elems + N, 
				reinterpret_cast<T*>(data.direct.e)
			);

			// Destroy elements e[0] to e[size_]
			for(size_t i=0; i<size_; ++i)
				heap_elems[i].~T();

			// Free heap memory
			SSE::alignedFree(heap_elems);
		}
	}

	size_--;

	invariant();
}


template <class T, int N>
const T& SmallVector<T, N>::back() const
{
	invariant();
	assert(size_ >= 1);

	if(size_ <= N)
		return *(reinterpret_cast<const T*>(data.direct.e) + (size_ - 1));
	else
		return data.heap.e[size_ - 1];
}


template <class T, int N>
T& SmallVector<T, N>::back()
{
	invariant();
	assert(size_ >= 1);

	if(size_ <= N)
		return *(reinterpret_cast<T*>(data.direct.e) + (size_ - 1));
	else
		return data.heap.e[size_ - 1];
}


template <class T, int N>
T& SmallVector<T, N>::operator[](size_t index)
{
	invariant();
	assert(index < size_);

	if(size_ <= N)
		return *(reinterpret_cast<T*>(data.direct.e) + index);
	else
		return data.heap.e[index];
}


template <class T, int N>
const T& SmallVector<T, N>::operator[](size_t index) const
{
	invariant();
	assert(index < size_);

	if(size_ <= N)
		return *(reinterpret_cast<const T*>(data.direct.e) + index);
	else
		return data.heap.e[index];
}


template <class T, int N>
void SmallVector<T, N>::invariant() const
{
#ifndef NDEBUG
	if(size_ < N)
	{
		// Elements are directly stored in class.
		// Nothing to check
	}
	else
	{
		assert(data.heap.capacity_ >= size_);

		if(size_ > 0)
		{
			assert(data.heap.e != NULL);
		}
		else
		{
			assert(data.heap.e == NULL);
		}
	}

#endif

}


template <class T, int N>
void SmallVector<T, N>::destroyExistingDirectObjects()
{
	assert(size_ <= N);
	for(size_t i=0; i<size_; ++i)
		(reinterpret_cast<const T*>(data.direct.e) + i)->~T();
}


template <class T, int N>
void SmallVector<T, N>::destroyExistingHeapObjectsAndFreeMem()
{
	assert(size_ > N);
	
	for(size_t i=0; i<size_; ++i)
		data.heap.e[i].~T();

	SSE::alignedFree(data.heap.e);
}


template <class T, int N>
typename SmallVector<T, N>::iterator SmallVector<T, N>::begin()
{
	if(size_ <= N)
		return reinterpret_cast<T*>(data.direct.e);
	else
		return data.heap.e;
}


template <class T, int N>
typename SmallVector<T, N>::iterator SmallVector<T, N>::end()
{
	if(size_ <= N)
		return reinterpret_cast<T*>(data.direct.e) + size_;
	else
		return data.heap.e + size_;
}


template <class T, int N>
typename SmallVector<T, N>::const_iterator SmallVector<T, N>::begin() const
{
	if(size_ <= N)
		return reinterpret_cast<const T*>(data.direct.e);
	else
		return data.heap.e;
}


template <class T, int N>
typename SmallVector<T, N>::const_iterator SmallVector<T, N>::end() const
{
	if(size_ <= N)
		return reinterpret_cast<const T*>(data.direct.e) + size_;
	else
		return data.heap.e + size_;
}