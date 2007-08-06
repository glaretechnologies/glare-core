/*=====================================================================
Vector.h
--------
File created by ClassTemplate on Sat Sep 02 19:47:39 2006
Code By Nicholas Chapman.
=====================================================================*/
#ifndef VECTOR_H_666
#define VECTOR_H_666


#include <assert.h>
#include "../indigo/globals.h"//TEMP
#include "stringutils.h"//TEMP
#include <malloc.h>

namespace js
{

//#define JS_VEC_ALIGNMENT 1


/*=====================================================================
Vector
------

=====================================================================*/
template <class T>
class Vector
{
public:
	inline Vector();
	inline ~Vector();

	inline Vector& operator=(const Vector& other);

	inline void reserve(unsigned int N); // make sure capacity is at least N
	inline void resize(unsigned int N);
	inline unsigned int capacity() const { return capacity_; }
	inline unsigned int size() const;
	inline bool empty() const;

	inline void push_back(const T& t);
	inline void pop_back();
	inline const T& back() const;
	inline T& back();
	inline T& operator[](unsigned int index);
	inline const T& operator[](unsigned int index) const;

	void setAlignment(unsigned int a);

private:
	Vector(const Vector& other);
	

	static inline void copy(T* src, T* dst, unsigned int num);

	T* e;
	unsigned int size_;
	unsigned int capacity_;
	unsigned int alignment_;
};


template <class T>
Vector<T>::Vector()
:	e(0),
	size_(0),
	capacity_(0),
	alignment_(4)
{
	//assert(alignment_in > 0);
}

template <class T>
Vector<T>::~Vector()
{
	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);

#ifdef JS_VEC_ALIGNMENT
	_aligned_free(e);
#else
	delete[] e;
#endif
}

template <class T>
Vector<T>& Vector<T>::operator=(const Vector& other)
{
	assert(capacity_ >= size_);
	
	if(this == &other)
		return *this;

	alignment_ = other.alignment_;

	resize(other.size());
	assert(size() == other.size());
	assert(capacity_ >= size_);
	copy(other.e, e, size_);

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);

	return *this;
}


template <class T>
void Vector<T>::reserve(unsigned int n)
{
	assert(capacity_ >= size_);

	if(n > capacity_) //if need to expand capacity
	{
		//conPrint("Vector<T>::reserve: allocing " + toString(n) + " items (" + toString(n*sizeof(T)) + " bytes)");//TEMP
		//T* new_e = new(_aligned_malloc(sizeof(T) * n, alignment_)) T[n];
		//NOTE: bother constructing these objects?
#ifdef JS_VEC_ALIGNMENT
		T* new_e = static_cast<T*>(_aligned_malloc(sizeof(T) * n, alignment_));
#else
		T* new_e = new T[n];
#endif
		
		if(e)
		{
			copy(e, new_e, size_); //copy existing data to new buffer

			//conPrint("Vector<T>::reserve: freeing " + toString(capacity_) + " items (" + toString(capacity_*sizeof(T)) + " bytes)");//TEMP
#ifdef JS_VEC_ALIGNMENT
			_aligned_free(e); //free old buffer
#else
			delete[] e;
#endif
		}
		e = new_e;
		capacity_ = n;
	}

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}

template <class T>
void Vector<T>::resize(unsigned int n)
{
	assert(capacity_ >= size_);

	reserve(n);
	size_ = n;

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}

template <class T>
unsigned int Vector<T>::size() const
{
	return size_;
}

template <class T>
bool Vector<T>::empty() const
{
	return size_ == 0;
}

template <class T>
void Vector<T>::push_back(const T& t)
{
	assert(capacity_ >= size_);

	if(size_ >= capacity_)//if next write would exceed capacity
	{
		const unsigned int ideal_newcapacity = size_ + size_ / 2; // current size * 1.5
		const unsigned int newcapacity = ideal_newcapacity > (size_ + 1) ? ideal_newcapacity : (size_ + 1); //(size_ + 1) * 2;
		assert(newcapacity >= size_ + 1);
		reserve(newcapacity);
	}

	e[size_++] = t;

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}

template <class T>
void Vector<T>::pop_back()
{
	assert(capacity_ >= size_);
	assert(size_ >= 1);
	
	if(size_ >= 1)
		size_--;

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}

template <class T>
const T& Vector<T>::back() const
{
	assert(capacity_ >= size_);
	assert(size_ >= 1);

	return e[size_-1];

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}

template <class T>
T& Vector<T>::back()
{
	assert(size_ >= 1);
	assert(capacity_ >= size_);

	return e[size_-1];

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}

template <class T>
void Vector<T>::copy(T* src, T* dst, unsigned int num)
{
	assert(src);
	assert(dst);

	for(unsigned int i=0; i<num; ++i)
		dst[i] = src[i];
}

template <class T>
T& Vector<T>::operator[](unsigned int index)
{
	assert(capacity_ >= size_);
	assert(index < size_);

	return e[index];

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}

template <class T>
const T& Vector<T>::operator[](unsigned int index) const
{
	assert(index < size_);
	assert(capacity_ >= size_);

	return e[index];

	assert(capacity_ >= size_);
	assert(size_ > 0 ? (e != NULL) : true);
}

template <class T>
void Vector<T>::setAlignment(unsigned int a)
{
	alignment_ = a;
}


} //end namespace js


#endif //VECTOR_H_666



















