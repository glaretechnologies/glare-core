/*=====================================================================
Vector.h
--------
File created by ClassTemplate on Sat Sep 02 19:47:39 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef VECTOR_H_666
#define VECTOR_H_666


#include "../maths/SSE.h"
#include "../maths/mathstypes.h"
#include <assert.h>


// If this is defined, prints out messages on allocing and deallocing in reserve()
//#define JS_VECTOR_VERBOSE 1


#if JS_VECTOR_VERBOSE
#include "../indigo/globals.h"
#include "stringutils.h"
#include <typeinfo.h>
#endif


namespace js
{


/*=====================================================================
Vector
------
Similar to std::vector, but aligns the elements.
=====================================================================*/
template <class T, int alignment>
class Vector
{
public:
	inline Vector();
	inline Vector(unsigned int count);
	inline Vector(unsigned int count, const T& val);
	inline Vector(const Vector& other);
	inline ~Vector();

	inline Vector& operator=(const Vector& other);

	inline void reserve(unsigned int N); // Make sure capacity is at least N.
	inline void resize(unsigned int N);
	inline unsigned int capacity() const { return capacity_; }
	inline unsigned int size() const;
	inline bool empty() const;
	inline void clearAndFreeMem(); // Set size to zero, but also frees actual array memory.

	inline void push_back(const T& t);
	inline void pop_back();
	inline const T& back() const;
	inline T& back();
	inline T& operator[](unsigned int index);
	inline const T& operator[](unsigned int index) const;

	typedef T* iterator;
	typedef const T* const_iterator;

	inline iterator begin();
	inline iterator end();
	inline const_iterator begin() const;
	inline const_iterator end() const;

private:
	
	
	static inline void copy(const T* const src, T* dst, unsigned int num);

	T* e;
	unsigned int size_;
	unsigned int capacity_;
};


template <class T, int alignment>
Vector<T, alignment>::Vector()
:	e(0),
	size_(0),
	capacity_(0)
{
	assert(alignment > sizeof(T) || sizeof(T) % alignment == 0); // sizeof(T) needs to be a multiple of alignment, otherwise e[1] will be unaligned.
}


template <class T, int alignment>
Vector<T, alignment>::Vector(unsigned int count)
:	e(0),
	size_(0),
	capacity_(0)
{
	assert(alignment > sizeof(T) || sizeof(T) % alignment == 0); // sizeof(T) needs to be a multiple of alignment, otherwise e[1] will be unaligned.

	resize(count);
	// TODO: construct elements?
}


template <class T, int alignment>
Vector<T, alignment>::Vector(unsigned int count, const T& val)
:	e(0),
	size_(0),
	capacity_(0)
{
	assert(alignment > sizeof(T) || sizeof(T) % alignment == 0); // sizeof(T) needs to be a multiple of alignment, otherwise e[1] will be unaligned.

	resize(count);
	for(unsigned int i=0; i<count; ++i)
		e[i] = val;
}


template <class T, int alignment>
Vector<T, alignment>::Vector(const Vector<T, alignment>& other)
:	e(0),
	size_(0),
	capacity_(0)
{
	*this = other;
}


template <class T, int alignment>
Vector<T, alignment>::~Vector()
{
	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);

	SSE::alignedFree(e);
}


template <class T, int alignment>
Vector<T, alignment>& Vector<T, alignment>::operator=(const Vector& other)
{
	assert(capacity_ >= size_);
	
	if(this == &other)
		return *this;

	resize(other.size());
	assert(size() == other.size());
	assert(capacity_ >= size_);
	copy(other.e, e, size_);

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);

	return *this;
}


