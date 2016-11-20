/*=====================================================================
ArrayRef.cpp
-------------------
Copyright Glare Technologies Limited 2016 -
=====================================================================*/
#pragma once


#include "SmallArray.h"
#include "SmallVector.h"
#include "Vector.h"
#include <vector>
#include <assert.h>


/*======================================================================================
ArrayRef
--------
A (pointer, size) pair.
Offers a read-only view on a contiguous array of values.
Idea based on LLVM's ArrayRef.
======================================================================================*/
template <class T>
class ArrayRef
{
public:
	inline ArrayRef(const T* data, size_t len);

	template <int N>
	inline ArrayRef(const SmallVector<T, N>& v) : data_(v.data()), len(v.size()) {}

	template <int N>
	inline ArrayRef(const SmallArray<T, N>& a)  : data_(a.data()), len(a.size()) {}

	inline ArrayRef(const std::vector<T>& v)    : data_(v.data()), len(v.size()) {}

	template <size_t A>
	inline ArrayRef(const js::Vector<T, A>& a)  : data_(a.data()), len(a.size()) {}

	inline T& operator[] (size_t index);
	inline const T& operator[] (size_t index) const;

	inline size_t size() const { return len; }
	inline T* data() { return data_; }

	inline const T& back() const { assert(len >= 1); return data_[len - 1]; }

	//-----------------------------------------------------------------
	// iterator stuff
	//-----------------------------------------------------------------
	typedef T* iterator;
	typedef const T* const_iterator;

	inline iterator begin() { return data_; }
	inline iterator end() { return data_ + len; }

	inline const_iterator begin() const { return data_; }
	inline const_iterator end() const { return data_ + len; }

private:
	const T* data_;
	size_t len;
};


template <class T>
ArrayRef<T>::ArrayRef(const T* newdata, size_t len_)
:	data_(newdata), len(len_)
{}


template <class T>
T& ArrayRef<T>::operator[] (size_t index)
{
	assert(index < len);
	return data_[index];
}


template <class T>
const T& ArrayRef<T>::operator[] (size_t index) const
{
	assert(index < len);
	return data_[index];
}