template <class T, int alignment>
void Vector<T, alignment>::reserve(unsigned int n)
{
	assert(capacity_ >= size_);

	if(n > capacity_) // If need to expand capacity
	{
		#if JS_VECTOR_VERBOSE
		conPrint("Vector<" + std::string(typeid(T).name()) + ", " + toString(alignment) + ">::reserve: allocing " + toString(n) + " items (" + toString(n*sizeof(T)) + " bytes)");
		#endif
		
		// NOTE: bother constructing these objects?
		T* new_e = static_cast<T*>(SSE::alignedMalloc(sizeof(T) * n, alignment));
		
		if(e)
		{
			copy(e, new_e, size_); // Copy existing data to new buffer

			#if JS_VECTOR_VERBOSE
			conPrint("Vector<" + std::string(typeid(T).name()) + ", " + toString(alignment) + ">::reserve: freeing " + toString(capacity_) + " items (" + toString(capacity_*sizeof(T)) + " bytes)");
			#endif

			SSE::alignedFree(e); // Free old buffer.
		}
		e = new_e;
		capacity_ = n;
	}

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}


template <class T, int alignment>
void Vector<T, alignment>::resize(unsigned int n)
{
	assert(capacity_ >= size_);

	//reserve(n);
	if(n > capacity_)
	{
		const unsigned int newcapacity = myMax(n, capacity_ + capacity_ / 2); // current size * 1.5
		//const unsigned int newcapacity = ideal_newcapacity > (size_ + 1) ? ideal_newcapacity : (size_ + 1); //(size_ + 1) * 2;
		//assert(newcapacity >= size_ + 1);
		reserve(newcapacity);
	}

	size_ = n;

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}


template <class T, int alignment>
unsigned int Vector<T, alignment>::size() const
{
	return size_;
}


template <class T, int alignment>
bool Vector<T, alignment>::empty() const
{
	return size_ == 0;
}


template <class T, int alignment>
void Vector<T, alignment>::clearAndFreeMem() // Set size to zero, but also frees actual array memory.
{
	SSE::alignedFree(e); // Free old buffer.
	e = NULL;
	size_ = 0;
	capacity_ = 0;
}


template <class T, int alignment>
void Vector<T, alignment>::push_back(const T& t)
{
	assert(capacity_ >= size_);

	/*if(size_ >= capacity_) // If next write would exceed capacity
	{
		const unsigned int ideal_newcapacity = size_ + size_ / 2; // current size * 1.5
		const unsigned int newcapacity = ideal_newcapacity > (size_ + 1) ? ideal_newcapacity : (size_ + 1); //(size_ + 1) * 2;
		assert(newcapacity >= size_ + 1);
		reserve(newcapacity);
	}*/
	const unsigned int old_size = size_;
	resize(size_ + 1);

	e[old_size] = t;

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}


template <class T, int alignment>
void Vector<T, alignment>::pop_back()
{
	assert(capacity_ >= size_);
	assert(size_ >= 1);
	
	if(size_ >= 1)
		size_--;

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}


template <class T, int alignment>
const T& Vector<T, alignment>::back() const
{
	assert(capacity_ >= size_);
	assert(size_ >= 1);

	return e[size_-1];
}


template <class T, int alignment>
T& Vector<T, alignment>::back()
{
	assert(size_ >= 1);
	assert(capacity_ >= size_);

	return e[size_-1];
}


template <class T, int alignment>
void Vector<T, alignment>::copy(const T * const src, T* dst, unsigned int num)
{
	assert(num == 0 || src);
	assert(num == 0 || dst);

	for(unsigned int i=0; i<num; ++i)
		dst[i] = src[i];
}


template <class T, int alignment>
T& Vector<T, alignment>::operator[](unsigned int index)
{
	assert(capacity_ >= size_);
	assert(index < size_);

	return e[index];
}


template <class T, int alignment>
const T& Vector<T, alignment>::operator[](unsigned int index) const
{
	assert(index < size_);
	assert(capacity_ >= size_);

	return e[index];
}


template <class T, int alignment>
typename Vector<T, alignment>::iterator Vector<T, alignment>::begin()
{
	return e;
}


template <class T, int alignment>
typename Vector<T, alignment>::iterator Vector<T, alignment>::end()
{
	return e + size_;
}


template <class T, int alignment>
typename Vector<T, alignment>::const_iterator Vector<T, alignment>::begin() const
{
	return e;
}


template <class T, int alignment>
typename Vector<T, alignment>::const_iterator Vector<T, alignment>::end() const
{
	return e + size_;
}


} //end namespace js


#endif //VECTOR_H_666
